#include "test.h"
#include "../src/same.h"

static void test_same_parse_valid(void)
{
    same_message_t msg;
    int r = same_parse(&msg, "ZCZC-WXR-TOR-048453-048491+0100-1411545-KHOU/NWS-");
    ASSERT_EQ_INT(r, 0, "parse valid SAME");
    ASSERT_EQ_STR(msg.originator, "WXR", "originator");
    ASSERT_EQ_STR(msg.event, "TOR", "event code");
    ASSERT_EQ_INT(msg.num_fips, 2, "fips count");
    ASSERT_EQ_STR(msg.fips[0], "048453", "fips[0]");
    ASSERT_EQ_STR(msg.fips[1], "048491", "fips[1]");
    ASSERT_EQ_INT(msg.duration_secs, 3600, "duration 1h");
    ASSERT_EQ_INT(msg.julian_day, 141, "julian day");
    ASSERT_EQ_INT(msg.hour, 15, "hour");
    ASSERT_EQ_INT(msg.minute, 45, "minute");
    ASSERT_EQ_STR(msg.callsign, "KHOU/NWS", "callsign");
    ASSERT_EQ_INT(msg.is_eom, 0, "not eom");
}

static void test_same_parse_eom(void)
{
    same_message_t msg;
    int r = same_parse(&msg, "NNNN");
    ASSERT_EQ_INT(r, 0, "parse NNNN");
    ASSERT_EQ_INT(msg.is_eom, 1, "is_eom flag");
}

static void test_same_parse_single_fips(void)
{
    same_message_t msg;
    int r = same_parse(&msg, "ZCZC-CIV-EWW-029165+0030-0911200-KFSD    -");
    ASSERT_EQ_INT(r, 0, "parse single fips");
    ASSERT_EQ_STR(msg.originator, "CIV", "org CIV");
    ASSERT_EQ_STR(msg.event, "EWW", "event EWW");
    ASSERT_EQ_INT(msg.num_fips, 1, "one fips");
    ASSERT_EQ_STR(msg.fips[0], "029165", "fips value");
    ASSERT_EQ_INT(msg.duration_secs, 1800, "30 min");
}

static void test_same_parse_malformed(void)
{
    same_message_t msg;
    ASSERT_EQ_INT(same_parse(&msg, "GARBAGE"), -1, "reject garbage");
    ASSERT_EQ_INT(same_parse(&msg, "ZCZC"), -1, "reject truncated");
    ASSERT_EQ_INT(same_parse(&msg, "ZCZC-"), -1, "reject no org");
}

static void test_same_match_fips_exact(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-048453-048491+0100-1411545-KHOU/NWS-");

    char fips[][SAME_FIPS_LEN + 1] = {"048453", "048201"};
    ASSERT_EQ_INT(same_match_fips(&msg, fips, 2), 1, "exact match 048453");
}

static void test_same_match_fips_pdigit_strip(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-148453+0100-1411545-KHOU/NWS-");

    char fips[][SAME_FIPS_LEN + 1] = {"048453"};
    ASSERT_EQ_INT(same_match_fips(&msg, fips, 1), 1,
                  "P-digit stripped: 148453 matches 048453");
}

static void test_same_match_fips_national(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-PEP-EAN-000000+0600-1411545-KEAX/NWS-");

    char fips[][SAME_FIPS_LEN + 1] = {"048453"};
    ASSERT_EQ_INT(same_match_fips(&msg, fips, 1), 1,
                  "national alert 000000 matches any");
}

static void test_same_match_fips_no_match(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-048453+0100-1411545-KHOU/NWS-");

    char fips[][SAME_FIPS_LEN + 1] = {"029165", "017001"};
    ASSERT_EQ_INT(same_match_fips(&msg, fips, 2), 0, "no match");
}

static void test_same_match_fips_empty_filter(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-048453+0100-1411545-KHOU/NWS-");

    char fips[][SAME_FIPS_LEN + 1] = {""};
    ASSERT_EQ_INT(same_match_fips(&msg, fips, 0), 1,
                  "empty filter matches all");
}

static void test_same_format_json(void)
{
    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-SVR-048453+0045-1411545-KHOU/NWS-");

    char buf[512];
    same_format_json(&msg, buf, sizeof(buf));
    ASSERT(strstr(buf, "\"event\":\"SVR\"") != NULL, "json has event");
    ASSERT(strstr(buf, "\"org\":\"WXR\"") != NULL, "json has org");
    ASSERT(strstr(buf, "\"048453\"") != NULL, "json has fips");
}

int main(void)
{
    fprintf(stderr, "test_same:\n");
    RUN_TEST(test_same_parse_valid);
    RUN_TEST(test_same_parse_eom);
    RUN_TEST(test_same_parse_single_fips);
    RUN_TEST(test_same_parse_malformed);
    RUN_TEST(test_same_match_fips_exact);
    RUN_TEST(test_same_match_fips_pdigit_strip);
    RUN_TEST(test_same_match_fips_national);
    RUN_TEST(test_same_match_fips_no_match);
    RUN_TEST(test_same_match_fips_empty_filter);
    RUN_TEST(test_same_format_json);
    TEST_SUMMARY();
}
