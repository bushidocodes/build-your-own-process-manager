CC      = gcc
CFLAGS  = -Wall -Wextra

# Build all three programs: the process manager (pm), the web server (web),
# and the reverse proxy (proxy). pm is the one you implement in this lab.
build: pm web proxy

pm: main.c
	$(CC) $(CFLAGS) -o pm main.c

web: web.c log.c log.h
	$(CC) $(CFLAGS) -o web web.c log.c

proxy: proxy.c log.c log.h
	$(CC) $(CFLAGS) -o proxy proxy.c log.c

clean:
	rm -f pm web proxy

# Run a single web server on port 8081 (handy for manual testing).
run-web: web
	./web 8081

# Run the reverse proxy on port 1337.
run-proxy: proxy
	./proxy

# Run one worker (Objective 1): ./pm
# Run worker pool (Objective 2): ./pm 8080 8081 8082
run: build
	./pm

.PHONY: build clean run run-web run-proxy
