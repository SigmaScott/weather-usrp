#include "test.h"
#include "../src/dsp.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void test_lpf_design(void)
{
    float taps[64];
    design_lpf(taps, 64, 0.1);

    float sum = 0.0f;
    for (int i = 0; i < 64; i++) sum += taps[i];
    ASSERT_NEAR(sum, 1.0, 0.001, "LPF unity DC gain");

    ASSERT_NEAR(taps[31], taps[32], 0.001, "LPF symmetric center");
    ASSERT_NEAR(taps[0], taps[63], 0.001, "LPF symmetric edges");
}

static void test_fm_demod_dc(void)
{
    fm_demod_t fm;
    fm_demod_init(&fm);

    iq_sample_t constant = {1.0f, 0.0f};
    float out = fm_demod_process(&fm, constant);
    (void)out;
    out = fm_demod_process(&fm, constant);
    ASSERT_NEAR(out, 0.0, 0.001, "constant IQ = zero frequency");
}

static void test_fm_demod_positive_freq(void)
{
    fm_demod_t fm;
    fm_demod_init(&fm);

    float phase = 0.0f;
    float mod_freq = 0.01f;
    float dev = 0.2f;
    int n = 2000;
    float peak = 0.0f;

    for (int i = 0; i < n; i++) {
        float mod = dev * sinf(2.0f * (float)M_PI * mod_freq * i);
        phase += 2.0f * (float)M_PI * mod;
        iq_sample_t s = {cosf(phase), sinf(phase)};
        float out = fm_demod_process(&fm, s);
        if (i > 500 && fabsf(out) > peak)
            peak = fabsf(out);
    }

    ASSERT_NEAR(peak, dev * 2.0, 0.05, "positive freq demod tracks modulation");
}

static void test_channelizer_passthrough(void)
{
    fir_chan_t fir;
    fir_chan_init(&fir, 0.0, 48000.0, 12500.0);

    float phase = 0.0f;
    float freq = 1000.0f / 48000.0f;
    int output_count = 0;
    float power = 0.0f;

    for (int i = 0; i < 5000; i++) {
        iq_sample_t in = {cosf(2.0f * (float)M_PI * phase), 0.0f};
        phase += freq;

        iq_sample_t out;
        if (fir_chan_process(&fir, in, &out)) {
            output_count++;
            power += out.i * out.i + out.q * out.q;
        }
    }

    ASSERT(output_count > 0, "channelizer produces output");
    ASSERT(power > 0.01f, "baseband tone has energy through 0-offset channelizer");
}

static void test_channelizer_rejection(void)
{
    fir_chan_t fir;
    fir_chan_init(&fir, 0.0, 48000.0, 12500.0);

    float phase = 0.0f;
    float freq = 20000.0f / 48000.0f;
    float power = 0.0f;
    int output_count = 0;

    for (int i = 0; i < 5000; i++) {
        iq_sample_t in = {cosf(2.0f * (float)M_PI * phase), 0.0f};
        phase += freq;

        iq_sample_t out;
        if (fir_chan_process(&fir, in, &out)) {
            output_count++;
            power += out.i * out.i + out.q * out.q;
        }
    }

    float avg_power = (output_count > 0) ? power / output_count : 0.0f;
    ASSERT(avg_power < 0.001f, "20kHz tone rejected by 12.5kHz channelizer");
}

static void test_decimator_rate(void)
{
    decimator_t dec;
    decimator_init(&dec);

    int output_count = 0;
    for (int i = 0; i < 48000; i++) {
        float out;
        if (decimator_process(&dec, 0.5f, &out))
            output_count++;
    }

    ASSERT_EQ_INT(output_count, 8000, "48kHz -> 8kHz decimation ratio");
}

int main(void)
{
    fprintf(stderr, "test_dsp:\n");
    RUN_TEST(test_lpf_design);
    RUN_TEST(test_fm_demod_dc);
    RUN_TEST(test_fm_demod_positive_freq);
    RUN_TEST(test_channelizer_passthrough);
    RUN_TEST(test_channelizer_rejection);
    RUN_TEST(test_decimator_rate);
    TEST_SUMMARY();
}
