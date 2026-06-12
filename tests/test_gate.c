#include "test.h"
#include "../src/gate.h"
#include "../src/same.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

static int create_rx(uint16_t port);

static char last_event[512];
static int event_count;

static void test_gate_event(int channel, const char *event, void *userdata)
{
    (void)channel;
    (void)userdata;
    strncpy(last_event, event, sizeof(last_event) - 1);
    event_count++;
}

static void test_gate_idle_discards_audio(void)
{
    int rx = create_rx(44990);

    channel_config_t cfg = {0};
    strcpy(cfg.usrp_host, "127.0.0.1");
    cfg.usrp_port = 44990;

    gate_t gate;
    gate_init(&gate, 0, &cfg, test_gate_event, NULL);

    int16_t audio[320];
    for (int i = 0; i < 320; i++) audio[i] = 1000;
    gate_process_audio(&gate, audio, 320);

    struct timeval tv = {.tv_sec = 0, .tv_usec = 50000};
    setsockopt(rx, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    uint8_t buf[352];
    ssize_t n = recv(rx, buf, sizeof(buf), 0);
    ASSERT(n <= 0, "IDLE state sends no packets");

    gate_close(&gate);
    close(rx);
}

static void test_gate_alert_forwards(void)
{
    int rx = create_rx(44989);

    channel_config_t cfg = {0};
    strcpy(cfg.usrp_host, "127.0.0.1");
    cfg.usrp_port = 44989;

    gate_t gate;
    gate_init(&gate, 0, &cfg, test_gate_event, NULL);
    event_count = 0;

    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-048453+0100-1411545-KHOU/NWS-");
    gate_alert(&gate, &msg);

    ASSERT_EQ_INT(gate_get_state(&gate), GATE_ALERT, "state is ALERT");
    ASSERT(event_count > 0, "event fired on alert");

    int16_t audio[160];
    for (int i = 0; i < 160; i++) audio[i] = 5000;
    gate_process_audio(&gate, audio, 160);

    uint8_t buf[352];
    ssize_t n = recv(rx, buf, sizeof(buf), 0);
    ASSERT_EQ_INT((int)n, 352, "ALERT state sends USRP packet");

    uint32_t keyup = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];
    ASSERT_EQ_INT((int)keyup, 1, "keyup=1 during alert");

    gate_close(&gate);
    close(rx);
}

static void test_gate_eom_releases(void)
{
    int rx = create_rx(44988);

    channel_config_t cfg = {0};
    strcpy(cfg.usrp_host, "127.0.0.1");
    cfg.usrp_port = 44988;

    gate_t gate;
    gate_init(&gate, 0, &cfg, test_gate_event, NULL);

    same_message_t msg;
    same_parse(&msg, "ZCZC-WXR-TOR-048453+0100-1411545-KHOU/NWS-");
    gate_alert(&gate, &msg);

    /* Send partial frame so EOM has something to flush */
    int16_t audio[80];
    for (int i = 0; i < 80; i++) audio[i] = 2000;
    gate_process_audio(&gate, audio, 80);

    gate_eom(&gate);
    ASSERT_EQ_INT(gate_get_state(&gate), GATE_IDLE, "EOM returns to IDLE");

    uint8_t buf[352];
    ssize_t n = recv(rx, buf, sizeof(buf), 0);
    ASSERT(n > 0, "EOM sends flush frame");

    uint32_t keyup = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];
    ASSERT_EQ_INT((int)keyup, 1, "flush frame has keyup=1");

    n = recv(rx, buf, sizeof(buf), 0);
    if (n > 0) {
        keyup = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];
        ASSERT_EQ_INT((int)keyup, 0, "final frame has keyup=0");
    }

    gate_close(&gate);
    close(rx);
}

static void test_gate_passthrough(void)
{
    int rx = create_rx(44987);

    channel_config_t cfg = {0};
    strcpy(cfg.usrp_host, "127.0.0.1");
    cfg.usrp_port = 44987;

    gate_t gate;
    gate_init(&gate, 0, &cfg, test_gate_event, NULL);

    gate_set_passthrough(&gate, 1);
    ASSERT_EQ_INT(gate_get_state(&gate), GATE_PASSTHROUGH, "passthrough on");

    int16_t audio[160];
    for (int i = 0; i < 160; i++) audio[i] = 3000;
    gate_process_audio(&gate, audio, 160);

    uint8_t buf[352];
    ssize_t n = recv(rx, buf, sizeof(buf), 0);
    ASSERT_EQ_INT((int)n, 352, "passthrough sends packets");

    gate_set_passthrough(&gate, 0);
    ASSERT_EQ_INT(gate_get_state(&gate), GATE_IDLE, "passthrough off -> IDLE");

    gate_close(&gate);
    close(rx);
}

static int create_rx(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    struct timeval tv = {.tv_sec = 0, .tv_usec = 100000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return fd;
}

int main(void)
{
    fprintf(stderr, "test_gate:\n");
    RUN_TEST(test_gate_idle_discards_audio);
    RUN_TEST(test_gate_alert_forwards);
    RUN_TEST(test_gate_eom_releases);
    RUN_TEST(test_gate_passthrough);
    TEST_SUMMARY();
}
