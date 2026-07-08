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

#include "log.h"
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

static log_level_t current_level = LOG_LVL_WARN;

static const char *level_names[] = {
    "ERROR", "WARN ", "INFO ", "DEBUG", "TRACE"
};

void log_init(int verbosity)
{
    switch (verbosity) {
    case 0:  current_level = LOG_LVL_WARN;  break;
    case 1:  current_level = LOG_LVL_INFO;  break;
    case 2:  current_level = LOG_LVL_DEBUG; break;
    default: current_level = LOG_LVL_TRACE; break;
    }
}

log_level_t log_get_level(void)
{
    return current_level;
}

void log_msg(log_level_t level, const char *module, const char *fmt, ...)
{
    if (level > current_level)
        return;

    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);

    fprintf(stderr, "%02d:%02d:%02d.%03ld [%s] %s: ",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            tv.tv_usec / 1000,
            level_names[level], module);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
}
