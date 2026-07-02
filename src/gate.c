#include "gate.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int gate_init(gate_t *gate, int channel, const channel_config_t *cfg,
              gate_event_cb_t event_cb, void *event_userdata)
{
    memset(gate, 0, sizeof(*gate));
    gate->state = GATE_IDLE;
    gate->channel = channel;
    gate->config = cfg;
    gate->event_cb = event_cb;
    gate->event_userdata = event_userdata;
    gate->alert_timeout_ms = 600000;

    if (usrp_init(&gate->usrp, cfg->usrp_host, cfg->usrp_port) < 0) {
        LOG_ERROR("gate", "ch%d failed to init USRP to %s:%u",
                  channel, cfg->usrp_host, cfg->usrp_port);
        return -1;
    }

    LOG_DEBUG("gate", "ch%d init: USRP -> %s:%u, timeout=%lums",
              channel, cfg->usrp_host, cfg->usrp_port,
              (unsigned long)gate->alert_timeout_ms);
    return 0;
}

void gate_process_audio(gate_t *gate, const int16_t *samples, int count)
{
    if (gate->state == GATE_IDLE)
        return;

    for (int i = 0; i < count; i++) {
        gate->frame_buf[gate->frame_pos++] = samples[i];
        if (gate->frame_pos >= USRP_SAMPLES) {
            usrp_send_audio(&gate->usrp, gate->frame_buf, 1);
            gate->frames_sent++;
            gate->frame_pos = 0;
        }
    }
}

void gate_alert(gate_t *gate, const same_message_t *msg)
{
    if (gate->state == GATE_PASSTHROUGH) {
        LOG_DEBUG("gate", "ch%d alert ignored (in PASSTHROUGH)", gate->channel);
        return;
    }

    LOG_INFO("gate", "ch%d IDLE -> ALERT (event=%s)", gate->channel, msg->event);
    gate->state = GATE_ALERT;
    gate->current_alert = *msg;
    gate->alert_start_ms = now_ms();
    gate->frame_pos = 0;
    gate->frames_sent = 0;

    if (gate->event_cb) {
        char json[1024];
        same_format_json(msg, json, sizeof(json));
        char event[1100];
        snprintf(event, sizeof(event), "ALERT %d %s", gate->channel, json);
        gate->event_cb(gate->channel, event, gate->event_userdata);
    }
}

void gate_eom(gate_t *gate)
{
    if (gate->state != GATE_ALERT) {
        LOG_DEBUG("gate", "ch%d EOM ignored (state=%d, not ALERT)", gate->channel, gate->state);
        return;
    }

    LOG_INFO("gate", "ch%d ALERT -> IDLE (EOM, %lu frames sent)",
             gate->channel, gate->frames_sent);

    if (gate->frame_pos > 0) {
        memset(gate->frame_buf + gate->frame_pos, 0,
               (USRP_SAMPLES - gate->frame_pos) * sizeof(int16_t));
        usrp_send_audio(&gate->usrp, gate->frame_buf, 1);
        gate->frame_pos = 0;
    }

    int16_t silence[USRP_SAMPLES] = {0};
    usrp_send_audio(&gate->usrp, silence, 0);

    gate->state = GATE_IDLE;

    if (gate->event_cb) {
        char event[64];
        snprintf(event, sizeof(event), "EOM %d", gate->channel);
        gate->event_cb(gate->channel, event, gate->event_userdata);
    }
}

void gate_set_passthrough(gate_t *gate, int on)
{
    if (on) {
        LOG_INFO("gate", "ch%d %s -> PASSTHROUGH",
                 gate->channel,
                 gate->state == GATE_IDLE ? "IDLE" : "ALERT");
        gate->state = GATE_PASSTHROUGH;
        gate->frame_pos = 0;
        gate->frames_sent = 0;
    } else {
        if (gate->state == GATE_PASSTHROUGH) {
            LOG_INFO("gate", "ch%d PASSTHROUGH -> IDLE (%lu frames sent)",
                     gate->channel, gate->frames_sent);
            if (gate->frame_pos > 0) {
                memset(gate->frame_buf + gate->frame_pos, 0,
                       (USRP_SAMPLES - gate->frame_pos) * sizeof(int16_t));
                usrp_send_audio(&gate->usrp, gate->frame_buf, 1);
                gate->frame_pos = 0;
            }
            int16_t silence[USRP_SAMPLES] = {0};
            usrp_send_audio(&gate->usrp, silence, 0);
            gate->state = GATE_IDLE;
        }
    }

    if (gate->event_cb) {
        char event[64];
        snprintf(event, sizeof(event), "STATE %d %s",
                 gate->channel,
                 gate->state == GATE_IDLE ? "IDLE" :
                 gate->state == GATE_ALERT ? "ALERT" : "PASSTHROUGH");
        gate->event_cb(gate->channel, event, gate->event_userdata);
    }
}

gate_state_t gate_get_state(const gate_t *gate)
{
    return gate->state;
}

void gate_tick(gate_t *gate, uint64_t now)
{
    if (gate->state == GATE_ALERT) {
        if (now - gate->alert_start_ms > gate->alert_timeout_ms) {
            LOG_WARN("gate", "ch%d ALERT timeout after %lums (%lu frames sent)",
                     gate->channel,
                     (unsigned long)(now - gate->alert_start_ms),
                     gate->frames_sent);
            gate_eom(gate);
        }
    }
}

void gate_close(gate_t *gate)
{
    if (gate->state != GATE_IDLE) {
        int16_t silence[USRP_SAMPLES] = {0};
        usrp_send_audio(&gate->usrp, silence, 0);
    }
    usrp_close(&gate->usrp);
}
