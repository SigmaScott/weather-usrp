#ifndef RTL_SDR_H
#define RTL_SDR_H

#include <stdint.h>

typedef struct rtlsdr_dev rtlsdr_dev_t;
typedef void(*rtlsdr_read_async_cb_t)(unsigned char *buf, uint32_t len, void *ctx);

int rtlsdr_open(rtlsdr_dev_t **dev, uint32_t index);
int rtlsdr_close(rtlsdr_dev_t *dev);
int rtlsdr_set_sample_rate(rtlsdr_dev_t *dev, uint32_t rate);
int rtlsdr_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq);
int rtlsdr_set_freq_correction(rtlsdr_dev_t *dev, int ppm);
int rtlsdr_set_tuner_gain_mode(rtlsdr_dev_t *dev, int manual);
int rtlsdr_set_tuner_gain(rtlsdr_dev_t *dev, int gain);
int rtlsdr_reset_buffer(rtlsdr_dev_t *dev);
int rtlsdr_read_async(rtlsdr_dev_t *dev, rtlsdr_read_async_cb_t cb,
                       void *ctx, uint32_t buf_num, uint32_t buf_len);
int rtlsdr_cancel_async(rtlsdr_dev_t *dev);

#endif
