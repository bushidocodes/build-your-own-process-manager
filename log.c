/*
 * log.c — see log.h. C replacement for logger.ts.
 */
#define _POSIX_C_SOURCE 200809L

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *LEVEL_NAMES[] = {"DEBUG", "INFO", "WARN", "ERROR"};

/* Resolve (and cache) the minimum level from LOG_LEVEL, defaulting to INFO. */
static log_level_t min_level(void)
{
    static int resolved = 0;
    static log_level_t level = LOG_INFO;
    if (resolved) {
        return level;
    }

    const char *env = getenv("LOG_LEVEL");
    if (env) {
        for (int i = 0; i < (int)(sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]));
             i++) {
            if (strcmp(env, LEVEL_NAMES[i]) == 0) {
                level = (log_level_t)i;
                break;
            }
        }
    }
    resolved = 1;
    return level;
}

/* Format the current UTC time like JS `new Date().toISOString()`. */
static void iso_timestamp(char *buf, size_t size)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm tm_utc;
    gmtime_r(&tv.tv_sec, &tm_utc);

    char secs[sizeof("2026-01-01T12:00:00")];
    strftime(secs, sizeof(secs), "%Y-%m-%dT%H:%M:%S", &tm_utc);
    snprintf(buf, size, "%s.%03dZ", secs, (int)(tv.tv_usec / 1000));
}

char *log_json_escape(char *dst, size_t dstsize, const char *src)
{
    size_t w = 0;
    for (const char *p = src; *p && w + 2 < dstsize; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
        case '"':
            dst[w++] = '\\';
            dst[w++] = '"';
            break;
        case '\\':
            dst[w++] = '\\';
            dst[w++] = '\\';
            break;
        case '\n':
            dst[w++] = '\\';
            dst[w++] = 'n';
            break;
        case '\r':
            dst[w++] = '\\';
            dst[w++] = 'r';
            break;
        case '\t':
            dst[w++] = '\\';
            dst[w++] = 't';
            break;
        default:
            if (c < 0x20) {
                /* Control character: emit \u00XX (needs 6 bytes). */
                if (w + 6 >= dstsize) {
                    goto done;
                }
                w += (size_t)snprintf(dst + w, dstsize - w, "\\u%04x", c);
            } else {
                dst[w++] = (char)c;
            }
            break;
        }
    }
done:
    dst[w] = '\0';
    return dst;
}

void log_emit(log_level_t level, const char *msg, const char *fields)
{
    if (level < min_level()) {
        return;
    }

    char ts[sizeof("2026-01-01T12:00:00.000Z")];
    iso_timestamp(ts, sizeof(ts));

    char msg_esc[512];
    log_json_escape(msg_esc, sizeof(msg_esc), msg);

    printf("{\"ts\":\"%s\",\"level\":\"%s\",\"msg\":\"%s\"%s}\n", ts,
           LEVEL_NAMES[level], msg_esc, fields ? fields : "");
    fflush(stdout);
}
