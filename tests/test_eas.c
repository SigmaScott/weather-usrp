#include "test.h"
#include "../src/eas.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static char last_message[512];
static int callback_count;

static void test_eas_callback(int channel, const char *message, void *userdata)
{
    (void)channel;
    (void)userdata;
    strncpy(last_message, message, sizeof(last_message) - 1);
    last_message[sizeof(last_message) - 1] = '\0';
    callback_count++;
}

static float afsk_phase = 0.0f;
static float afsk_bit_clock = 0.0f;

static void generate_afsk_byte(float *buf, int *pos, uint8_t byte,
                                float sample_rate)
{
    float samples_per_bit = sample_rate / EAS_BAUD;

    for (int bit = 0; bit < 8; bit++) {
        int b = (byte >> bit) & 1;
        float freq = b ? EAS_MARK_FREQ : EAS_SPACE_FREQ;
        float phase_inc = 2.0f * (float)M_PI * freq / sample_rate;

        afsk_bit_clock += samples_per_bit;
        while (*pos < (int)afsk_bit_clock) {
            buf[(*pos)++] = sinf(afsk_phase) * 0.8f;
            afsk_phase += phase_inc;
            if (afsk_phase > 2.0f * (float)M_PI) afsk_phase -= 2.0f * (float)M_PI;
        }
    }
}

static int generate_same_burst(float *buf, const char *message, float sample_rate)
{
    int pos = 0;

    for (int i = 0; i < 16; i++)
        generate_afsk_byte(buf, &pos, 0xAB, sample_rate);

    for (int i = 0; message[i]; i++)
        generate_afsk_byte(buf, &pos, (uint8_t)message[i], sample_rate);

    /* Tail silence: gives decoder enough samples to clock out final bit */
    int tail = (int)(sample_rate / EAS_BAUD) + 1;
    for (int i = 0; i < tail; i++)
        buf[pos++] = 0.0f;

    return pos;
}

static void test_eas_decode_zczc(void)
{
    eas_decoder_t eas;
    eas_init(&eas, 48000.0f, 0, test_eas_callback, NULL);

    const char *same_msg = "ZCZC-WXR-TOR-048453+0100-1411545-KHOU/NWS-";
    float buf[500000];
    callback_count = 0;
    memset(last_message, 0, sizeof(last_message));

    afsk_phase = 0.0f;
    afsk_bit_clock = 0.0f;
    int len1 = generate_same_burst(buf, same_msg, 48000.0f);
    eas_process(&eas, buf, len1);

    int silence_samples = (int)(48000.0f * 1.0f);
    float silence[48000];
    memset(silence, 0, sizeof(silence));
    eas_process(&eas, silence, silence_samples);

    afsk_phase = 0.0f;
    afsk_bit_clock = 0.0f;
    int len2 = generate_same_burst(buf, same_msg, 48000.0f);
    eas_process(&eas, buf, len2);

    ASSERT_EQ_INT(callback_count, 1, "callback fired once");
    ASSERT_EQ_STR(last_message, same_msg, "decoded message matches");
}

static void test_eas_decode_nnnn(void)
{
    eas_decoder_t eas;
    eas_init(&eas, 48000.0f, 0, test_eas_callback, NULL);

    float buf[200000];
    callback_count = 0;
    memset(last_message, 0, sizeof(last_message));

    afsk_phase = 0.0f;
    afsk_bit_clock = 0.0f;
    int len1 = generate_same_burst(buf, "NNNN", 48000.0f);
    eas_process(&eas, buf, len1);

    float silence[48000];
    memset(silence, 0, sizeof(silence));
    eas_process(&eas, silence, 48000);

    afsk_phase = 0.0f;
    afsk_bit_clock = 0.0f;
    int len2 = generate_same_burst(buf, "NNNN", 48000.0f);
    eas_process(&eas, buf, len2);

    ASSERT_EQ_INT(callback_count, 1, "EOM callback fired");
    ASSERT_EQ_STR(last_message, "NNNN", "EOM message is NNNN");
}

static void test_eas_reset(void)
{
    eas_decoder_t eas;
    eas_init(&eas, 48000.0f, 0, test_eas_callback, NULL);
    callback_count = 0;

    eas_reset(&eas);
    ASSERT_EQ_INT(eas.synced, 0, "reset clears sync");
    ASSERT_EQ_INT(eas.burst_idx, 0, "reset clears burst idx");
    ASSERT_EQ_INT(eas.in_message, 0, "reset clears in_message");
}

int main(void)
{
    fprintf(stderr, "test_eas:\n");
    RUN_TEST(test_eas_decode_zczc);
    RUN_TEST(test_eas_decode_nnnn);
    RUN_TEST(test_eas_reset);
    TEST_SUMMARY();
}
