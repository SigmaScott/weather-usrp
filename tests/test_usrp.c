#include "test.h"
#include "../src/usrp.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static int create_udp_receiver(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    return fd;
}

static void test_usrp_header_format(void)
{
    int rx = create_udp_receiver(44999);
    usrp_conn_t conn;
    ASSERT_EQ_INT(usrp_init(&conn, "127.0.0.1", 44999), 0, "init conn");

    int16_t samples[160];
    for (int i = 0; i < 160; i++) samples[i] = (int16_t)(i * 100);

    ASSERT_EQ_INT(usrp_send_audio(&conn, samples, 1), 0, "send frame");

    uint8_t buf[512];
    ssize_t n = recv(rx, buf, sizeof(buf), 0);
    ASSERT_EQ_INT((int)n, 352, "frame size 352");

    ASSERT(buf[0] == 'U' && buf[1] == 'S' && buf[2] == 'R' && buf[3] == 'P',
           "magic USRP");

    uint32_t seq = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
    ASSERT_EQ_INT((int)seq, 0, "seq starts at 0");

    uint32_t keyup = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];
    ASSERT_EQ_INT((int)keyup, 1, "keyup=1");

    uint32_t type = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    ASSERT_EQ_INT((int)type, 0, "type=0 voice");

    int16_t s0 = (int16_t)(buf[32] | (buf[33] << 8));
    ASSERT_EQ_INT(s0, 0, "sample[0] = 0");
    int16_t s1 = (int16_t)(buf[34] | (buf[35] << 8));
    ASSERT_EQ_INT(s1, 100, "sample[1] = 100");
    int16_t s159 = (int16_t)(buf[32 + 318] | (buf[32 + 319] << 8));
    ASSERT_EQ_INT(s159, 15900, "sample[159] = 15900");

    usrp_close(&conn);
    close(rx);
}

static void test_usrp_sequence_increments(void)
{
    int rx = create_udp_receiver(44998);
    usrp_conn_t conn;
    usrp_init(&conn, "127.0.0.1", 44998);

    int16_t silence[160] = {0};
    usrp_send_audio(&conn, silence, 0);
    usrp_send_audio(&conn, silence, 0);
    usrp_send_audio(&conn, silence, 0);

    uint8_t buf[352];
    recv(rx, buf, sizeof(buf), 0);
    uint32_t seq0 = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

    recv(rx, buf, sizeof(buf), 0);
    uint32_t seq1 = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

    recv(rx, buf, sizeof(buf), 0);
    uint32_t seq2 = (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

    ASSERT_EQ_INT((int)seq0, 0, "seq 0");
    ASSERT_EQ_INT((int)seq1, 1, "seq 1");
    ASSERT_EQ_INT((int)seq2, 2, "seq 2");

    usrp_close(&conn);
    close(rx);
}

static void test_usrp_keyup_zero(void)
{
    int rx = create_udp_receiver(44997);
    usrp_conn_t conn;
    usrp_init(&conn, "127.0.0.1", 44997);

    int16_t silence[160] = {0};
    usrp_send_audio(&conn, silence, 0);

    uint8_t buf[352];
    recv(rx, buf, sizeof(buf), 0);
    uint32_t keyup = (buf[12] << 24) | (buf[13] << 16) | (buf[14] << 8) | buf[15];
    ASSERT_EQ_INT((int)keyup, 0, "keyup=0 when idle");

    usrp_close(&conn);
    close(rx);
}

static void test_usrp_text_frame(void)
{
    int rx = create_udp_receiver(44996);
    usrp_conn_t conn;
    usrp_init(&conn, "127.0.0.1", 44996);

    usrp_send_text(&conn, "HELLO");

    uint8_t buf[352];
    recv(rx, buf, sizeof(buf), 0);

    uint32_t type = (buf[20] << 24) | (buf[21] << 16) | (buf[22] << 8) | buf[23];
    ASSERT_EQ_INT((int)type, 2, "type=2 text");
    ASSERT(memcmp(buf + 32, "HELLO", 5) == 0, "text payload");

    usrp_close(&conn);
    close(rx);
}

int main(void)
{
    fprintf(stderr, "test_usrp:\n");
    RUN_TEST(test_usrp_header_format);
    RUN_TEST(test_usrp_sequence_increments);
    RUN_TEST(test_usrp_keyup_zero);
    RUN_TEST(test_usrp_text_frame);
    TEST_SUMMARY();
}
