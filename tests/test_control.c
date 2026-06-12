#include "test.h"
#include "../src/control.h"
#include "../src/gate.h"
#include "../src/same.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

static gate_t test_gates[NUM_CHANNELS];
static gate_t *test_gate_ptrs[NUM_CHANNELS];

static int connect_to_control(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    for (int attempt = 0; attempt < 20; attempt++) {
        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            return fd;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 50000000};
        nanosleep(&ts, NULL);
    }
    return -1;
}

static int read_line(int fd, char *buf, int buflen)
{
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int pos = 0;
    while (pos < buflen - 1) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) break;
        if (c == '\n') break;
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return pos > 0 ? 0 : -1;
}

static void test_control_status(void)
{
    control_t ctl;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channel_config_t cfg = {0};
        strcpy(cfg.usrp_host, "127.0.0.1");
        cfg.usrp_port = (uint16_t)(45100 + i);
        gate_init(&test_gates[i], i, &cfg, NULL, NULL);
        test_gate_ptrs[i] = &test_gates[i];
    }

    ASSERT_EQ_INT(control_init(&ctl, "127.0.0.1", 45050, test_gate_ptrs), 0, "init");
    ASSERT_EQ_INT(control_start(&ctl), 0, "start");

    int client = connect_to_control(45050);
    ASSERT(client >= 0, "connected to control");

    ssize_t wr = write(client, "STATUS\n", 7);
    (void)wr;
    char buf[1024];
    int r = read_line(client, buf, sizeof(buf));
    ASSERT_EQ_INT(r, 0, "got status response");
    ASSERT(strstr(buf, "channels") != NULL, "status has channels");

    close(client);
    control_stop(&ctl);

    for (int i = 0; i < NUM_CHANNELS; i++)
        gate_close(&test_gates[i]);
}

static void test_control_passthrough(void)
{
    control_t ctl;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channel_config_t cfg = {0};
        strcpy(cfg.usrp_host, "127.0.0.1");
        cfg.usrp_port = (uint16_t)(45110 + i);
        gate_init(&test_gates[i], i, &cfg, NULL, NULL);
        test_gate_ptrs[i] = &test_gates[i];
    }

    control_init(&ctl, "127.0.0.1", 45051, test_gate_ptrs);
    control_start(&ctl);

    int client = connect_to_control(45051);
    ASSERT(client >= 0, "connected");

    ssize_t wr;
    wr = write(client, "PASSTHROUGH 2 ON\n", 17);
    (void)wr;
    char buf[256];
    read_line(client, buf, sizeof(buf));
    ASSERT(strstr(buf, "OK") != NULL, "passthrough on OK");
    ASSERT_EQ_INT(gate_get_state(&test_gates[2]), GATE_PASSTHROUGH,
                  "gate 2 is passthrough");

    wr = write(client, "PASSTHROUGH 2 OFF\n", 18);
    (void)wr;
    read_line(client, buf, sizeof(buf));
    ASSERT(strstr(buf, "OK") != NULL, "passthrough off OK");
    ASSERT_EQ_INT(gate_get_state(&test_gates[2]), GATE_IDLE,
                  "gate 2 back to idle");

    close(client);
    control_stop(&ctl);

    for (int i = 0; i < NUM_CHANNELS; i++)
        gate_close(&test_gates[i]);
}

static void test_control_bad_command(void)
{
    control_t ctl;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channel_config_t cfg = {0};
        strcpy(cfg.usrp_host, "127.0.0.1");
        cfg.usrp_port = (uint16_t)(45120 + i);
        gate_init(&test_gates[i], i, &cfg, NULL, NULL);
        test_gate_ptrs[i] = &test_gates[i];
    }

    control_init(&ctl, "127.0.0.1", 45052, test_gate_ptrs);
    control_start(&ctl);

    int client = connect_to_control(45052);
    ASSERT(client >= 0, "connected");

    ssize_t wr;
    wr = write(client, "NONSENSE\n", 9);
    (void)wr;
    char buf[256];
    read_line(client, buf, sizeof(buf));
    ASSERT(strstr(buf, "ERR") != NULL, "bad command returns ERR");

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    nanosleep(&ts, NULL);

    wr = write(client, "PASSTHROUGH 99 ON\n", 18);
    (void)wr;
    read_line(client, buf, sizeof(buf));
    ASSERT(strstr(buf, "ERR") != NULL, "bad channel returns ERR");

    close(client);
    control_stop(&ctl);

    for (int i = 0; i < NUM_CHANNELS; i++)
        gate_close(&test_gates[i]);
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);
    fprintf(stderr, "test_control:\n");
    RUN_TEST(test_control_status);
    RUN_TEST(test_control_passthrough);
    RUN_TEST(test_control_bad_command);
    TEST_SUMMARY();
}
