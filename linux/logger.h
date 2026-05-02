#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

static FILE *log_file = NULL;

static void init_logger(const char *logpath) {
    log_file = fopen(logpath, "a");
    if (!log_file) log_file = stderr;
    time_t t = time(NULL);
    char *ts = ctime(&t);
    ts[strlen(ts)-1] = '\0';
    fprintf(log_file, "\n=== SESSION: %s ===\n", ts);
    fflush(log_file);
}

static void close_logger(void) {
    if (log_file && log_file != stderr) {
        fprintf(log_file, "=== END ===\n");
        fclose(log_file);
        log_file = NULL;
    }
}

static void log_success(const char *call, long result) {
    fprintf(log_file, "[SUCCESS] %-20s => %ld\n", call, result);
    fflush(log_file);
}

static void log_error(const char *call) {
    fprintf(log_file, "[ERROR]   %-20s => %s (errno=%d)\n",
            call, strerror(errno), errno);
    fflush(log_file);
    perror(call);
}

static int safe_open(const char *path, int flags, mode_t mode) {
    int fd = open(path, flags, mode);
    if (fd < 0) log_error("open");
    else        log_success("open", fd);
    return fd;
}

static void safe_close(int fd, const char *tag) {
    if (close(fd) < 0) log_error(tag);
    else               log_success(tag, 0);
}

#endif
