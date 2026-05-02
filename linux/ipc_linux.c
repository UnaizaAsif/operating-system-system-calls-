/* ipc_linux.c — syscalls: pipe, fork, read/write over pipe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "logger.h"

int main(void) {
    init_logger("syscalls.log");
    printf("=== IPC Demo (pipe/fork/read/write) ===\n");

    int pfd[2];
    if (pipe(pfd) < 0) { log_error("pipe"); close_logger(); return 1; }
    log_success("pipe", pfd[0]);
    printf("[pipe]  read_fd=%d write_fd=%d\n", pfd[0], pfd[1]);

    pid_t pid = fork();
    if (pid < 0) { log_error("fork"); close_logger(); return 1; }
    log_success("fork", pid);

    if (pid == 0) {
        close(pfd[0]);
        const char *msg = "IPC message via pipe from child!";
        ssize_t w = write(pfd[1], msg, strlen(msg)+1);
        if (w < 0) log_error("write(pipe)");
        else       log_success("write(pipe)", w);
        close(pfd[1]); exit(0);
    } else {
        close(pfd[1]);
        char buf[256];
        ssize_t r = read(pfd[0], buf, sizeof(buf)-1);
        if (r < 0) log_error("read(pipe)");
        else { buf[r]='\0'; log_success("read(pipe)", r);
               printf("[pipe]  Received: \"%s\"\n", buf); }
        close(pfd[0]);
        int st; wait(&st);
        printf("[wait]  Child code %d\n", WEXITSTATUS(st));
    }

    printf("IPC demo complete.\n\n");
    close_logger(); return 0;
}
