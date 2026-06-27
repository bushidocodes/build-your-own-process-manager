## Practical Linux Systems Programming with the Process API

### GWU CSCI 3411 - Fall 2020 - Lab 4

---

## Build your own Process manager

Today, you are going to implement your own process manager from scratch using C
and the Process APIs presented by POSIX, such as `fork`, `exec`, and `wait`.

## Prerequisites

This lab targets a POSIX environment (Linux, WSL, or macOS). You only need a C
compiler and `make`:

```sh
gcc --version
make --version
```

There is no runtime to install — the web server and proxy you will manage are
small C programs that build alongside your process manager. Build everything
with:

```sh
make build
```

This compiles three programs:

- `pm` — the process manager you implement in `main.c`
- `web` — the static web server (provided, in `web.c`)
- `proxy` — the reverse proxy (provided, in `proxy.c`)

Note: You are NOT expected to modify `web.c` or `proxy.c`. Skim them to get a
rough idea of what they do.

## Investigating the Problematic App

Our buggy app is one of the most mission-critical things on the web: Gabe
Parmer's website. If the website goes down, potential PhDs are unable to learn
about Gabe's research and consider applying to join his lab.

Open the `www` directory and look at the contents. This contains the HTML, CSS,
and image for Gabe's site.

These static assets need to be served by a web server, and the buggy web server
is written in C in `web.c`.

Open the `web.c` script, and notice the following:

The port is configurable via the `PORT` environment variable or a command-line
argument, defaulting to 8080 if neither is provided.

```c
const char *port_env = getenv("PORT");
int port = port_env ? atoi(port_env) : argc > 1 ? atoi(argv[1]) : 8080;
```

The request handler crashes with a configurable probability (defaulting to 5%,
via the `CHAOS_RATE` environment variable), simulating spontaneous failure.

```c
if ((double)rand() / RAND_MAX < chaos_rate) {
    log_emit(LOG_WARN, "chaos monkey strike", fields);
    exit(1);
}
```

Let's first take a look at the Makefile to see how the web server is run.

```Makefile
run-web: web
	./web 8081
```

The Makefile rule `run-web` builds the `web` program and runs it, passing `8081`
as an argument. As we saw above, this sets the port the web server will serve
Gabe's website from.

Let's run this.

```sh
make run-web
```

should yield the following

```sh
sean@MANPUTER:~/projects/process-manager$ make run-web
gcc -Wall -Wextra -o web web.c log.c
./web 8081
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"HTTP webserver starting","port":8081,"chaosRate":0.05}
```

Since you are running from the VSCode terminal, the Remote extensions
automatically proxy from the WSL backend / Multipass VM. This means that you can
hold down control and left click on the URL in the console as if it were a
hyperlink, and you'll open Gabe's webpage in your default browser. (Or just open
`http://localhost:8081/` yourself.)

_Note: If you are running WSL, the actual port that the link opens may be
slightly different. Be sure to click the link in the console rather than
copy/pasting text into your URL bar._

Your default browser should load Gabe's web page. Hit refresh until the website
doesn't load properly.

Go back to your terminal. You should see the following!

```sh
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"serve","path":"index.html","status":200}
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"serve","path":"index.css","status":200}
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"serve","path":"gp.png","status":200}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"chaos monkey strike","path":"gp.png"}
```

The `web` process exits, and the website is down until something restarts it.

Note: the Chaos Monkey is the mascot of the discipline software engineering
[Chaos Engineering](https://en.wikipedia.org/wiki/Chaos_engineering). I suggest
saving this link and reading up on this later.

## Objective 1: Auto-respawn

The first objective is to create a process manager that launches the web server
and relaunches it whenever the web server exits!

You should implement this in `main.c`. The program should invoke the `web`
program just as in the `run-web` rule in the Makefile.

Running `make build` will compile your process manager.

Running `make run` will compile and execute your process manager.

Hints:

- Read the manual pages for `fork`, `exec`, and `wait`
- The arguments probably need to be split into an array of NULL-terminated
  tokens. The first element should be the relative path used to launch the
  program. The last element should be NULL. This exact syntax might vary
  depending on the `exec` variant you use
  ```c
  char *args[] = {"./web", "8081", NULL};
  ```
- If you are printing to console from a child process, you might need to use
  `fflush` to make sure the text is printed before the process terminates

#### Reference Architecture

```
__________________________
|       web server       |
|         :8081          |
''''''''''''''''''''''''''
__________________________
|           PM           |
''''''''''''''''''''''''''
```

Success Criteria: If the chaos monkey strikes and your process terminates, your
process manager should automatically restart the process. This should be at the
same IP address, such that one can refresh hitting the website over and over.
Specific requests might sporadically fail, but the website is never fully down.

## Objective 2: Pool of 3 Workers

Enhance your process to spawn a pool of three workers. This allows requests to
automatically fail over to another worker if one fails. That makes the error
transparent to the client.

We provide a proxy server called `proxy.c` to help redirect traffic across your
pool of workers.

Open this file and notice the following:

1. The proxy defaults to balancing between web servers running on 8080, 8081,
   and 8082 (configurable via `WORKER_PORTS`). This means that you need to make
   sure that these ports are used exactly:

   ```c
   parse_workers(workers_env ? workers_env : "8080,8081,8082");
   ```

2. The proxy round robins between the three web servers, starting from a random
   one

   ```c
   int idx = rand() % num_workers;
   for (int attempt = 0; attempt < num_workers && !served; attempt++) {
       int target_port = worker_ports[idx];
       idx = (idx + 1) % num_workers;
       /* ...try this worker... */
   }
   ```

3. If a worker responds with 200, the response is relayed back to the client.
   Otherwise, it tries the next port. If all backends fail, a 502 Bad Gateway is
   returned.

You can run the proxy alongside your process manager.

Run `make run` to start your process manager.

Open a second tab and run `make run-proxy`.

Notice that the proxy attempts to load balance between ports 8080, 8081, and
8082, but because you're only running a server on 8081, it ends up failing over
to 8081 on each request.

```sh
sean@MANPUTER:~/projects/process-manager$ make run-proxy
gcc -Wall -Wextra -o proxy proxy.c log.c
./proxy
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"HTTP proxy starting","port":1337,"workers":[8080,8081,8082]}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8082,"pathname":"/"}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8080,"pathname":"/"}
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"proxy success","targetPort":8081,"pathname":"/"}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8082,"pathname":"/index.css"}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8080,"pathname":"/index.css"}
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"proxy success","targetPort":8081,"pathname":"/index.css"}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8082,"pathname":"/gp.png"}
{"ts":"2026-01-01T12:00:00.000Z","level":"WARN","msg":"backend unreachable","targetPort":8080,"pathname":"/gp.png"}
{"ts":"2026-01-01T12:00:00.000Z","level":"INFO","msg":"proxy success","targetPort":8081,"pathname":"/gp.png"}
```

You now have all the pieces that you need to enhance your process manager to
spawn and monitor a pool of workers.

```
                _______________
                |    Proxy    |
                |    :1337    |
                '''''''''''''''
                       |
        v--------------v---------------v
_______________ _______________ _______________
| web server  | | web server  | | web server  |
|    :8080    | |    :8081    | |    :8082    |
''''''''''''''' ''''''''''''''' '''''''''''''''
_______________________________________________
|                     PM                      |
'''''''''''''''''''''''''''''''''''''''''''''''
```

Hints:

- You need a way to know the port of the node that failed. This probably needs
  to be maintained in some sort of mapping from pid to port.
- You thus need to know the pid of the child process that failed. One of the
  members of the `wait` family of APIs is better for this than others.
- Is the `wait` call you chose blocking or nonblocking? Which should it be?
- What does `WNOHANG` do?

## Cleanup

There is nothing to uninstall. To remove the compiled binaries, run:

```sh
make clean
```

## Reference Solution

A reference implementation of `main.c` is provided in encoded form under
`solution/main.c.b64`.

**Only look at this after you have made a genuine attempt at the lab.**

To decode and inspect it:

```sh
base64 -d solution/main.c.b64 > /tmp/solution.c
cat /tmp/solution.c
```

To decode and build it directly:

```sh
base64 -d solution/main.c.b64 > main.c
make build
```
