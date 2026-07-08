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
 * log.h - Structured logging for weather-usrp
 *
 * Levels: ERROR, WARN, INFO, DEBUG, TRACE
 * Controlled via CLI: -v (INFO), -vv (DEBUG), -vvv (TRACE)
 * Default (no flag): WARN and above
 *
 * Usage:
 *   LOG_INFO("gate", "ch%d state %s -> %s", ch, old, new);
 *   LOG_DEBUG("eas", "ch%d preamble count=%d", ch, cnt);
 *   LOG_TRACE("dsp", "ch%d power=%.2f", ch, pwr);
 */

#ifndef LOG_H
#define LOG_H

#include <stdio.h>

typedef enum {
    LOG_LVL_ERROR = 0,
    LOG_LVL_WARN,
    LOG_LVL_INFO,
    LOG_LVL_DEBUG,
    LOG_LVL_TRACE
} log_level_t;

/*
 * Initialize logging subsystem.
 * verbosity: 0 = WARN+ERROR only, 1 = +INFO, 2 = +DEBUG, 3 = +TRACE
 */
void log_init(int verbosity);

/*
 * Get current log level (for guarding expensive format operations).
 */
log_level_t log_get_level(void);

/*
 * Core log function. Use the macros below instead.
 */
void log_msg(log_level_t level, const char *module, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/* Convenience macros */
#define LOG_ERROR(mod, ...) log_msg(LOG_LVL_ERROR, mod, __VA_ARGS__)
#define LOG_WARN(mod, ...)  log_msg(LOG_LVL_WARN,  mod, __VA_ARGS__)
#define LOG_INFO(mod, ...)  log_msg(LOG_LVL_INFO,  mod, __VA_ARGS__)
#define LOG_DEBUG(mod, ...) log_msg(LOG_LVL_DEBUG, mod, __VA_ARGS__)
#define LOG_TRACE(mod, ...) log_msg(LOG_LVL_TRACE, mod, __VA_ARGS__)

/* Guard macro for expensive trace/debug operations */
#define LOG_ENABLED(lvl) (log_get_level() >= (lvl))

#endif /* LOG_H */
