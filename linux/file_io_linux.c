/* file_io_linux.c — syscalls: open, read, write, close */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "logger.h"
#define BUF_SIZE 256

int main(void) {
    init_logger("syscalls.log");
    printf("=== File I/O Demo (open/read/write/close) ===\n");

    int fd_in = safe_open("input_signal.txt", O_RDONLY, 0);
    if (fd_in < 0) { close_logger(); return 1; }

    char buf[BUF_SIZE];
    ssize_t bytes_read = read(fd_in, buf, sizeof(buf)-1);
    if (bytes_read < 0) { log_error("read"); close_logger(); return 1; }
    buf[bytes_read] = '\0';
    log_success("read", bytes_read);
    printf("[read]  Read %zd bytes: \"%s\"\n", bytes_read, buf);
    safe_close(fd_in, "close(input)");

    int fd_out = safe_open("output_signal.txt", O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (fd_out < 0) { close_logger(); return 1; }
    ssize_t bw = write(fd_out, buf, bytes_read);
    if (bw < 0) log_error("write");
    else        log_success("write", bw);
    printf("[write] Wrote %zd bytes to output_signal.txt\n", bw);
    safe_close(fd_out, "close(output)");

    printf("File I/O complete.\n\n");
    close_logger(); return 0;
}
