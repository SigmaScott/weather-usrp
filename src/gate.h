/*
 * gate.h - Per-channel audio gate state machine
 *
 * States:
 *   IDLE:        Audio discarded. EAS decoder still active.
 *   ALERT:       SAME match detected. Audio forwarded to USRP with keyup=1.
 *                Transitions back to IDLE on EOM or timeout.
 *   PASSTHROUGH: Manual override. All audio forwarded to USRP with keyup=1.
 *                Entered/exited via TCP control command.
 */

#ifndef GATE_H
#define GATE_H

#include "common.h"
#include "same.h"
#include "usrp.h"

/* Gate event callback (for TCP event push) */
typedef void (*gate_event_cb_t)(int channel, const char *event_json, void *userdata);

/* Gate state per channel */
typedef struct {
    gate_state_t    state;
    usrp_conn_t     usrp;
    int             channel;

    /* Alert state */
    same_message_t  current_alert;
    uint64_t        alert_start_ms;     /* timestamp when alert started */
    uint64_t        alert_timeout_ms;   /* auto-return to IDLE after this */

    /* Audio buffer for USRP framing (accumulate 160 samples) */
    int16_t         frame_buf[USRP_SAMPLES];
    int             frame_pos;
    unsigned long   frames_sent;

    /* Event callback */
    gate_event_cb_t event_cb;
    void            *event_userdata;

    /* PTT toggle for passthrough testing */
    uint64_t        ptt_toggle_ms;
    int             ptt_state;

    /* Config reference */
    const channel_config_t *config;
} gate_t;

/*
 * Initialize gate for a channel.
 */
int gate_init(gate_t *gate, int channel, const channel_config_t *cfg,
              gate_event_cb_t event_cb, void *event_userdata);

/*
 * Process decoded audio through gate.
 * Audio is 8kHz int16 (already decimated).
 * Gate decides whether to forward to USRP based on state.
 */
void gate_process_audio(gate_t *gate, const int16_t *samples, int count);

/*
 * Notify gate of a SAME alert (already FIPS-matched).
 * Transitions IDLE -> ALERT.
 */
void gate_alert(gate_t *gate, const same_message_t *msg);

/*
 * Notify gate of End Of Message.
 * Transitions ALERT -> IDLE.
 */
void gate_eom(gate_t *gate);

/*
 * Set passthrough mode.
 * on=1: IDLE/ALERT -> PASSTHROUGH
 * on=0: PASSTHROUGH -> IDLE
 */
void gate_set_passthrough(gate_t *gate, int on);

/*
 * Get current gate state.
 */
gate_state_t gate_get_state(const gate_t *gate);

/*
 * Periodic tick (call every ~100ms). Handles alert timeout.
 */
void gate_tick(gate_t *gate, uint64_t now_ms);

/*
 * Cleanup gate resources.
 */
void gate_close(gate_t *gate);

#endif /* GATE_H */
