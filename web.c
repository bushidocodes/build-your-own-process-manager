/*
 * web.c — static file HTTP server with a chaos monkey (C replacement for
 * web.ts)
 *
 * Serves files from the www/ directory over HTTP/1.1. The listening port comes
 * from the PORT environment variable, then the first CLI argument, then 8080.
 *
 * On every successfully served request the "chaos monkey" may strike with
 * probability CHAOS_RATE (default 0.05), terminating the process with exit(1)
 * to simulate spontaneous failure — this is what the process manager must
 * recover from.
 *
 * Connections are handled one at a time (a simple sequential accept loop),
 * which is plenty for this lab and keeps the focus on process management.
 */
#include "log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define REQ_BUF_SIZE 8192

/* Map a filename extension to a MIME type (replaces @std/media-types). */
static const char *mime_type(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (strcmp(dot, ".html") == 0)
            return "text/html; charset=UTF-8";
        if (strcmp(dot, ".css") == 0)
            return "text/css; charset=UTF-8";
        if (strcmp(dot, ".js") == 0)
            return "text/javascript; charset=UTF-8";
        if (strcmp(dot, ".json") == 0)
            return "application/json; charset=UTF-8";
        if (strcmp(dot, ".png") == 0)
            return "image/png";
        if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
            return "image/jpeg";
        if (strcmp(dot, ".gif") == 0)
            return "image/gif";
        if (strcmp(dot, ".svg") == 0)
            return "image/svg+xml";
        if (strcmp(dot, ".ico") == 0)
            return "image/x-icon";
        if (strcmp(dot, ".txt") == 0)
            return "text/plain; charset=UTF-8";
    }
    return "application/octet-stream";
}

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

static void send_status(int fd, int status, const char *reason)
{
    char head[128];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: "
                     "close\r\n\r\n",
                     status, reason);
    send_all(fd, head, (size_t)n);
}

int main(int argc, char *argv[])
{
    const char *port_env = getenv("PORT");
    int port = port_env ? atoi(port_env) : argc > 1 ? atoi(argv[1]) : 8080;

    const char *chaos_env = getenv("CHAOS_RATE");
    double chaos_rate = chaos_env ? atof(chaos_env) : 0.05;

    const char *www_dir = getenv("WWW_DIR");
    if (!www_dir)
        www_dir = "www";

    srand((unsigned int)(time(NULL) ^ (getpid() << 16)));

    char fields[1152];
    snprintf(fields, sizeof(fields), ",\"port\":%d,\"chaosRate\":%g", port,
             chaos_rate);
    log_emit(LOG_INFO, "HTTP webserver starting", fields);

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
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(listen_fd, 16) < 0) {
        perror("listen");
        return 1;
    }

    for (;;) {
        int conn = accept(listen_fd, NULL, NULL);
        if (conn < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        char req[REQ_BUF_SIZE];
        ssize_t r = read(conn, req, sizeof(req) - 1);
        if (r <= 0) {
            close(conn);
            continue;
        }
        req[r] = '\0';

        /* Parse the request line: "GET /path HTTP/1.1". */
        char path[1024] = "index.html";
        char *sp1 = strchr(req, ' ');
        if (sp1) {
            char *target = sp1 + 1;
            char *sp2 = strchr(target, ' ');
            if (sp2)
                *sp2 = '\0';
            char *query = strchr(target, '?');
            if (query)
                *query = '\0';
            if (target[0] == '/')
                target++;
            if (*target)
                snprintf(path, sizeof(path), "%s", target);
        }

        char path_esc[1024];
        log_json_escape(path_esc, sizeof(path_esc), path);

        /* Reject path traversal outright. */
        if (strstr(path, "..")) {
            snprintf(fields, sizeof(fields), ",\"path\":\"%s\",\"status\":404",
                     path_esc);
            log_emit(LOG_WARN, "not found", fields);
            send_status(conn, 404, "Not Found");
            close(conn);
            continue;
        }

        char fullpath[1100];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", www_dir, path);

        int file_fd = open(fullpath, O_RDONLY);
        struct stat st;
        if (file_fd < 0 || fstat(file_fd, &st) < 0 || !S_ISREG(st.st_mode)) {
            if (file_fd >= 0)
                close(file_fd);
            snprintf(fields, sizeof(fields), ",\"path\":\"%s\",\"status\":404",
                     path_esc);
            log_emit(LOG_WARN, "not found", fields);
            send_status(conn, 404, "Not Found");
            close(conn);
            continue;
        }

        /* Chaos monkey: strike only on otherwise-successful requests. */
        if ((double)rand() / RAND_MAX < chaos_rate) {
            snprintf(fields, sizeof(fields), ",\"path\":\"%s\"", path_esc);
            log_emit(LOG_WARN, "chaos monkey strike", fields);
            close(file_fd);
            close(conn);
            exit(1);
        }

        char *body = malloc((size_t)st.st_size);
        if (!body) {
            close(file_fd);
            send_status(conn, 500, "Internal Server Error");
            close(conn);
            continue;
        }
        ssize_t got = 0;
        while (got < st.st_size) {
            ssize_t n = read(file_fd, body + got, (size_t)(st.st_size - got));
            if (n <= 0)
                break;
            got += n;
        }
        close(file_fd);

        char header[256];
        int hn =
            snprintf(header, sizeof(header),
                     "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length:"
                     " %lld\r\nConnection: close\r\n\r\n",
                     mime_type(path), (long long)got);
        if (send_all(conn, header, (size_t)hn) == 0)
            send_all(conn, body, (size_t)got);
        free(body);

        snprintf(fields, sizeof(fields), ",\"path\":\"%s\",\"status\":200",
                 path_esc);
        log_emit(LOG_INFO, "serve", fields);
        close(conn);
    }

    return 0;
}
