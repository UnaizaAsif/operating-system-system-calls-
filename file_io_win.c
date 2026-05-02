/*
 * file_io_win.c
 * Windows API equivalents of: open, read, write, close
 * Win32 API: CreateFile, ReadFile, WriteFile, CloseHandle
 *
 * Compile (MinGW): gcc -Wall -o file_io_win.exe file_io_win.c
 * Compile (MSVC):  cl file_io_win.c
 */
#include <windows.h>
#include <stdio.h>
#include "logger_win.h"

#define INPUT_FILE  "input_signal.txt"
#define OUTPUT_FILE "output_signal.txt"
#define BUF_SIZE    512

int main(void) {
    init_logger_win("syscalls_win.log");
    printf("=== File I/O Demo (CreateFile/ReadFile/WriteFile/CloseHandle) ===\n");

    /* -------------------------------------------------------
     * API CALL 1: CreateFile  (equivalent to open())
     * Opens an existing file for reading.
     * GENERIC_READ         = read-only access
     * FILE_SHARE_READ      = allow other readers simultaneously
     * OPEN_EXISTING        = fail if file does not exist
     * FILE_ATTRIBUTE_NORMAL = no special attributes
     * ------------------------------------------------------- */
    HANDLE hIn = safe_create_file(
        INPUT_FILE,
        GENERIC_READ,
        FILE_SHARE_READ,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL
    );
    if (hIn == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open %s\n", INPUT_FILE);
        close_logger_win(); return 1;
    }

    /* -------------------------------------------------------
     * API CALL 2: ReadFile  (equivalent to read())
     * lpBuffer        = destination buffer
     * nNumberOfBytesToRead = max bytes to read
     * lpNumberOfBytesRead  = actual bytes read (out param)
     * ------------------------------------------------------- */
    char buf[BUF_SIZE];
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(hIn, buf, BUF_SIZE - 1, &bytesRead, NULL);
    if (!ok || bytesRead == 0) {
        log_error_w("ReadFile");
        CloseHandle(hIn);
        close_logger_win(); return 1;
    }
    buf[bytesRead] = '\0';
    char detail[64];
    snprintf(detail, sizeof(detail), "%lu bytes", bytesRead);
    log_success_w("ReadFile", detail);
    printf("[ReadFile]  Read %lu bytes: \"%s\"\n", bytesRead, buf);

    /* -------------------------------------------------------
     * API CALL 3: CloseHandle  (equivalent to close())
     * Releases the file HANDLE back to the OS.
     * ------------------------------------------------------- */
    safe_close_handle(hIn, "CloseHandle(input)");

    /* -------------------------------------------------------
     * API CALL 4: CreateFile  (write mode, equivalent to open() O_WRONLY|O_CREAT|O_TRUNC)
     * GENERIC_WRITE        = write-only access
     * CREATE_ALWAYS        = create new, overwrite if exists
     * ------------------------------------------------------- */
    HANDLE hOut = safe_create_file(
        OUTPUT_FILE,
        GENERIC_WRITE,
        0,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL
    );
    if (hOut == INVALID_HANDLE_VALUE) {
        close_logger_win(); return 1;
    }

    /* -------------------------------------------------------
     * API CALL 5: WriteFile  (equivalent to write())
     * ------------------------------------------------------- */
    DWORD bytesWritten = 0;
    ok = WriteFile(hOut, buf, bytesRead, &bytesWritten, NULL);
    if (!ok) {
        log_error_w("WriteFile");
    } else {
        snprintf(detail, sizeof(detail), "%lu bytes", bytesWritten);
        log_success_w("WriteFile", detail);
        printf("[WriteFile] Wrote %lu bytes to %s\n", bytesWritten, OUTPUT_FILE);
    }

    /* -------------------------------------------------------
     * API CALL 6: CloseHandle  (close output)
     * ------------------------------------------------------- */
    safe_close_handle(hOut, "CloseHandle(output)");

    printf("File I/O complete. Check syscalls_win.log\n\n");
    close_logger_win();
    return 0;
}
