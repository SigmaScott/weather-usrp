#include "common.h"
#include "config.h"
#include "capture.h"
#include "dsp.h"
#include "eas.h"
#include "same.h"
#include "gate.h"
#include "control.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

typedef struct channel_s {
    int             id;
    fir_chan_t       channelizer;
    fm_demod_t      demod;
    decimator_t     decimator;
    eas_decoder_t   eas;
    gate_t          gate;
    const channel_config_t *config;

    /* Signal diagnostics (accumulated between reports) */
    float           sig_power_sum;
    float           sig_power_peak;
    float           fm_demod_sum;
    float           fm_demod_peak;
    unsigned long   sig_sample_count;
} channel_t;

static volatile int running = 1;
static config_t cfg;
static channel_t channels[NUM_CHANNELS];
static gate_t *gate_ptrs[NUM_CHANNELS];
static capture_t capture;
static control_t control;

static unsigned long adc_clip_count;
static unsigned long adc_total_count;

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
    if (same_parse(&msg, message) < 0) {
        LOG_WARN("same", "ch%d parse failed: %.40s...", channel, message);
        return;
    }

    channel_t *ch = &channels[channel];

    if (msg.is_eom) {
        LOG_DEBUG("same", "ch%d received EOM", channel);
        gate_eom(&ch->gate);
        return;
    }

    if (same_match_fips(&msg, ch->config->fips, ch->config->num_fips)) {
        if (same_event_blacklisted(&msg, ch->config->event_blacklist,
                                   ch->config->num_event_blacklist)) {
            LOG_INFO("same", "ch%d BLOCKED (blacklisted event %s)",
                     channel, msg.event);
            return;
        }
        char formatted[256];
        same_format(&msg, formatted, sizeof(formatted));
        LOG_INFO("same", "ch%d ALERT: %s", channel, formatted);
        gate_alert(&ch->gate, &msg);
    } else {
        LOG_DEBUG("same", "ch%d no FIPS match (event=%s)", channel, msg.event);
    }
}

static void capture_callback(const uint8_t *buf, uint32_t len, void *userdata)
{
    (void)userdata;
    if (len < 2) return;

    unsigned long clips = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (buf[i] == 0 || buf[i] == 255)
            clips++;
    }
    adc_clip_count += clips;
    adc_total_count += len;

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

            float power = chan_out.i * chan_out.i + chan_out.q * chan_out.q;
            channels[ch].sig_power_sum += power;
            if (power > channels[ch].sig_power_peak)
                channels[ch].sig_power_peak = power;

            float audio = fm_demod_process(&channels[ch].demod, chan_out);

            float abs_audio = audio < 0 ? -audio : audio;
            channels[ch].fm_demod_sum += abs_audio;
            if (abs_audio > channels[ch].fm_demod_peak)
                channels[ch].fm_demod_peak = abs_audio;
            channels[ch].sig_sample_count++;

            eas_process(&channels[ch].eas, &audio, 1);

            float decimated;
            if (decimator_process(&channels[ch].decimator, audio, &decimated)) {
                float scaled = decimated * 16000.0f * cfg.audio_gain;
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
    fprintf(stderr, "Usage: %s [-c config.ini] [-v [-v [-v]]]\n", prog);
    fprintf(stderr, "  -v     INFO level (operational events)\n");
    fprintf(stderr, "  -vv    DEBUG level (per-event detail)\n");
    fprintf(stderr, "  -vvv   TRACE level (hot-path data)\n");
    fprintf(stderr, "  Default config search: ./config.ini, /etc/weather-usrp/config.ini\n");
    exit(1);
}

static const char *find_config(void)
{
    static const char *search_paths[] = {
        "config.ini",
        "/etc/weather-usrp/config.ini",
        NULL
    };

    for (int i = 0; search_paths[i]; i++) {
        FILE *f = fopen(search_paths[i], "r");
        if (f) {
            fclose(f);
            return search_paths[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    const char *config_path = NULL;
    int verbose = 0;

    int opt;
    while ((opt = getopt(argc, argv, "c:vh")) != -1) {
        switch (opt) {
        case 'c': config_path = optarg; break;
        case 'v': verbose++; break;
        default: usage(argv[0]);
        }
    }

    log_init(verbose);

    if (!config_path) {
        config_path = find_config();
        if (!config_path) {
            LOG_ERROR("main", "no config file found (tried ./config.ini, /etc/weather-usrp/config.ini)");
            fprintf(stderr, "Use -c <path> to specify config file\n");
            return 1;
        }
    }

    if (config_load(&cfg, config_path) < 0)
        return 1;
    cfg.verbose = verbose;

    LOG_INFO("main", "config loaded: %s", config_path);
    if (verbose >= 2)
        config_dump(&cfg);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].id = i;
        channels[i].config = &cfg.channels[i];

        double freq_offset = cfg.channels[i].frequency - cfg.center_freq;
        fir_chan_init(&channels[i].channelizer, freq_offset,
                     CAPTURE_SAMPLE_RATE, 20000.0);
        fm_demod_init(&channels[i].demod);
        decimator_init(&channels[i].decimator);
        eas_init(&channels[i].eas, CHANNEL_AUDIO_RATE, i,
                 eas_alert_handler, NULL);

        if (gate_init(&channels[i].gate, i, &cfg.channels[i],
                      gate_event_handler, &control) < 0) {
            LOG_ERROR("main", "failed to init gate for channel %d", i);
            return 1;
        }
        gate_ptrs[i] = &channels[i].gate;

        if (cfg.channels[i].enabled) {
            LOG_DEBUG("main", "ch%d: %.3f MHz, offset=%+.1f kHz, USRP -> %s:%u",
                      i, cfg.channels[i].frequency / 1e6,
                      freq_offset / 1000.0,
                      cfg.channels[i].usrp_host, cfg.channels[i].usrp_port);
        }
    }

    if (control_init(&control, cfg.control_host, cfg.control_port,
                     gate_ptrs) < 0) {
        LOG_ERROR("main", "failed to init control server");
        return 1;
    }

    if (control_start(&control) < 0)
        return 1;

    LOG_INFO("main", "control server on %s:%u",
             cfg.control_host, cfg.control_port);

    if (capture_init(&capture, &cfg) < 0) {
        LOG_ERROR("main", "failed to init RTL-SDR capture");
        control_stop(&control);
        return 1;
    }
    capture.callback = capture_callback;
    capture.userdata = NULL;

    LOG_INFO("main", "capture: %.3f MHz @ %u S/s",
             cfg.center_freq / 1e6, CAPTURE_SAMPLE_RATE);

    if (capture_start(&capture) < 0) {
        control_stop(&control);
        return 1;
    }

    LOG_INFO("main", "running (verbosity=%d)", verbose);
    fprintf(stderr, "Running. Press Ctrl-C to stop.\n");

    struct timespec ts;
    int tick_count = 0;
    while (running) {
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000;
        nanosleep(&ts, NULL);

        clock_gettime(CLOCK_MONOTONIC, &ts);
        uint64_t now = (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
        for (int i = 0; i < NUM_CHANNELS; i++)
            gate_tick(&channels[i].gate, now);

        tick_count++;
        if (tick_count >= 50 && LOG_ENABLED(LOG_LVL_INFO)) {
            tick_count = 0;

            if (adc_total_count > 0) {
                float clip_pct = 100.0f * adc_clip_count / adc_total_count;
                LOG_INFO("adc", "clip: %lu/%lu samples (%.2f%%)",
                         adc_clip_count, adc_total_count, clip_pct);
                adc_clip_count = 0;
                adc_total_count = 0;
            }

            for (int i = 0; i < NUM_CHANNELS; i++) {
                if (!cfg.channels[i].enabled)
                    continue;
                unsigned long n = channels[i].sig_sample_count;
                if (n == 0) continue;

                float avg_power = channels[i].sig_power_sum / n;
                float peak_power = channels[i].sig_power_peak;
                float avg_demod = channels[i].fm_demod_sum / n;
                float peak_demod = channels[i].fm_demod_peak;

                float avg_db = (avg_power > 1e-12f) ?
                    10.0f * log10f(avg_power) : -120.0f;
                float peak_db = (peak_power > 1e-12f) ?
                    10.0f * log10f(peak_power) : -120.0f;

                LOG_INFO("sig", "ch%d %.3fMHz: pwr avg=%.1fdB peak=%.1fdB | "
                         "FM dev avg=%.3f peak=%.3f (%.0fHz pk)",
                         i, cfg.channels[i].frequency / 1e6,
                         avg_db, peak_db,
                         avg_demod, peak_demod,
                         peak_demod * (CHANNEL_AUDIO_RATE / 2.0f));

                channels[i].sig_power_sum = 0;
                channels[i].sig_power_peak = 0;
                channels[i].fm_demod_sum = 0;
                channels[i].fm_demod_peak = 0;
                channels[i].sig_sample_count = 0;
            }
        }
    }

    LOG_INFO("main", "shutting down...");
    capture_stop(&capture);
    control_stop(&control);

    for (int i = 0; i < NUM_CHANNELS; i++)
        gate_close(&channels[i].gate);

    return 0;
}
