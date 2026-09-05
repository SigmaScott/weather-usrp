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
 * same.h - SAME message parser and FIPS code matcher
 *
 * Parses the ZCZC-ORG-EEE-PSSCCC-PSSCCC+TTTT-JJJHHMM-CCCCCCCC- format
 * and matches against configured FIPS codes. A configured code with a
 * leading '0' (0SSCCC) matches the whole county (P-digit ignored on both
 * sides); a configured code with a leading '1'-'9' (PSSCCC) matches only
 * that specific sub-area (exact 6-digit match).
 */

#ifndef SAME_H
#define SAME_H

#include "common.h"

/* SAME message event codes (subset of most common) */
#define SAME_EVENT_TOR  "TOR"   /* Tornado Warning */
#define SAME_EVENT_SVR  "SVR"   /* Severe Thunderstorm Warning */
#define SAME_EVENT_FFW  "FFW"   /* Flash Flood Warning */
#define SAME_EVENT_EWW  "EWW"   /* Extreme Wind Warning */
#define SAME_EVENT_RWT  "RWT"   /* Required Weekly Test */
#define SAME_EVENT_RMT  "RMT"   /* Required Monthly Test */
#define SAME_EVENT_EOM  "EOM"   /* End of Message (special - sent as NNNN) */

/* Parsed SAME message */
typedef struct {
    char    originator[4];          /* ORG: WXR, CIV, EAS, PEP */
    char    event[4];               /* EEE: event code */
    char    fips[SAME_MAX_FIPS][SAME_FIPS_LEN + 1]; /* PSSCCC codes */
    int     num_fips;               /* number of FIPS codes in message */
    int     duration_secs;          /* TTTT: purge time in seconds */
    int     julian_day;             /* JJJ */
    int     hour;                   /* HH */
    int     minute;                 /* MM */
    char    callsign[9];            /* CCCCCCCC: station callsign */
    char    raw[EAS_MAX_MSG_LEN + 1]; /* raw message string */
    int     is_eom;                 /* true if this is NNNN (end of message) */
} same_message_t;

/*
 * Parse a raw SAME message string (starting with ZCZC or NNNN).
 * Returns 0 on success, -1 on parse error.
 */
int same_parse(same_message_t *msg, const char *raw);

/*
 * Check if a parsed SAME message matches any of the given FIPS codes.
 * Matching depends on the leading digit of each configured entry:
 *   - '0' prefix (0SSCCC): county-wide. The P-digit is ignored on both
 *     sides and only SSCCC (5 digits) are compared, so 048453 matches
 *     any sub-area of county 48453.
 *   - '1'-'9' prefix (PSSCCC): exact sub-area. All 6 digits must match,
 *     so 148453 matches only sub-area 1 of county 48453.
 * fips_list: array of 6-char FIPS strings to match against
 * num_fips: number of entries in fips_list
 *
 * Returns 1 if match found, 0 if no match.
 * National alerts (FIPS 000000) always match.
 */
int same_match_fips(const same_message_t *msg,
                    const char fips_list[][SAME_FIPS_LEN + 1],
                    int num_fips);

/*
 * Check if a SAME message's event code is in the blacklist.
 * blacklist: array of 3-char event code strings
 * num_blacklist: number of entries in blacklist
 *
 * Returns 1 if event IS blacklisted (should be rejected), 0 if allowed.
 * Empty blacklist (num_blacklist == 0) allows all events.
 */
int same_event_blacklisted(const same_message_t *msg,
                           const char blacklist[][SAME_EVENT_LEN + 1],
                           int num_blacklist);

/*
 * Format a SAME message as a human-readable string.
 * buf: output buffer
 * buflen: buffer size
 * Returns number of bytes written.
 */
int same_format(const same_message_t *msg, char *buf, size_t buflen);

/*
 * Format a SAME message as JSON.
 * buf: output buffer
 * buflen: buffer size
 * Returns number of bytes written.
 */
int same_format_json(const same_message_t *msg, char *buf, size_t buflen);

#endif /* SAME_H */
