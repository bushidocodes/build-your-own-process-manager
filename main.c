#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NCHILDREN 3
#define PROGLEN 30

const int port_base = 8080;
int children[NCHILDREN] = {0};
char port[5] = "8080";
char *args[] = {"./deno", "run", "--allow-read", "--allow-net", "web.ts", port, NULL};

int child_main(int port_offset)
{
    sprintf(port, "%d", port_base + port_offset);

    if (execvp("deno", args) < 0)
    {
        fprintf(stderr, "execv failed with %s\n", strerror(errno));
        fflush(stderr);
        exit(-1);
    }
}

int main(void)
{
    pid_t pid;

    int active = 0;

    for (int i = 0; i < NCHILDREN; i++)
    {
        switch (pid = fork())
        {
        case -1:
            perror("fork");
        case 0:
            return child_main(i);
        default:
            children[i] = pid;
        }
    }

    printf("I'm the parent\n");
    fflush(stdout);

    int children_returned = 0;
    while (children_returned < NCHILDREN)
    {
        for (int i = 0; i < NCHILDREN; i++)
        {
            sleep(1);
            if (children[i] != -1)
            {
                int rc = -99;
                pid_t pid = waitpid(children[i], &rc, WNOHANG);
                if (pid < 0)
                {
                    perror("waitpid");
                }
                else if (pid > 0)
                {
                    printf("Child with PID %d returned with RC %d\n", pid, rc);
                    fflush(stdout);

                    switch (pid = fork())
                    {
                    case -1:
                        perror("fork");
                    case 0:
                        return child_main(i);
                    default:
                        children[i] = pid;
                    }
                }
            }
        }
    }

    printf("Done!\n");

    // pid_t child = wait(&rc);
    // printf("Child %d ended with RC of %d which is %s\n", child, rc, strerror(rc));

    // Unix Domain Socket Heartbeat?
    //

    return 0;
}
