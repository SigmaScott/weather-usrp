/*
 * usrp.h - USRP protocol framer and UDP sender
 */

#ifndef USRP_H
#define USRP_H

#include "common.h"
#include <netinet/in.h>

/* USRP connection state */
typedef struct {
    int                 sockfd;
    struct sockaddr_in  dest_addr;
    uint32_t            seq;
    uint32_t            keyup;
} usrp_conn_t;

/*
 * Initialize USRP connection.
 * Returns 0 on success, -1 on error.
 */
int usrp_init(usrp_conn_t *conn, const char *host, uint16_t port);

/*
 * Send one USRP audio frame (160 samples of int16 @ 8kHz).
 * keyup: 1 = PTT active (audio present), 0 = idle
 * Returns 0 on success, -1 on error.
 */
int usrp_send_audio(usrp_conn_t *conn, const int16_t *samples, int keyup);

/*
 * Send USRP text metadata frame.
 * Returns 0 on success, -1 on error.
 */
int usrp_send_text(usrp_conn_t *conn, const char *text);

/*
 * Close USRP connection.
 */
void usrp_close(usrp_conn_t *conn);

#endif /* USRP_H */
