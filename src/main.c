#include "common.h"
#include "config.h"
#include "capture.h"
#include "dsp.h"
#include "eas.h"
#include "same.h"
#include "gate.h"
#include "control.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

typedef struct channel_s {
    int             id;
    fir_chan_t       channelizer;
    fm_demod_t      demod;
    decimator_t     decimator;
    eas_decoder_t   eas;
    gate_t          gate;
    const channel_config_t *config;
} channel_t;

static volatile int running = 1;
static config_t cfg;
static channel_t channels[NUM_CHANNELS];
static gate_t *gate_ptrs[NUM_CHANNELS];
static capture_t capture;
static control_t control;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static void gate_event_handler(int channel, const char *event, void *userdata)
{
    (void)channel;
    control_t *ctl = (control_t *)userdata;
    control_broadcast(ctl, event);
}

static void eas_alert_handler(int channel, const char *message, void *userdata)
{
    (void)userdata;
    same_message_t msg;
    if (same_parse(&msg, message) < 0)
        return;

    channel_t *ch = &channels[channel];

    if (msg.is_eom) {
        gate_eom(&ch->gate);
        return;
    }

    if (same_match_fips(&msg, ch->config->fips, ch->config->num_fips)) {
        char formatted[256];
        same_format(&msg, formatted, sizeof(formatted));
        fprintf(stderr, "[ch%d] ALERT: %s\n", channel, formatted);
        gate_alert(&ch->gate, &msg);
    }
}

static void capture_callback(const uint8_t *buf, uint32_t len, void *userdata)
{
    (void)userdata;
    if (len < 2) return;

    for (uint32_t i = 0; i + 1 < len; i += 2) {
        iq_sample_t raw;
        raw.i = ((float)buf[i] - 127.5f) / 127.5f;
        raw.q = ((float)buf[i + 1] - 127.5f) / 127.5f;

        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            if (!channels[ch].config->enabled)
                continue;

            iq_sample_t chan_out;
            if (!fir_chan_process(&channels[ch].channelizer, raw, &chan_out))
                continue;

            float audio = fm_demod_process(&channels[ch].demod, chan_out);

            eas_process(&channels[ch].eas, &audio, 1);

            float decimated;
            if (decimator_process(&channels[ch].decimator, audio, &decimated)) {
                float scaled = decimated * 16000.0f;
                if (scaled > 32767.0f) scaled = 32767.0f;
                if (scaled < -32768.0f) scaled = -32768.0f;
                int16_t sample = (int16_t)scaled;
                gate_process_audio(&channels[ch].gate, &sample, 1);
            }
        }
    }
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [-c config.ini] [-v]\n", prog);
    exit(1);
}

int main(int argc, char *argv[])
{
    const char *config_path = "config.ini";
    int verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "c:vh")) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'v': verbose = 1; break;
        default: usage(argv[0]);
        }
    }

    if (config_load(&cfg, config_path) < 0)
        return 1;
    cfg.verbose = verbose;

    if (verbose)
        config_dump(&cfg);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].id = i;
        channels[i].config = &cfg.channels[i];

        double freq_offset = cfg.channels[i].frequency - CAPTURE_CENTER_FREQ;
        fir_chan_init(&channels[i].channelizer, freq_offset,
                     CAPTURE_SAMPLE_RATE, 12500.0);
        fm_demod_init(&channels[i].demod);
        decimator_init(&channels[i].decimator);
        eas_init(&channels[i].eas, CHANNEL_AUDIO_RATE, i,
                 eas_alert_handler, NULL);

        if (gate_init(&channels[i].gate, i, &cfg.channels[i],
                      gate_event_handler, &control) < 0) {
            fprintf(stderr, "Failed to init gate for channel %d\n", i);
            return 1;
        }
        gate_ptrs[i] = &channels[i].gate;
    }

    if (control_init(&control, cfg.control_host, cfg.control_port,
                     gate_ptrs) < 0) {
        fprintf(stderr, "Failed to init control server\n");
        return 1;
    }

    if (control_start(&control) < 0)
        return 1;

    fprintf(stderr, "Control server on %s:%u\n",
            cfg.control_host, cfg.control_port);

    if (capture_init(&capture, &cfg) < 0) {
        fprintf(stderr, "Failed to init RTL-SDR capture\n");
        control_stop(&control);
        return 1;
    }
    capture.callback = capture_callback;
    capture.userdata = NULL;

    fprintf(stderr, "Capture: %.3f MHz @ %u S/s\n",
            CAPTURE_CENTER_FREQ / 1e6, CAPTURE_SAMPLE_RATE);

    if (capture_start(&capture) < 0) {
        control_stop(&control);
        return 1;
    }

    fprintf(stderr, "Running. Press Ctrl-C to stop.\n");

    struct timespec ts;
    while (running) {
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000;
        nanosleep(&ts, NULL);

        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        for (int i = 0; i < NUM_CHANNELS; i++)
            gate_tick(&channels[i].gate, now);
    }

    fprintf(stderr, "\nShutting down...\n");
    capture_stop(&capture);
    control_stop(&control);

    for (int i = 0; i < NUM_CHANNELS; i++)
        gate_close(&channels[i].gate);

    return 0;
}
