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
