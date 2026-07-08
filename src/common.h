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
 * common.h - Shared constants, types, and macros
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>

/* Number of NOAA Weather Radio channels */
#define NUM_CHANNELS        7

/* NOAA WX frequencies (Hz) */
#define NOAA_FREQ_BASE      162400000
#define NOAA_FREQ_SPACING   25000

/* SDR capture parameters */
#define CAPTURE_SAMPLE_RATE 2400000     /* 2.4 MS/s */
#define CAPTURE_CENTER_FREQ 162482500   /* offset from ch4 to avoid DC spike */

/* DSP parameters */
#define CHANNEL_AUDIO_RATE  48000       /* internal audio rate per channel */
#define USRP_AUDIO_RATE     8000        /* USRP output rate */
#define DECIMATION_IQ       50          /* 2.4M / 50 = 48k */
#define DECIMATION_AUDIO    6           /* 48k / 6 = 8k */
#define FIR_TAPS            128         /* channelizer FIR length */

/* EAS/SAME parameters */
#define EAS_MARK_FREQ       2083.3f
#define EAS_SPACE_FREQ      1562.5f
#define EAS_BAUD            520.83f
#define EAS_MAX_MSG_LEN     268         /* max SAME message bytes */
#define EAS_NUM_BURSTS      3           /* headers sent 3 times */
#define EAS_MIN_MATCH       2           /* 2-of-3 must agree */

/* SAME message limits */
#define SAME_MAX_FIPS       31          /* max FIPS codes per message */
#define SAME_FIPS_LEN       6           /* PSSCCC length */
#define SAME_EVENT_LEN      3           /* EEE code length */
#define SAME_MAX_BLACKLIST  32          /* max blacklisted event codes */

/* USRP protocol constants */
#define USRP_MAGIC          "USRP"
#define USRP_HEADER_SIZE    32
#define USRP_AUDIO_SIZE     320         /* 160 samples * 2 bytes */
#define USRP_FRAME_SIZE     352         /* header + audio */
#define USRP_SAMPLES        160         /* samples per frame */
#define USRP_TYPE_VOICE     0
#define USRP_TYPE_TEXT      2

/* Gate states */
typedef enum {
    GATE_IDLE = 0,
    GATE_ALERT,
    GATE_PASSTHROUGH
} gate_state_t;

/* Forward declarations */
typedef struct channel_s channel_t;
typedef struct config_s config_t;

/* Per-channel configuration */
typedef struct {
    double      frequency;              /* channel frequency in Hz */
    char        usrp_host[64];          /* USRP destination host */
    uint16_t    usrp_port;              /* USRP destination port */
    char        fips[SAME_MAX_FIPS][SAME_FIPS_LEN + 1];  /* watched FIPS codes */
    int         num_fips;               /* number of configured FIPS codes */
    char        event_blacklist[SAME_MAX_BLACKLIST][SAME_EVENT_LEN + 1]; /* blocked event codes */
    int         num_event_blacklist;    /* number of blacklisted event codes */
    int         enabled;                /* channel enabled flag */
} channel_config_t;

/* Global configuration */
struct config_s {
    /* SDR settings */
    uint32_t    device_index;
    int         gain;                   /* tenths of dB, or -1 for auto */
    int         ppm;                    /* frequency correction */
    uint32_t    center_freq;            /* SDR center frequency in Hz */
    float       audio_gain;             /* audio output multiplier (1.0 = default) */
    int         eas_min_bursts;         /* 1-3: bursts needed before alert fires */

    /* Channel configs */
    channel_config_t channels[NUM_CHANNELS];

    /* Control interface */
    char        control_host[64];
    uint16_t    control_port;

    /* Runtime */
    int         verbose;
};

/* Utility macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#endif /* COMMON_H */
