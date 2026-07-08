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
 * control.h - TCP text control interface
 *
 * Commands (client -> server):
 *   s                      - request status of all channels (JSON)
 *   p <ch> <1|0>           - enable/disable passthrough on channel (0-6)
 *   q                      - close connection
 *
 * Legacy long-form also accepted: STATUS, PASSTHROUGH <ch> ON|OFF, QUIT
 *
 * Responses (server -> client):
 *   OK <message>           - command accepted
 *   ERR <message>          - command rejected
 *
 * Events (server -> client, unsolicited):
 *   ALERT <json>           - SAME alert detected and matched
 *   EOM <ch>               - End of message on channel
 *   STATE <ch> <state>     - Gate state change
 */

#ifndef CONTROL_H
#define CONTROL_H

#include "common.h"
#include "gate.h"

/* Control server state */
typedef struct {
    int             listen_fd;
    int             client_fds[8];      /* max 8 simultaneous clients */
    int             num_clients;
    pthread_t       thread;
    int             running;
    char            host[64];
    uint16_t        port;

    gate_t          **gates;
    pthread_mutex_t lock;
} control_t;

/*
 * Initialize control server.
 */
int control_init(control_t *ctl, const char *host, uint16_t port, gate_t **gates);

/*
 * Start control server thread (accepts connections, processes commands).
 */
int control_start(control_t *ctl);

/*
 * Broadcast an event string to all connected clients.
 * Thread-safe.
 */
void control_broadcast(control_t *ctl, const char *msg);

/*
 * Stop control server.
 */
void control_stop(control_t *ctl);

#endif /* CONTROL_H */
