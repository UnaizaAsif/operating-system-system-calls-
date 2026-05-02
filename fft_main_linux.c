/*
 * fft_main_linux.c  — All 15 syscalls integrated + parallel FFT
 * Syscalls: open,read,write,close | fork,wait | mmap,munmap | pipe | chmod,umask
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>
#include "logger.h"
#include "../common/fft.c"

#define FFT_N    8
#define SHM_SIZE (FFT_N * sizeof(double complex))

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

static void write_results(const char *path, double complex *d, int n) {
    int fd = safe_open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) return;
    char line[128];
    for (int i=0;i<n;i++){
        int len = snprintf(line,sizeof(line),"X[%d] = %10.4f + %10.4fi\n",
                           i,creal(d[i]),cimag(d[i]));
        write(fd, line, len);
    }
    log_success("write(fft_output)", n);
    safe_close(fd, "close(fft_output)");
}

int main(void) {
    init_logger("syscalls.log");
    printf("=================================================\n");
    printf("  Parallel FFT — All 15 Syscalls Integrated\n");
    printf("=================================================\n\n");
    double t0 = now_ms();

    /* 1. PERMISSIONS */
    printf("--- [1] Permissions ---\n");
    mode_t old_mask = umask(0022);
    log_success("umask(0022)", old_mask);
    printf("[umask]  Set 0022 (was 0%03o)\n", old_mask);

    /* 2. FILE I/O: read signal */
    printf("\n--- [2] File I/O: read input ---\n");
    int fd_in = safe_open("input_signal.txt", O_RDONLY, 0);
    if (fd_in < 0) { close_logger(); return 1; }
    char fbuf[512];
    ssize_t rb = read(fd_in, fbuf, sizeof(fbuf)-1);
    if (rb < 0) { log_error("read"); close_logger(); return 1; }
    log_success("read(input)", rb);
    safe_close(fd_in, "close(input)");
    fbuf[rb] = '\0';

    double samples[FFT_N];
    int count = 0;
    char *tok = strtok(fbuf, "\n, ");
    while (tok && count < FFT_N) { samples[count++] = atof(tok); tok = strtok(NULL,"\n, "); }
    for (int i=count;i<FFT_N;i++) samples[i]=0.0;
    printf("[read]   Parsed %d samples: ", count);
    for (int i=0;i<FFT_N;i++) printf("%.1f ",samples[i]);
    printf("\n");

    /* 3. MEMORY: mmap shared */
    printf("\n--- [3] Memory: mmap ---\n");
    double complex *shared = mmap(NULL, SHM_SIZE,
                                  PROT_READ|PROT_WRITE,
                                  MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) { log_error("mmap"); close_logger(); return 1; }
    log_success("mmap(shared)", 1);
    for (int i=0;i<FFT_N;i++) shared[i] = samples[i] + 0.0*I;
    printf("[mmap]   %zu bytes shared for %d complex samples\n", SHM_SIZE, FFT_N);

    /* 4. IPC: pipe */
    printf("\n--- [4] IPC: pipe ---\n");
    int pfd[2];
    if (pipe(pfd)<0) { log_error("pipe"); munmap(shared,SHM_SIZE); close_logger(); return 1; }
    log_success("pipe", pfd[0]);
    printf("[pipe]   read_fd=%d write_fd=%d\n", pfd[0], pfd[1]);

    /* 5. PROCESS: fork parallel FFT */
    printf("\n--- [5] Process: parallel FFT ---\n");
    double tf = now_ms();

    pid_t pid1 = fork();
    if (pid1 < 0) { log_error("fork(1)"); close_logger(); return 1; }
    log_success("fork(child1)", pid1);
    if (pid1 == 0) {
        close(pfd[0]);
        printf("[child1] PID=%d: FFT on first half\n", getpid());
        fft(shared, FFT_N/2);
        write(pfd[1], "H1", 2);
        close(pfd[1]); exit(0);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) { log_error("fork(2)"); close_logger(); return 1; }
    log_success("fork(child2)", pid2);
    if (pid2 == 0) {
        close(pfd[0]);
        printf("[child2] PID=%d: FFT on second half\n", getpid());
        fft(&shared[FFT_N/2], FFT_N/2);
        write(pfd[1], "H2", 2);
        close(pfd[1]); exit(0);
    }

    close(pfd[1]);
    char notify[8];
    read(pfd[0], notify, 2);
    read(pfd[0], notify+2, 2);
    close(pfd[0]);

    int s1,s2;
    waitpid(pid1,&s1,0); log_success("wait(child1)", WEXITSTATUS(s1));
    waitpid(pid2,&s2,0); log_success("wait(child2)", WEXITSTATUS(s2));
    printf("[parent] FFT done in %.3f ms\n", now_ms()-tf);

    /* 6. FILE I/O: write output */
    printf("\n--- [6] File I/O: write fft_output.txt ---\n");
    write_results("fft_output.txt", shared, FFT_N);
    chmod("fft_output.txt", 0644);
    log_success("chmod(fft_output)", 0644);
    printf("[write]  Saved to fft_output.txt\n");

    /* 7. MEMORY: munmap */
    printf("\n--- [7] Memory: munmap ---\n");
    if (munmap(shared, SHM_SIZE)<0) log_error("munmap");
    else                            log_success("munmap", 0);
    printf("[munmap] Released shared memory\n");

    umask(old_mask);

    /* 8. PRINT RESULTS */
    printf("\n=================================================\n");
    printf("  FFT RESULTS (8-point DFT)\n");
    printf("=================================================\n");
    int fr = safe_open("fft_output.txt", O_RDONLY, 0);
    if (fr>=0) {
        char rb2[1024]; ssize_t n2=read(fr,rb2,sizeof(rb2)-1);
        if (n2>0){rb2[n2]='\0'; printf("%s",rb2);}
        safe_close(fr,"close(results)");
    }
    printf("\n  Total time: %.3f ms\n", now_ms()-t0);
    printf("=================================================\n");
    printf("\nCheck syscalls.log for full trace.\n");
    close_logger(); return 0;
}
