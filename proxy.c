/*
 * proxy.c — reverse proxy with failover (C replacement for proxy.ts)
 *
 * Accepts connections on PROXY_PORT (default 1337) and forwards each request to
 * a randomly chosen worker from WORKER_PORTS (default "8080,8081,8082") on
 * localhost. If a worker is unreachable, returns a non-200/404 status, or does
 * not respond within BACKEND_TIMEOUT_MS, the proxy fails over to the next
 * worker. If every worker fails, it returns 502 Bad Gateway.
 *
 * Connections are handled one at a time (a simple sequential accept loop).
 */
#include "log.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORKERS 32
#define REQ_BUF_SIZE 8192
#define RELAY_BUF_SIZE 16384
#define BACKEND_TIMEOUT_MS 500

static int worker_ports[MAX_WORKERS];
static int num_workers = 0;

/* Parse a comma-separated "8080,8081,8082" list into worker_ports. */
static void parse_workers(const char *spec)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", spec);
    for (char *tok = strtok(buf, ","); tok && num_workers < MAX_WORKERS;
         tok = strtok(NULL, ",")) {
        while (*tok == ' ')
            tok++;
        worker_ports[num_workers++] = atoi(tok);
    }
}

/* Write a NUL-terminated string in full. */
static int send_str(int fd, const char *s);

/* Write the whole buffer, retrying on partial writes. */
static int send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int send_str(int fd, const char *s)
{
    return send_all(fd, s, strlen(s));
}

/* Connect to 127.0.0.1:port, giving up after timeout_ms. Returns fd or -1. */
static int connect_with_timeout(int port, int timeout_ms)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno == EINPROGRESS) {
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
        rc = select(fd + 1, NULL, &wset, NULL, &tv);
        if (rc <= 0) {
            close(fd);
            return -1; /* timed out or select error */
        }
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err != 0) {
            close(fd);
            return -1; /* connection refused, etc. */
        }
    } else if (rc < 0) {
        close(fd);
        return -1;
    }

    /* Back to blocking, with read/write deadlines. */
    fcntl(fd, F_SETFL, flags);
    struct timeval tv = {timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    return fd;
}

int main(void)
{
    const char *port_env = getenv("PROXY_PORT");
    int proxy_port = port_env ? atoi(port_env) : 1337;

    const char *workers_env = getenv("WORKER_PORTS");
    parse_workers(workers_env ? workers_env : "8080,8081,8082");
    if (num_workers == 0) {
        fprintf(stderr, "no worker ports configured\n");
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

    /* Build the "workers":[...] fragment once for the startup log. snprintf
     * returns the length it WOULD have written, so guard every step to keep
     * the offset in bounds and never let the remaining-size underflow. */
    char workers_json[256];
    {
        size_t off = 0;
        int n = snprintf(workers_json, sizeof(workers_json), "[");
        if (n > 0 && (size_t)n < sizeof(workers_json))
            off = (size_t)n;
        for (int i = 0; i < num_workers; i++) {
            n = snprintf(workers_json + off, sizeof(workers_json) - off, "%s%d",
                         i ? "," : "", worker_ports[i]);
            if (n < 0 || (size_t)n >= sizeof(workers_json) - off)
                break; /* would overflow; leave the list truncated */
            off += (size_t)n;
        }
        if (off < sizeof(workers_json) - 1)
            snprintf(workers_json + off, sizeof(workers_json) - off, "]");
    }
    char fields[1408];
    snprintf(fields, sizeof(fields), ",\"port\":%d,\"workers\":%s", proxy_port,
             workers_json);
    log_emit(LOG_INFO, "HTTP proxy starting", fields);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }
    int one = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)proxy_port);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    for (;;) {
        int client = accept(listen_fd, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        char req[REQ_BUF_SIZE];
        ssize_t reqlen = read(client, req, sizeof(req) - 1);
        if (reqlen <= 0) {
            close(client);
            continue;
        }
        req[reqlen] = '\0';

        /* Extract the request path for logging (best-effort). */
        char pathname[1024] = "/";
        char *sp1 = strchr(req, ' ');
        if (sp1) {
            char *target = sp1 + 1;
            char *sp2 = strchr(target, ' ');
            size_t plen = sp2 ? (size_t)(sp2 - target) : strlen(target);
            if (plen >= sizeof(pathname))
                plen = sizeof(pathname) - 1;
            memcpy(pathname, target, plen);
            pathname[plen] = '\0';
        }
        char path_esc[1024];
        log_json_escape(path_esc, sizeof(path_esc), pathname);

        snprintf(fields, sizeof(fields), ",\"pathname\":\"%s\"", path_esc);
        log_emit(LOG_DEBUG, "request", fields);

        int served = 0;
        int idx = rand() % num_workers;
        for (int attempt = 0; attempt < num_workers && !served; attempt++) {
            int target_port = worker_ports[idx];
            idx = (idx + 1) % num_workers;

            int backend = connect_with_timeout(target_port, BACKEND_TIMEOUT_MS);
            if (backend < 0) {
                snprintf(fields, sizeof(fields),
                         ",\"targetPort\":%d,\"pathname\":\"%s\"", target_port,
                         path_esc);
                log_emit(LOG_WARN, "backend unreachable", fields);
                continue;
            }

            if (send_all(backend, req, (size_t)reqlen) < 0) {
                snprintf(fields, sizeof(fields),
                         ",\"targetPort\":%d,\"pathname\":\"%s\"", target_port,
                         path_esc);
                log_emit(LOG_WARN, "backend unreachable", fields);
                close(backend);
                continue;
            }

            char chunk[RELAY_BUF_SIZE];
            ssize_t n = read(backend, chunk, sizeof(chunk));
            if (n < 13 || strncmp(chunk, "HTTP/", 5) != 0) {
                snprintf(fields, sizeof(fields),
                         ",\"targetPort\":%d,\"pathname\":\"%s\"", target_port,
                         path_esc);
                log_emit(LOG_WARN, "backend unreachable", fields);
                close(backend);
                continue;
            }

            int status = atoi(chunk + 9); /* "HTTP/1.1 NNN ..." */

            if (status == 404) {
                snprintf(fields, sizeof(fields),
                         ",\"targetPort\":%d,\"pathname\":\"%s\"", target_port,
                         path_esc);
                log_emit(LOG_WARN, "upstream 404", fields);
                send_str(client, "HTTP/1.1 404 Not Found\r\nContent-Length: "
                                 "0\r\nConnection: close\r\n\r\n");
                close(backend);
                served = 1;
                break;
            }

            if (status == 200) {
                snprintf(fields, sizeof(fields),
                         ",\"targetPort\":%d,\"pathname\":\"%s\"", target_port,
                         path_esc);
                log_emit(LOG_INFO, "proxy success", fields);
                /* Relay the first chunk, then stream the rest verbatim. */
                if (send_all(client, chunk, (size_t)n) == 0) {
                    while ((n = read(backend, chunk, sizeof(chunk))) > 0) {
                        if (send_all(client, chunk, (size_t)n) < 0)
                            break;
                    }
                }
                close(backend);
                served = 1;
                break;
            }

            /* Unexpected status: try the next worker. */
            snprintf(fields, sizeof(fields),
                     ",\"targetPort\":%d,\"pathname\":\"%s\",\"status\":%d",
                     target_port, path_esc, status);
            log_emit(LOG_WARN, "unexpected upstream status", fields);
            close(backend);
        }

        if (!served) {
            snprintf(fields, sizeof(fields), ",\"pathname\":\"%s\"", path_esc);
            log_emit(LOG_ERROR, "bad gateway — all backends failed", fields);
            send_str(client, "HTTP/1.1 502 Bad Gateway\r\nContent-Type: "
                             "text/plain\r\nContent-Length: 11\r\nConnection: "
                             "close\r\n\r\nBad Gateway");
        }
        close(client);
    }

    return 0;
}
