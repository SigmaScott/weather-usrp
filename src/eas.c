#include "eas.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PREAMBLE_BYTE   0xAB
#define PREAMBLE_MIN    16
#define BURST_GAP_SECS  3.0f

void eas_init(eas_decoder_t *eas, float sample_rate, int channel,
              eas_callback_t callback, void *userdata)
{
    memset(eas, 0, sizeof(*eas));
    eas->sample_rate = sample_rate;
    eas->channel = channel;
    eas->callback = callback;
    eas->userdata = userdata;

    eas->corr_len = (int)(sample_rate / EAS_BAUD);
    eas->bit_freq = sample_rate / EAS_BAUD;
    eas->burst_timeout = (int)(sample_rate * BURST_GAP_SECS);
}

static float correlator_energy(const float *buf_i, const float *buf_q,
                               int len, int idx)
{
    float ei = 0.0f, eq = 0.0f;
    for (int i = 0; i < len; i++) {
        int k = (idx + i) % 128;
        ei += buf_i[k];
        eq += buf_q[k];
    }
    return ei * ei + eq * eq;
}

static void process_bit(eas_decoder_t *eas, int bit)
{
    if (eas->synced)
        eas->silence_count = 0;

    /* LSB first into shift register */
    eas->shift_reg = (eas->shift_reg >> 1) | (bit ? 0x80 : 0x00);
    eas->bit_count++;

    if (!eas->synced) {
        if (eas->shift_reg == PREAMBLE_BYTE) {
            eas->preamble_count++;
            eas->bit_count = 0;
            if (eas->preamble_count >= PREAMBLE_MIN)
                eas->synced = 1;
        } else if (eas->bit_count >= 8) {
            eas->preamble_count = 0;
            eas->bit_count = 0;
        }
        return;
    }

    if (eas->bit_count < 8)
        return;
    eas->bit_count = 0;

    uint8_t byte = eas->shift_reg;

    if (!eas->in_message) {
        /* Look for ZCZC or NNNN start */
        if (byte == 'Z' || byte == 'N') {
            eas->msg_buf[eas->burst_idx][0] = (char)byte;
            eas->msg_pos = 1;
            eas->in_message = 1;
        }
        return;
    }

    /* Accumulate message bytes */
    if (eas->msg_pos < EAS_MAX_MSG_LEN) {
        eas->msg_buf[eas->burst_idx][eas->msg_pos++] = (char)byte;
    }

    /* Check for EOM: "NNNN" is exactly 4 bytes, no trailing dash */
    if (eas->msg_pos == 4 &&
        strncmp(eas->msg_buf[eas->burst_idx], "NNNN", 4) == 0) {
        eas->msg_buf[eas->burst_idx][4] = '\0';
        eas->msg_len[eas->burst_idx] = 4;

        for (int i = 0; i < eas->burst_idx; i++) {
            if (eas->msg_len[i] == 4 &&
                strncmp(eas->msg_buf[i], "NNNN", 4) == 0) {
                if (eas->callback)
                    eas->callback(eas->channel, "NNNN", eas->userdata);
                eas_reset(eas);
                return;
            }
        }

        eas->burst_idx++;
        if (eas->burst_idx >= EAS_NUM_BURSTS) {
            if (eas->callback)
                eas->callback(eas->channel, "NNNN", eas->userdata);
            eas_reset(eas);
            return;
        }
        eas->in_message = 0;
        eas->synced = 0;
        eas->preamble_count = 0;
        return;
    }

    /* Check for end of SAME message: 3 dashes after '+' (duration marker) */
    if (byte == '-' && eas->msg_pos > 10) {
        char *plus = memchr(eas->msg_buf[eas->burst_idx], '+', eas->msg_pos);
        if (plus && strncmp(eas->msg_buf[eas->burst_idx], "ZCZC-", 5) == 0) {
            int dashes = 0;
            int plus_pos = (int)(plus - eas->msg_buf[eas->burst_idx]);
            for (int j = plus_pos + 1; j < eas->msg_pos; j++) {
                if (eas->msg_buf[eas->burst_idx][j] == '-')
                    dashes++;
            }
            if (dashes >= 3) {
                eas->msg_buf[eas->burst_idx][eas->msg_pos] = '\0';
                eas->msg_len[eas->burst_idx] = eas->msg_pos;

                /* 2-of-3 voting */
                for (int i = 0; i < eas->burst_idx; i++) {
                    if (eas->msg_len[i] == eas->msg_pos &&
                        strncmp(eas->msg_buf[i], eas->msg_buf[eas->burst_idx],
                                eas->msg_pos) == 0) {
                        if (eas->callback)
                            eas->callback(eas->channel,
                                          eas->msg_buf[eas->burst_idx],
                                          eas->userdata);
                        eas_reset(eas);
                        return;
                    }
                }

                eas->burst_idx++;
                if (eas->burst_idx >= EAS_NUM_BURSTS) {
                    if (eas->callback)
                        eas->callback(eas->channel, eas->msg_buf[0], eas->userdata);
                    eas_reset(eas);
                    return;
                }

                eas->in_message = 0;
                eas->synced = 0;
                eas->preamble_count = 0;
            }
        }
    }
}

void eas_process(eas_decoder_t *eas, const float *samples, int count)
{
    float mark_phase_inc = 2.0f * (float)M_PI * EAS_MARK_FREQ / eas->sample_rate;
    float space_phase_inc = 2.0f * (float)M_PI * EAS_SPACE_FREQ / eas->sample_rate;
    int corr_len = eas->corr_len;

    for (int s = 0; s < count; s++) {
        float sample = samples[s];
        int idx = eas->corr_idx;

        eas->mark_i[idx] = sample * cosf(eas->mark_phase);
        eas->mark_q[idx] = sample * sinf(eas->mark_phase);
        eas->space_i[idx] = sample * cosf(eas->space_phase);
        eas->space_q[idx] = sample * sinf(eas->space_phase);

        eas->mark_phase += mark_phase_inc;
        if (eas->mark_phase > 2.0f * (float)M_PI)
            eas->mark_phase -= 2.0f * (float)M_PI;
        eas->space_phase += space_phase_inc;
        if (eas->space_phase > 2.0f * (float)M_PI)
            eas->space_phase -= 2.0f * (float)M_PI;

        eas->corr_idx = (eas->corr_idx + 1) % 128;

        /* Advance bit clock */
        eas->bit_phase += 1.0f;
        if (eas->bit_phase >= eas->bit_freq) {
            eas->bit_phase -= eas->bit_freq;

            int clen = corr_len > 128 ? 128 : corr_len;
            int start = (eas->corr_idx + 128 - clen) % 128;
            float mark_e = correlator_energy(eas->mark_i, eas->mark_q,
                                             clen, start);
            float space_e = correlator_energy(eas->space_i, eas->space_q,
                                              clen, start);

            int bit = (mark_e > space_e) ? 1 : 0;

            if (bit != eas->last_bit) {
                float err = eas->bit_phase / eas->bit_freq;
                if (err > 0.5f) err -= 1.0f;
                eas->bit_phase -= err * 0.5f;
            }
            eas->last_bit = bit;

            process_bit(eas, bit);
        }

        /* Burst timeout detection */
        eas->silence_count++;
        if (eas->silence_count > eas->burst_timeout && eas->burst_idx > 0) {
            if (eas->msg_len[0] > 0 && eas->callback)
                eas->callback(eas->channel, eas->msg_buf[0], eas->userdata);
            eas_reset(eas);
        }
    }
}

void eas_reset(eas_decoder_t *eas)
{
    eas->synced = 0;
    eas->preamble_count = 0;
    eas->bit_count = 0;
    eas->shift_reg = 0;
    eas->in_message = 0;
    eas->burst_idx = 0;
    eas->msg_pos = 0;
    eas->silence_count = 0;
    eas->bit_phase = 0;
    eas->mark_phase = 0;
    eas->space_phase = 0;
    for (int i = 0; i < EAS_NUM_BURSTS; i++)
        eas->msg_len[i] = 0;
}
