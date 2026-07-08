/*
 * NOAA Weather decoder Allstar Repeater Bridge
 * Copyright (C) 2026 Scott Gillins W2KP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * dsp.h - DSP routines: FIR channelizer, FM demodulator, decimation
 */

#ifndef DSP_H
#define DSP_H

#include "common.h"

/* Complex sample (IQ) */
typedef struct {
    float i;
    float q;
} iq_sample_t;

/* FIR channelizer state (one per channel) */
typedef struct {
    float       taps_i[FIR_TAPS];   /* filter taps (real part, freq-shifted) */
    float       taps_q[FIR_TAPS];   /* filter taps (imag part, freq-shifted) */
    iq_sample_t history[FIR_TAPS];  /* delay line */
    int         hist_idx;           /* circular buffer index */
    int         decim_count;        /* decimation counter */
    int         decim_factor;       /* decimation ratio */
} fir_chan_t;

/* FM demodulator state */
typedef struct {
    iq_sample_t prev;               /* previous IQ sample */
    float       dc_prev_in;         /* DC-block: previous input */
    float       dc_prev_out;        /* DC-block: previous output */
} fm_demod_t;

/* Audio decimator (48k -> 8k) */
typedef struct {
    float       taps[FIR_TAPS];     /* LPF taps for anti-alias */
    float       history[FIR_TAPS];  /* delay line */
    int         hist_idx;
    int         decim_count;
    int         decim_factor;
} decimator_t;

/*
 * Initialize channelizer FIR for a given channel.
 * freq_offset: channel offset from center in Hz (can be negative)
 * sample_rate: input sample rate
 * bandwidth: channel bandwidth in Hz
 */
void fir_chan_init(fir_chan_t *fir, double freq_offset,
                   double sample_rate, double bandwidth);

/*
 * Process one IQ sample through channelizer FIR.
 * Returns 1 if decimated output is ready, 0 otherwise.
 * Output written to *out when ready.
 */
int fir_chan_process(fir_chan_t *fir, iq_sample_t in, iq_sample_t *out);

/*
 * Initialize FM demodulator.
 */
void fm_demod_init(fm_demod_t *fm);

/*
 * Demodulate one IQ sample to audio.
 * Returns instantaneous frequency as float [-1.0, 1.0].
 */
float fm_demod_process(fm_demod_t *fm, iq_sample_t in);

/*
 * Initialize audio decimator (48kHz -> 8kHz).
 */
void decimator_init(decimator_t *dec);

/*
 * Process one audio sample through decimator.
 * Returns 1 if decimated output ready, 0 otherwise.
 * Output written to *out when ready.
 */
int decimator_process(decimator_t *dec, float in, float *out);

/*
 * Design a low-pass FIR filter (windowed sinc, Blackman window).
 * taps: output array
 * num_taps: filter length
 * cutoff: normalized cutoff frequency (0.0 to 0.5)
 */
void design_lpf(float *taps, int num_taps, double cutoff);

#endif /* DSP_H */
