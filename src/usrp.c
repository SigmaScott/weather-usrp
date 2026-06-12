#include "usrp.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int usrp_init(usrp_conn_t *conn, const char *host, uint16_t port)
{
    memset(conn, 0, sizeof(*conn));

    conn->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (conn->sockfd < 0) {
        perror("usrp: socket");
        return -1;
    }

    conn->dest_addr.sin_family = AF_INET;
    conn->dest_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &conn->dest_addr.sin_addr) <= 0) {
        fprintf(stderr, "usrp: invalid host %s\n", host);
        close(conn->sockfd);
        conn->sockfd = -1;
        return -1;
    }

    conn->seq = 0;
    conn->keyup = 0;
    return 0;
}

int usrp_send_audio(usrp_conn_t *conn, const int16_t *samples, int keyup)
{
    uint8_t frame[USRP_FRAME_SIZE];
    memset(frame, 0, USRP_HEADER_SIZE);

    /* Header: "USRP" magic */
    memcpy(frame, USRP_MAGIC, 4);

    /* Sequence number (big-endian) */
    uint32_t seq_be = htonl(conn->seq++);
    memcpy(frame + 4, &seq_be, 4);

    /* Memory (unused, 0) at offset 8 */

    /* Keyup at offset 12 */
    uint32_t keyup_be = htonl((uint32_t)keyup);
    memcpy(frame + 12, &keyup_be, 4);

    /* Talkgroup at offset 16 (0) */
    /* Type at offset 20 (0 = voice) */
    /* Mpxid at offset 24 (0) */
    /* Reserved at offset 28 (0) */

    /* Audio payload: 160 samples, int16 little-endian */
    for (int i = 0; i < USRP_SAMPLES; i++) {
        int16_t s = samples ? samples[i] : 0;
        frame[USRP_HEADER_SIZE + i * 2] = (uint8_t)(s & 0xFF);
        frame[USRP_HEADER_SIZE + i * 2 + 1] = (uint8_t)((s >> 8) & 0xFF);
    }

    ssize_t sent = sendto(conn->sockfd, frame, USRP_FRAME_SIZE, 0,
                          (struct sockaddr *)&conn->dest_addr,
                          sizeof(conn->dest_addr));
    if (sent != USRP_FRAME_SIZE) {
        perror("usrp: sendto");
        return -1;
    }

    return 0;
}

int usrp_send_text(usrp_conn_t *conn, const char *text)
{
    uint8_t frame[USRP_FRAME_SIZE];
    memset(frame, 0, sizeof(frame));

    memcpy(frame, USRP_MAGIC, 4);

    uint32_t seq_be = htonl(conn->seq++);
    memcpy(frame + 4, &seq_be, 4);

    /* Type = 2 (text) at offset 20 */
    uint32_t type_be = htonl(USRP_TYPE_TEXT);
    memcpy(frame + 20, &type_be, 4);

    /* Text in payload area */
    size_t len = strlen(text);
    if (len > USRP_AUDIO_SIZE - 1)
        len = USRP_AUDIO_SIZE - 1;
    memcpy(frame + USRP_HEADER_SIZE, text, len);

    ssize_t sent = sendto(conn->sockfd, frame, USRP_FRAME_SIZE, 0,
                          (struct sockaddr *)&conn->dest_addr,
                          sizeof(conn->dest_addr));
    if (sent != USRP_FRAME_SIZE)
        return -1;

    return 0;
}

void usrp_close(usrp_conn_t *conn)
{
    if (conn->sockfd >= 0) {
        close(conn->sockfd);
        conn->sockfd = -1;
    }
}
