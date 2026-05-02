/*
 * logger_win.h  —  Windows logging & safe API wrappers
 * Works with MSVC and MinGW (Windows-only header)
 */
#ifndef LOGGER_WIN_H
#define LOGGER_WIN_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static FILE *log_file_w = NULL;

/* Get last Windows error as a readable string */
static void get_win_error(char *buf, DWORD sz) {
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(), 0, buf, sz, NULL);
    /* strip trailing \r\n */
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n')) buf[--len] = '\0';
}

static void init_logger_win(const char *logpath) {
    log_file_w = fopen(logpath, "a");
    if (!log_file_w) log_file_w = stderr;
    time_t t = time(NULL);
    char *ts = ctime(&t);
    ts[strlen(ts)-1] = '\0';
    fprintf(log_file_w, "\n=== SESSION: %s ===\n", ts);
    fflush(log_file_w);
}

static void close_logger_win(void) {
    if (log_file_w && log_file_w != stderr) {
        fprintf(log_file_w, "=== END ===\n");
        fclose(log_file_w);
        log_file_w = NULL;
    }
}

static void log_success_w(const char *call, const char *detail) {
    fprintf(log_file_w, "[SUCCESS] %-26s => %s\n", call, detail);
    fflush(log_file_w);
}

static void log_error_w(const char *call) {
    char errbuf[512];
    get_win_error(errbuf, sizeof(errbuf));
    DWORD code = GetLastError();
    fprintf(log_file_w, "[ERROR]   %-26s => %s (code=%lu)\n", call, errbuf, code);
    fflush(log_file_w);
    fprintf(stderr, "[ERROR] %s: %s\n", call, errbuf);
}

/* Safe CreateFile wrapper */
static HANDLE safe_create_file(const char *path, DWORD access, DWORD share,
                                DWORD disposition, DWORD flags) {
    HANDLE h = CreateFileA(path, access, share, NULL, disposition, flags, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        log_error_w("CreateFile");
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf), "HANDLE=0x%p", (void*)h);
        log_success_w("CreateFile", buf);
    }
    return h;
}

/* Safe CloseHandle wrapper */
static void safe_close_handle(HANDLE h, const char *tag) {
    if (!CloseHandle(h)) {
        log_error_w(tag);
    } else {
        log_success_w(tag, "OK");
    }
}

#endif /* LOGGER_WIN_H */
