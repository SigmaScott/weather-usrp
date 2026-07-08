/*
 * NOAA Weather decoder Allstar Repeater Bridge
 * Copyright (C) 2026 Scott Gillins W2KP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
