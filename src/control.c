#include "control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CMD_LEN 256

static void send_to_client(int fd, const char *msg)
{
    size_t len = strlen(msg);
    ssize_t r = write(fd, msg, len);
    (void)r;
    if (len == 0 || msg[len - 1] != '\n') {
        r = write(fd, "\n", 1);
        (void)r;
    }
}

static void handle_command(control_t *ctl, int client_fd, const char *cmd)
{
    char response[512];

    if (strncmp(cmd, "PASSTHROUGH", 11) == 0) {
        int ch = -1;
        char onoff[8] = {0};
        if (sscanf(cmd + 11, " %d %7s", &ch, onoff) != 2 ||
            ch < 0 || ch >= NUM_CHANNELS) {
            send_to_client(client_fd, "ERR invalid channel");
            return;
        }

        int on = (strcasecmp(onoff, "ON") == 0) ? 1 : 0;
        pthread_mutex_lock(&ctl->lock);
        gate_set_passthrough(ctl->gates[ch], on);
        pthread_mutex_unlock(&ctl->lock);

        snprintf(response, sizeof(response), "OK channel %d passthrough %s",
                 ch, on ? "on" : "off");
        send_to_client(client_fd, response);

    } else if (strncmp(cmd, "STATUS", 6) == 0) {
        char json[2048];
        int pos = 0;
        pos += snprintf(json + pos, sizeof(json) - pos, "{\"channels\":[");
        for (int i = 0; i < NUM_CHANNELS; i++) {
            const char *state_str;
            pthread_mutex_lock(&ctl->lock);
            gate_state_t st = gate_get_state(ctl->gates[i]);
            pthread_mutex_unlock(&ctl->lock);

            switch (st) {
            case GATE_ALERT: state_str = "alert"; break;
            case GATE_PASSTHROUGH: state_str = "passthrough"; break;
            default: state_str = "idle"; break;
            }

            pos += snprintf(json + pos, sizeof(json) - pos,
                     "%s{\"ch\":%d,\"state\":\"%s\"}",
                     i > 0 ? "," : "", i, state_str);
        }
        snprintf(json + pos, sizeof(json) - pos, "]}");
        send_to_client(client_fd, json);

    } else if (strncmp(cmd, "QUIT", 4) == 0) {
        send_to_client(client_fd, "OK bye");

    } else {
        send_to_client(client_fd, "ERR unknown command");
    }
}

static void *control_thread(void *arg)
{
    control_t *ctl = (control_t *)arg;
    fd_set readfds;
    struct timeval tv;

    while (ctl->running) {
        FD_ZERO(&readfds);
        FD_SET(ctl->listen_fd, &readfds);
        int maxfd = ctl->listen_fd;

        for (int i = 0; i < ctl->num_clients; i++) {
            FD_SET(ctl->client_fds[i], &readfds);
            if (ctl->client_fds[i] > maxfd)
                maxfd = ctl->client_fds[i];
        }

        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int ready = select(maxfd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(ctl->listen_fd, &readfds)) {
            struct sockaddr_in addr;
            socklen_t len = sizeof(addr);
            int fd = accept(ctl->listen_fd, (struct sockaddr *)&addr, &len);
            if (fd >= 0) {
                if (ctl->num_clients < 8) {
                    ctl->client_fds[ctl->num_clients++] = fd;
                } else {
                    send_to_client(fd, "ERR max clients reached");
                    close(fd);
                }
            }
        }

        for (int i = 0; i < ctl->num_clients; i++) {
            if (!FD_ISSET(ctl->client_fds[i], &readfds))
                continue;

            char buf[MAX_CMD_LEN];
            ssize_t n = read(ctl->client_fds[i], buf, sizeof(buf) - 1);
            if (n <= 0) {
                close(ctl->client_fds[i]);
                ctl->client_fds[i] = ctl->client_fds[--ctl->num_clients];
                i--;
                continue;
            }

            buf[n] = '\0';
            int disconnected = 0;
            char *line = buf;
            char *nl;
            while ((nl = strchr(line, '\n')) != NULL && !disconnected) {
                *nl = '\0';
                char *cr = strchr(line, '\r');
                if (cr) *cr = '\0';
                if (strlen(line) > 0) {
                    if (strncmp(line, "QUIT", 4) == 0) {
                        send_to_client(ctl->client_fds[i], "OK bye");
                        close(ctl->client_fds[i]);
                        ctl->client_fds[i] = ctl->client_fds[--ctl->num_clients];
                        i--;
                        disconnected = 1;
                    } else {
                        handle_command(ctl, ctl->client_fds[i], line);
                    }
                }
                line = nl + 1;
            }
            if (!disconnected && *line != '\0') {
                char *cr = strchr(line, '\r');
                if (cr) *cr = '\0';
                if (strlen(line) > 0) {
                    if (strncmp(line, "QUIT", 4) == 0) {
                        send_to_client(ctl->client_fds[i], "OK bye");
                        close(ctl->client_fds[i]);
                        ctl->client_fds[i] = ctl->client_fds[--ctl->num_clients];
                        i--;
                    } else {
                        handle_command(ctl, ctl->client_fds[i], line);
                    }
                }
            }
        }
    }

    for (int i = 0; i < ctl->num_clients; i++)
        close(ctl->client_fds[i]);
    ctl->num_clients = 0;

    return NULL;
}

int control_init(control_t *ctl, const char *host, uint16_t port, gate_t **gates)
{
    memset(ctl, 0, sizeof(*ctl));
    strncpy(ctl->host, host, sizeof(ctl->host) - 1);
    ctl->port = port;
    ctl->gates = gates;
    pthread_mutex_init(&ctl->lock, NULL);

    ctl->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctl->listen_fd < 0) {
        perror("control: socket");
        return -1;
    }

    int opt = 1;
    setsockopt(ctl->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
    };
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (bind(ctl->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("control: bind");
        close(ctl->listen_fd);
        return -1;
    }

    if (listen(ctl->listen_fd, 4) < 0) {
        perror("control: listen");
        close(ctl->listen_fd);
        return -1;
    }

    return 0;
}

int control_start(control_t *ctl)
{
    ctl->running = 1;
    if (pthread_create(&ctl->thread, NULL, control_thread, ctl) != 0) {
        perror("control: pthread_create");
        ctl->running = 0;
        return -1;
    }
    return 0;
}

void control_broadcast(control_t *ctl, const char *msg)
{
    pthread_mutex_lock(&ctl->lock);
    for (int i = 0; i < ctl->num_clients; i++)
        send_to_client(ctl->client_fds[i], msg);
    pthread_mutex_unlock(&ctl->lock);
}

void control_stop(control_t *ctl)
{
    ctl->running = 0;
    pthread_join(ctl->thread, NULL);
    close(ctl->listen_fd);
    pthread_mutex_destroy(&ctl->lock);
}
