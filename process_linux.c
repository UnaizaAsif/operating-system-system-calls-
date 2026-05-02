/* process_linux.c — syscalls: fork, execl, wait */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"

int main(void) {
    init_logger("syscalls.log");
    printf("=== Process Demo (fork/execl/wait) ===\n");

    pid_t pid = fork();
    if (pid < 0) { log_error("fork"); close_logger(); return 1; }
    log_success("fork", pid);

    if (pid == 0) {
        printf("[child]  PID=%d started\n", getpid());
        execl("/bin/echo", "echo", "[child]  execl: Hello from child!", NULL);
        log_error("execl"); exit(1);
    } else {
        printf("[parent] Forked child PID=%d\n", pid);
        int status;
        pid_t r = wait(&status);
        if (r < 0) log_error("wait");
        else       log_success("wait", r);
        printf("[parent] Child exited code %d\n", WEXITSTATUS(status));
    }

    printf("Process demo complete.\n\n");
    close_logger(); return 0;
}
