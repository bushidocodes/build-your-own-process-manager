#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_WORKERS 8

static pid_t pids[MAX_WORKERS];
static int   ports[MAX_WORKERS];
static int   num_workers = 0;
static volatile sig_atomic_t running = 1;

static void spawn(int i)
{
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", ports[i]);

    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = {
            "deno", "run", "--allow-read", "--allow-net", "--allow-env",
            "web.ts", port_str, NULL
        };
        execvp("deno", args);
        perror("execvp");
        _exit(1);
    }
    if (pid < 0) { perror("fork"); exit(1); }
    pids[i] = pid;
    printf("[pm] worker %d  pid=%d  port=%d\n", i, pid, ports[i]);
}

static void on_signal(int sig) { (void)sig; running = 0; }

int main(int argc, char *argv[])
{
    /* Accept port numbers as args; default to single worker on 8080 */
    if (argc > 1) {
        for (int i = 1; i < argc && num_workers < MAX_WORKERS; i++)
            ports[num_workers++] = atoi(argv[i]);
    } else {
        ports[num_workers++] = 8080;
    }

    struct sigaction sa;
    sa.sa_handler = on_signal;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    for (int i = 0; i < num_workers; i++)
        spawn(i);

    /* Poll for dead workers and respawn them */
    while (running) {
        int status;
        pid_t died;
        while ((died = waitpid(-1, &status, WNOHANG)) > 0) {
            for (int i = 0; i < num_workers; i++) {
                if (pids[i] == died) {
                    printf("[pm] worker %d (pid=%d) exited — respawning in 1s\n", i, died);
                    sleep(1); /* backoff: prevents thrash if worker fails immediately */
                    spawn(i);
                    break;
                }
            }
        }
        usleep(100000); /* 100 ms poll interval */
    }

    /* Graceful shutdown: SIGTERM all children then wait for them */
    printf("[pm] shutting down\n");
    for (int i = 0; i < num_workers; i++)
        if (pids[i] > 0) kill(pids[i], SIGTERM);
    for (int i = 0; i < num_workers; i++)
        if (pids[i] > 0) waitpid(pids[i], NULL, 0);

    return 0;
}
