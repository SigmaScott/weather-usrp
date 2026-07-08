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
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 512

static void trim(char *s)
{
    char *start = s;
    while (isspace((unsigned char)*start)) start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
}

static int parse_fips_list(channel_config_t *ch, const char *val)
{
    char buf[MAX_LINE];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    ch->num_fips = 0;
    char *tok = strtok(buf, ",");
    while (tok && ch->num_fips < SAME_MAX_FIPS) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) *end-- = '\0';
        if (strlen(tok) == SAME_FIPS_LEN) {
            strncpy(ch->fips[ch->num_fips], tok, SAME_FIPS_LEN);
            ch->fips[ch->num_fips][SAME_FIPS_LEN] = '\0';
            ch->num_fips++;
        }
        tok = strtok(NULL, ",");
    }
    return 0;
}

static int parse_event_blacklist(channel_config_t *ch, const char *val)
{
    char buf[MAX_LINE];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    ch->num_event_blacklist = 0;
    char *tok = strtok(buf, ",");
    while (tok && ch->num_event_blacklist < SAME_MAX_BLACKLIST) {
        while (isspace((unsigned char)*tok)) tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && isspace((unsigned char)*end)) *end-- = '\0';
        if (strlen(tok) == SAME_EVENT_LEN) {
            strncpy(ch->event_blacklist[ch->num_event_blacklist], tok, SAME_EVENT_LEN);
            ch->event_blacklist[ch->num_event_blacklist][SAME_EVENT_LEN] = '\0';
            ch->num_event_blacklist++;
        }
        tok = strtok(NULL, ",");
    }
    return 0;
}

int config_load(config_t *cfg, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "config: cannot open %s\n", path);
        return -1;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->gain = -1;
    cfg->center_freq = CAPTURE_CENTER_FREQ;
    cfg->audio_gain = 1.0f;
    cfg->eas_min_bursts = 2;
    strncpy(cfg->control_host, "127.0.0.1", sizeof(cfg->control_host));
    cfg->control_port = 5555;

    for (int i = 0; i < NUM_CHANNELS; i++) {
        cfg->channels[i].frequency = NOAA_FREQ_BASE + i * NOAA_FREQ_SPACING;
        cfg->channels[i].usrp_port = 34001 + i;
        strncpy(cfg->channels[i].usrp_host, "127.0.0.1",
                sizeof(cfg->channels[i].usrp_host));
        cfg->channels[i].enabled = 1;
    }

    char line[MAX_LINE];
    int current_channel = -1;
    enum { SEC_NONE, SEC_SDR, SEC_CONTROL, SEC_CHANNEL } section = SEC_NONE;

    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#' || *p == ';')
            continue;

        if (*p == '[') {
            char *end = strchr(p, ']');
            if (!end) continue;
            *end = '\0';
            p++;

            if (strcmp(p, "sdr") == 0) {
                section = SEC_SDR;
            } else if (strcmp(p, "control") == 0) {
                section = SEC_CONTROL;
            } else if (strncmp(p, "channel", 7) == 0) {
                section = SEC_CHANNEL;
                current_channel = atoi(p + 7);
                if (current_channel < 0 || current_channel >= NUM_CHANNELS) {
                    fprintf(stderr, "config: invalid channel %d\n", current_channel);
                    current_channel = -1;
                    section = SEC_NONE;
                }
            }
            continue;
        }

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        trim(key);
        trim(val);

        switch (section) {
        case SEC_SDR:
            if (strcmp(key, "device_index") == 0)
                cfg->device_index = (uint32_t)atoi(val);
            else if (strcmp(key, "gain") == 0)
                cfg->gain = atoi(val);
            else if (strcmp(key, "ppm") == 0)
                cfg->ppm = atoi(val);
            else if (strcmp(key, "center_freq") == 0)
                cfg->center_freq = (uint32_t)strtoul(val, NULL, 10);
            else if (strcmp(key, "audio_gain") == 0)
                cfg->audio_gain = (float)atof(val);
            else if (strcmp(key, "eas_min_bursts") == 0) {
                int v = atoi(val);
                if (v >= 1 && v <= 3) cfg->eas_min_bursts = v;
            }
            break;

        case SEC_CONTROL:
            if (strcmp(key, "host") == 0)
                strncpy(cfg->control_host, val, sizeof(cfg->control_host) - 1);
            else if (strcmp(key, "port") == 0)
                cfg->control_port = (uint16_t)atoi(val);
            break;

        case SEC_CHANNEL:
            if (current_channel < 0) break;
            if (strcmp(key, "frequency") == 0)
                cfg->channels[current_channel].frequency = atof(val);
            else if (strcmp(key, "usrp_host") == 0)
                strncpy(cfg->channels[current_channel].usrp_host, val,
                        sizeof(cfg->channels[current_channel].usrp_host) - 1);
            else if (strcmp(key, "usrp_port") == 0)
                cfg->channels[current_channel].usrp_port = (uint16_t)atoi(val);
            else if (strcmp(key, "fips") == 0)
                parse_fips_list(&cfg->channels[current_channel], val);
            else if (strcmp(key, "event_blacklist") == 0)
                parse_event_blacklist(&cfg->channels[current_channel], val);
            else if (strcmp(key, "enabled") == 0)
                cfg->channels[current_channel].enabled = atoi(val);
            break;

        case SEC_NONE:
            break;
        }
    }

    fclose(f);
    return 0;
}

void config_dump(const config_t *cfg)
{
    printf("SDR: device=%u gain=%d ppm=%d center_freq=%.6f MHz audio_gain=%.1f\n",
           cfg->device_index, cfg->gain, cfg->ppm, cfg->center_freq / 1e6,
           cfg->audio_gain);
    printf("Control: %s:%u\n", cfg->control_host, cfg->control_port);
    for (int i = 0; i < NUM_CHANNELS; i++) {
        const channel_config_t *ch = &cfg->channels[i];
        printf("Channel %d: freq=%.6f MHz usrp=%s:%u enabled=%d fips=",
               i, ch->frequency / 1e6, ch->usrp_host, ch->usrp_port, ch->enabled);
        for (int j = 0; j < ch->num_fips; j++)
            printf("%s%s", j ? "," : "", ch->fips[j]);
        if (ch->num_event_blacklist > 0) {
            printf(" blacklist=");
            for (int j = 0; j < ch->num_event_blacklist; j++)
                printf("%s%s", j ? "," : "", ch->event_blacklist[j]);
        }
        printf("\n");
    }
}
