#ifndef NITRO_LOG_H
#define NITRO_LOG_H

#include "common.h"

#define NITRO_LOG_CRIT_PFX (isatty(2) ? "\x1b[31m" : "")
#define NITRO_LOG_WARN_PFX (isatty(2) ? "\x1b[33m" : "")
#define NITRO_LOG_OK_PFX (isatty(2) ? "\x1b[32m" : "")
#define NITRO_LOG_CLR_SFX (isatty(2) ? "\x1b[0m" : "")

#define __nitro_log(lev, sys, ...) {\
    char __nitro_log_buf1[1000];\
    char __nitro_log_buf2[1050];\
    char __nitro_log_date[50];\
    struct timeval __nitro_log_tv;\
    struct tm __nitro_log_tm;\
    gettimeofday(&__nitro_log_tv, NULL);\
    gmtime_r(&__nitro_log_tv.tv_sec, &__nitro_log_tm);\
    strftime(__nitro_log_date, sizeof(__nitro_log_date), "%Y-%m-%d %H:%M:%S", &__nitro_log_tm);\
    snprintf(__nitro_log_buf1, sizeof(__nitro_log_buf1), __VA_ARGS__);\
    int __nitro_log_fmt_written = snprintf(__nitro_log_buf2, sizeof(__nitro_log_buf2), "[%s.%03d] %s{%s:%s}%s %s\n",\
        __nitro_log_date, (int)(__nitro_log_tv.tv_usec / 1000),\
        (lev == 2 ? NITRO_LOG_CRIT_PFX : (lev == 1 ? NITRO_LOG_WARN_PFX : NITRO_LOG_OK_PFX)),\
        sys, (lev == 2 ? "ERR" : (lev == 1 ? "WARN" : "INFO")),\
        NITRO_LOG_CLR_SFX, __nitro_log_buf1);\
    (void)(write(2, __nitro_log_buf2, __nitro_log_fmt_written >= sizeof(__nitro_log_buf2) ? sizeof(__nitro_log_buf2) : __nitro_log_fmt_written)+1);\
}

/* The write call above in __nitro_log has a hack with the (void)(write(...)+1)
 * cast to ignore the return value of write in a way that doesn't anger gcc
 * 4.6+.
 */

#define nitro_log_info(...) __nitro_log(0, __VA_ARGS__)
#define nitro_log_warn(...) __nitro_log(1, __VA_ARGS__)
#define nitro_log_warning(...) __nitro_log(1, __VA_ARGS__)
#define nitro_log_err(...) __nitro_log(2, __VA_ARGS__)
#define nitro_log_error(...) __nitro_log(2, __VA_ARGS__)

#endif /* NITRO_LOG_H */
