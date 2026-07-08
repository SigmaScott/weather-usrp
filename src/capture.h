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
 * capture.h - RTL-SDR async IQ capture
 */

#ifndef CAPTURE_H
#define CAPTURE_H

#include "common.h"
#include "dsp.h"

/* Capture callback: receives raw IQ samples (uint8 interleaved I/Q) */
typedef void (*capture_cb_t)(const uint8_t *buf, uint32_t len, void *userdata);

/* Capture state */
typedef struct {
    void            *dev;           /* rtlsdr_dev_t* (opaque to avoid header dep) */
    pthread_t       thread;
    int             running;
    capture_cb_t    callback;
    void            *userdata;
    uint32_t        center_freq;
    uint32_t        sample_rate;
    int             gain;           /* tenths of dB, -1 = auto */
    int             ppm;
    uint32_t        device_index;

    /* Stats (for debug logging) */
    unsigned long   total_samples;
    unsigned long   callback_count;
    unsigned long   overflow_count;
} capture_t;

/*
 * Initialize RTL-SDR capture.
 * Returns 0 on success, -1 on error.
 */
int capture_init(capture_t *cap, const config_t *cfg);

/*
 * Start async capture thread.
 * Returns 0 on success, -1 on error.
 */
int capture_start(capture_t *cap);

/*
 * Stop capture and close device.
 */
void capture_stop(capture_t *cap);

#endif /* CAPTURE_H */
