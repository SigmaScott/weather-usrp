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

#include "same.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

int same_parse(same_message_t *msg, const char *raw)
{
    memset(msg, 0, sizeof(*msg));
    strncpy(msg->raw, raw, EAS_MAX_MSG_LEN);

    if (strncmp(raw, "NNNN", 4) == 0) {
        msg->is_eom = 1;
        return 0;
    }

    if (strncmp(raw, "ZCZC-", 5) != 0)
        return -1;

    const char *p = raw + 5;

    /* ORG (3 chars) */
    if (strlen(p) < 4 || p[3] != '-')
        return -1;
    memcpy(msg->originator, p, 3);
    p += 4;

    /* EEE (3 chars) */
    if (strlen(p) < 4 || p[3] != '-')
        return -1;
    memcpy(msg->event, p, 3);
    p += 4;

    /* PSSCCC codes (one or more, dash-separated, terminated by +) */
    msg->num_fips = 0;
    while (*p && *p != '+' && msg->num_fips < SAME_MAX_FIPS) {
        if (strlen(p) < SAME_FIPS_LEN)
            break;
        memcpy(msg->fips[msg->num_fips], p, SAME_FIPS_LEN);
        msg->fips[msg->num_fips][SAME_FIPS_LEN] = '\0';
        msg->num_fips++;
        p += SAME_FIPS_LEN;
        if (*p == '-')
            p++;
    }

    /* +TTTT (duration in 15-min increments as 4-digit HHMM) */
    if (*p == '+') {
        p++;
        if (strlen(p) >= 4) {
            int hh = (p[0] - '0') * 10 + (p[1] - '0');
            int mm = (p[2] - '0') * 10 + (p[3] - '0');
            msg->duration_secs = hh * 3600 + mm * 60;
            p += 4;
        }
    }

    /* -JJJHHMM */
    if (*p == '-') {
        p++;
        if (strlen(p) >= 7) {
            msg->julian_day = (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
            msg->hour = (p[3] - '0') * 10 + (p[4] - '0');
            msg->minute = (p[5] - '0') * 10 + (p[6] - '0');
            p += 7;
        }
    }

    /* -CCCCCCCC- (callsign, up to 8 chars) */
    if (*p == '-') {
        p++;
        int i = 0;
        while (*p && *p != '-' && i < 8) {
            msg->callsign[i++] = *p++;
        }
        msg->callsign[i] = '\0';
    }

    return 0;
}

int same_match_fips(const same_message_t *msg,
                    const char fips_list[][SAME_FIPS_LEN + 1],
                    int num_fips)
{
    if (num_fips == 0)
        return 1;

    for (int i = 0; i < msg->num_fips; i++) {
        /* National alert (000000) always matches */
        if (strcmp(msg->fips[i] + 1, "00000") == 0)
            return 1;

        /* Strip P-digit: compare only SSCCC (last 5 chars) */
        const char *msg_ssccc = msg->fips[i] + 1;

        for (int j = 0; j < num_fips; j++) {
            const char *cfg_ssccc = fips_list[j] + 1;
            if (strncmp(msg_ssccc, cfg_ssccc, 5) == 0)
                return 1;
        }
    }

    return 0;
}

int same_event_blacklisted(const same_message_t *msg,
                           const char blacklist[][SAME_EVENT_LEN + 1],
                           int num_blacklist)
{
    if (num_blacklist == 0)
        return 0;

    for (int i = 0; i < num_blacklist; i++) {
        if (strncmp(msg->event, blacklist[i], SAME_EVENT_LEN) == 0)
            return 1;
    }

    return 0;
}

int same_format(const same_message_t *msg, char *buf, size_t buflen)
{
    if (msg->is_eom)
        return snprintf(buf, buflen, "EOM (End of Message)");

    return snprintf(buf, buflen, "%s %s from %s [%d county/zone%s] valid %dh%02dm",
                    msg->originator, msg->event, msg->callsign,
                    msg->num_fips, msg->num_fips != 1 ? "s" : "",
                    msg->duration_secs / 3600, (msg->duration_secs % 3600) / 60);
}

int same_format_json(const same_message_t *msg, char *buf, size_t buflen)
{
    if (msg->is_eom)
        return snprintf(buf, buflen, "{\"type\":\"eom\"}");

    int n = snprintf(buf, buflen,
        "{\"org\":\"%s\",\"event\":\"%s\",\"fips\":[",
        msg->originator, msg->event);

    for (int i = 0; i < msg->num_fips && (size_t)n < buflen - 20; i++) {
        n += snprintf(buf + n, buflen - n, "%s\"%s\"",
                      i ? "," : "", msg->fips[i]);
    }

    n += snprintf(buf + n, buflen - n,
        "],\"duration\":%d,\"day\":%d,\"time\":\"%02d%02d\","
        "\"callsign\":\"%s\",\"raw\":\"%s\"}",
        msg->duration_secs, msg->julian_day,
        msg->hour, msg->minute,
        msg->callsign, msg->raw);

    return n;
}
