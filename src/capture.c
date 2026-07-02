#include "capture.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rtl-sdr.h>

#define CAPTURE_BUF_LEN (16 * 16384)

static void rtlsdr_callback(unsigned char *buf, uint32_t len, void *ctx)
{
    capture_t *cap = (capture_t *)ctx;
    if (!cap->running)
        return;

    cap->total_samples += len / 2;
    cap->callback_count++;

    if (len == 0) {
        cap->overflow_count++;
        LOG_WARN("sdr", "zero-length callback (overflow #%lu)",
                 cap->overflow_count);
        return;
    }

    LOG_TRACE("sdr", "callback len=%u samples=%lu callbacks=%lu",
              len, cap->total_samples, cap->callback_count);

    if (cap->callback)
        cap->callback(buf, len, cap->userdata);
}

static void *capture_thread(void *arg)
{
    capture_t *cap = (capture_t *)arg;
    rtlsdr_dev_t *dev = (rtlsdr_dev_t *)cap->dev;

    rtlsdr_read_async(dev, rtlsdr_callback, cap, 0, CAPTURE_BUF_LEN);
    return NULL;
}

int capture_init(capture_t *cap, const config_t *cfg)
{
    memset(cap, 0, sizeof(*cap));
    cap->center_freq = CAPTURE_CENTER_FREQ;
    cap->sample_rate = CAPTURE_SAMPLE_RATE;
    cap->gain = cfg->gain;
    cap->ppm = cfg->ppm;
    cap->device_index = cfg->device_index;

    rtlsdr_dev_t *dev = NULL;
    int r = rtlsdr_open(&dev, cfg->device_index);
    if (r < 0) {
        LOG_ERROR("sdr", "failed to open device %u (err=%d)", cfg->device_index, r);
        return -1;
    }

    rtlsdr_set_sample_rate(dev, cap->sample_rate);
    rtlsdr_set_center_freq(dev, cap->center_freq);

    if (cfg->ppm) {
        rtlsdr_set_freq_correction(dev, cfg->ppm);
        LOG_DEBUG("sdr", "ppm correction=%d", cfg->ppm);
    }

    if (cfg->gain < 0) {
        rtlsdr_set_tuner_gain_mode(dev, 0);
        int actual = rtlsdr_get_tuner_gain(dev);
        LOG_INFO("sdr", "gain=auto (tuner reports %d = %.1f dB)",
                 actual, actual / 10.0);
    } else {
        rtlsdr_set_tuner_gain_mode(dev, 1);
        rtlsdr_set_tuner_gain(dev, cfg->gain);
        LOG_INFO("sdr", "gain=%d (%.1f dB)", cfg->gain, cfg->gain / 10.0);
    }

    rtlsdr_reset_buffer(dev);
    cap->dev = dev;

    LOG_INFO("sdr", "device %u opened: %.6f MHz @ %u S/s",
             cfg->device_index, cap->center_freq / 1e6, cap->sample_rate);
    return 0;
}

int capture_start(capture_t *cap)
{
    cap->running = 1;
    if (pthread_create(&cap->thread, NULL, capture_thread, cap) != 0) {
        LOG_ERROR("sdr", "failed to create capture thread");
        cap->running = 0;
        return -1;
    }
    LOG_DEBUG("sdr", "capture thread started");
    return 0;
}

void capture_stop(capture_t *cap)
{
    cap->running = 0;
    if (cap->dev) {
        rtlsdr_cancel_async((rtlsdr_dev_t *)cap->dev);
        pthread_join(cap->thread, NULL);
        rtlsdr_close((rtlsdr_dev_t *)cap->dev);
        cap->dev = NULL;
    }
    LOG_INFO("sdr", "stopped: %lu samples, %lu callbacks, %lu overflows",
             cap->total_samples, cap->callback_count, cap->overflow_count);
}
