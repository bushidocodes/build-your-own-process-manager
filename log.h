/*
 * log.h — tiny JSON structured logger (C replacement for logger.ts)
 *
 * Each call emits one JSON object per line to stdout, e.g.
 *   {"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"serve","path":"index.html","status":200}
 *
 * The minimum level is controlled by the LOG_LEVEL environment variable
 * (DEBUG | INFO | WARN | ERROR), defaulting to INFO.
 */
#ifndef LOG_H
#define LOG_H

#include <stddef.h> /* size_t */

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR } log_level_t;

/*
 * Emit a log line. `fields`, if non-NULL, is a fragment of additional JSON that
 * is spliced in before the closing brace; it MUST start with a comma, e.g.
 *   log_emit(LOG_INFO, "serve", ",\"path\":\"index.html\",\"status\":200");
 * Pass NULL (or "") when there are no extra fields. Lines below the configured
 * LOG_LEVEL are dropped.
 */
void log_emit(log_level_t level, const char *msg, const char *fields);

/*
 * JSON-escape `src` into `dst` (writing at most dstsize-1 chars plus a NUL).
 * Use this for dynamic field values such as request paths before embedding
 * them in a `fields` fragment. Returns `dst` for convenience.
 */
char *log_json_escape(char *dst, size_t dstsize, const char *src);

#endif /* LOG_H */
