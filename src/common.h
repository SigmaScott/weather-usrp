/*
 * weather-usrp - NOAA Weather Radio SAME/EAS alert gate for AllStarLink
 * Copyright (C) 2024 - GPL v2
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
    int         enabled;                /* channel enabled flag */
} channel_config_t;

/* Global configuration */
struct config_s {
    /* SDR settings */
    uint32_t    device_index;
    int         gain;                   /* tenths of dB, or -1 for auto */
    int         ppm;                    /* frequency correction */

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
