/*
 * eas.h - EAS/SAME AFSK demodulator and byte framer
 *
 * Implements a correlator-based FSK demodulator for the SAME protocol:
 * Mark = 2083.3 Hz, Space = 1562.5 Hz, Baud = 520.83
 *
 * Receives 48kHz float audio, demodulates to bits, assembles bytes,
 * detects preamble (0xAB), frames ZCZC messages, and performs
 * 2-of-3 burst voting.
 */

#ifndef EAS_H
#define EAS_H

#include "common.h"

/* EAS decoder callback - called when a validated SAME message is ready */
typedef void (*eas_callback_t)(int channel, const char *message, void *userdata);

/* EAS decoder state (one per channel) */
typedef struct {
    /* Correlator state */
    float       mark_i[128];        /* mark correlator buffer (in-phase) */
    float       mark_q[128];        /* mark correlator buffer (quadrature) */
    float       space_i[128];       /* space correlator buffer (in-phase) */
    float       space_q[128];       /* space correlator buffer (quadrature) */
    int         corr_idx;           /* correlator circular index */
    int         corr_len;           /* correlator length (samples per symbol) */
    float       mark_phase;         /* running mark reference phase */
    float       space_phase;        /* running space reference phase */

    /* Bit timing / DLL */
    float       bit_phase;          /* current phase within bit period */
    float       bit_freq;           /* samples per bit (nominally sample_rate/baud) */
    int         last_bit;           /* previous bit decision */

    /* Byte framer */
    uint8_t     shift_reg;          /* 8-bit shift register for byte assembly */
    int         bit_count;          /* bits received in current byte */
    int         synced;             /* preamble detected flag */
    int         preamble_count;     /* consecutive 0xAB bytes seen */

    /* Message assembly */
    char        msg_buf[EAS_NUM_BURSTS][EAS_MAX_MSG_LEN + 1];
    int         msg_len[EAS_NUM_BURSTS]; /* length of each stored burst */
    int         burst_idx;          /* which burst we're on (0-2) */
    int         msg_pos;            /* current position in active burst */
    int         in_message;         /* currently receiving a message */

    /* Timing */
    int         silence_count;      /* samples since last valid byte */
    int         burst_timeout;      /* samples before burst is considered done */

    /* Callback */
    eas_callback_t callback;
    void        *userdata;
    int         channel;

    /* Config */
    int         min_bursts;

    /* Sample rate */
    float       sample_rate;
} eas_decoder_t;

/*
 * Initialize EAS decoder for given sample rate.
 */
void eas_init(eas_decoder_t *eas, float sample_rate, int channel,
              int min_bursts, eas_callback_t callback, void *userdata);

/*
 * Process audio samples through EAS decoder.
 * samples: float audio buffer (mono, normalized -1.0 to 1.0)
 * count: number of samples
 */
void eas_process(eas_decoder_t *eas, const float *samples, int count);

/*
 * Reset EAS decoder state (e.g., after EOM received).
 */
void eas_reset(eas_decoder_t *eas);

#endif /* EAS_H */
