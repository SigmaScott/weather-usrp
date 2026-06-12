#include "test.h"
#include "../src/config.h"
#include <unistd.h>

static void test_config_load_valid(void)
{
    config_t cfg;
    ASSERT_EQ_INT(config_load(&cfg, "config.ini"), 0, "load config.ini");
    ASSERT_EQ_INT(cfg.device_index, 0, "device_index");
    ASSERT_EQ_INT(cfg.gain, -1, "gain auto");
    ASSERT_EQ_INT(cfg.ppm, 0, "ppm zero");
    ASSERT_EQ_STR(cfg.control_host, "127.0.0.1", "control host");
    ASSERT_EQ_INT(cfg.control_port, 5555, "control port");

    ASSERT_NEAR(cfg.channels[0].frequency, 162400000.0, 1.0, "ch0 freq");
    ASSERT_NEAR(cfg.channels[6].frequency, 162550000.0, 1.0, "ch6 freq");
    ASSERT_EQ_STR(cfg.channels[0].usrp_host, "127.0.0.1", "ch0 usrp host");
    ASSERT_EQ_INT(cfg.channels[0].usrp_port, 34001, "ch0 usrp port");
    ASSERT_EQ_INT(cfg.channels[0].enabled, 1, "ch0 enabled");
    ASSERT_EQ_INT(cfg.channels[0].num_fips, 2, "ch0 fips count");
    ASSERT_EQ_STR(cfg.channels[0].fips[0], "048453", "ch0 fips[0]");
    ASSERT_EQ_STR(cfg.channels[0].fips[1], "048491", "ch0 fips[1]");
}

static void test_config_missing_file(void)
{
    config_t cfg;
    ASSERT_EQ_INT(config_load(&cfg, "/nonexistent.ini"), -1, "missing file returns -1");
}

static void test_config_defaults(void)
{
    const char *tmp = "/tmp/opencode/test_minimal.ini";
    FILE *f = fopen(tmp, "w");
    fprintf(f, "[sdr]\ndevice_index = 2\n");
    fclose(f);

    config_t cfg;
    ASSERT_EQ_INT(config_load(&cfg, tmp), 0, "load minimal config");
    ASSERT_EQ_INT(cfg.device_index, 2, "device index from file");
    ASSERT_NEAR(cfg.channels[0].frequency, 162400000.0, 1.0, "default ch0 freq");
    ASSERT_NEAR(cfg.channels[3].frequency, 162475000.0, 1.0, "default ch3 freq");
    ASSERT_EQ_INT(cfg.channels[0].usrp_port, 34001, "default ch0 port");
    ASSERT_EQ_INT(cfg.channels[0].enabled, 1, "default enabled");

    unlink(tmp);
}

static void test_config_whitespace(void)
{
    const char *tmp = "/tmp/opencode/test_ws.ini";
    FILE *f = fopen(tmp, "w");
    fprintf(f, "[channel0]\n  frequency  =  162400000  \n  fips = 048453 , 048491 \n");
    fclose(f);

    config_t cfg;
    ASSERT_EQ_INT(config_load(&cfg, tmp), 0, "load whitespace config");
    ASSERT_NEAR(cfg.channels[0].frequency, 162400000.0, 1.0, "whitespace freq");
    ASSERT_EQ_INT(cfg.channels[0].num_fips, 2, "whitespace fips count");
    ASSERT_EQ_STR(cfg.channels[0].fips[0], "048453", "whitespace fips[0]");
    ASSERT_EQ_STR(cfg.channels[0].fips[1], "048491", "whitespace fips[1]");

    unlink(tmp);
}

int main(void)
{
    fprintf(stderr, "test_config:\n");
    RUN_TEST(test_config_load_valid);
    RUN_TEST(test_config_missing_file);
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_whitespace);
    TEST_SUMMARY();
}
