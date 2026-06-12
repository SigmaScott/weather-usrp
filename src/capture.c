#include "capture.h"
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
        fprintf(stderr, "capture: failed to open device %u\n", cfg->device_index);
        return -1;
    }

    rtlsdr_set_sample_rate(dev, cap->sample_rate);
    rtlsdr_set_center_freq(dev, cap->center_freq);

    if (cfg->ppm)
        rtlsdr_set_freq_correction(dev, cfg->ppm);

    if (cfg->gain < 0) {
        rtlsdr_set_tuner_gain_mode(dev, 0);
    } else {
        rtlsdr_set_tuner_gain_mode(dev, 1);
        rtlsdr_set_tuner_gain(dev, cfg->gain);
    }

    rtlsdr_reset_buffer(dev);
    cap->dev = dev;
    return 0;
}

int capture_start(capture_t *cap)
{
    cap->running = 1;
    if (pthread_create(&cap->thread, NULL, capture_thread, cap) != 0) {
        perror("capture: pthread_create");
        cap->running = 0;
        return -1;
    }
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
}
