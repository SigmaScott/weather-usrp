/*
 * same.h - SAME message parser and FIPS code matcher
 *
 * Parses the ZCZC-ORG-EEE-PSSCCC-PSSCCC+TTTT-JJJHHMM-CCCCCCCC- format
 * and matches against configured FIPS codes (with P-digit stripping).
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
 * Performs P-digit stripping: compares only SSCCC (5 digits).
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
