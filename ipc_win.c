/*
 * ipc_win.c
 * Windows API equivalents of: pipe, fork+write, read
 * Win32 API: CreatePipe, CreateProcess (child writer),
 *            ReadFile (parent reader), WaitForSingleObject
 *
 * Compile (MinGW): gcc -Wall -o ipc_win.exe ipc_win.c
 * Compile (MSVC):  cl ipc_win.c
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "logger_win.h"

/*
 * On Windows there is no fork(). Instead we re-launch ourselves
 * with a "--child" argument. The child writes to the pipe's write
 * end (passed as an inheritable HANDLE via command line), then exits.
 * The parent reads from the read end — same IPC pattern as Linux pipe.
 */

int child_mode(HANDLE hWrite) {
    const char *msg = "IPC message via CreatePipe from child process!";
    DWORD written = 0;
    BOOL ok = WriteFile(hWrite, msg, (DWORD)strlen(msg) + 1, &written, NULL);
    if (!ok) {
        fprintf(stderr, "[child] WriteFile failed\n");
        return 1;
    }
    printf("[child] Wrote %lu bytes to pipe\n", written);
    CloseHandle(hWrite);
    return 0;
}

int main(int argc, char *argv[]) {

    /* ── Child branch ──────────────────────────────────────── */
    if (argc >= 3 && strcmp(argv[1], "--child") == 0) {
        /* Receive write handle passed by parent as hex string */
        HANDLE hWrite = (HANDLE)(ULONG_PTR)strtoull(argv[2], NULL, 16);
        return child_mode(hWrite);
    }

    /* ── Parent branch ─────────────────────────────────────── */
    init_logger_win("syscalls_win.log");
    printf("=== IPC Demo (CreatePipe / CreateProcess / ReadFile) ===\n");

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle       = TRUE;   /* pipe ends must be inheritable */

    HANDLE hRead = NULL, hWrite = NULL;

    /* -------------------------------------------------------
     * API CALL 1: CreatePipe  (equivalent to pipe())
     * Creates an anonymous pipe with a read end and write end.
     * bInheritHandle = TRUE so child can inherit hWrite.
     * ------------------------------------------------------- */
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        log_error_w("CreatePipe");
        close_logger_win(); return 1;
    }
    char detail[128];
    snprintf(detail, sizeof(detail), "hRead=0x%p hWrite=0x%p",
             (void*)hRead, (void*)hWrite);
    log_success_w("CreatePipe", detail);
    printf("[CreatePipe] hRead=%p  hWrite=%p\n", (void*)hRead, (void*)hWrite);

    /* Make read end non-inheritable so child doesn't get it */
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    /* -------------------------------------------------------
     * API CALL 2: CreateProcess  (child writer)
     * We re-launch this same .exe with --child <hWrite_hex>
     * so the child can write to the pipe's write end.
     * ------------------------------------------------------- */
    char cmdline[512];
    snprintf(cmdline, sizeof(cmdline), "\"%s\" --child %llx",
             argv[0], (unsigned long long)(ULONG_PTR)hWrite);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL,
                             TRUE,  /* inherit handles */
                             0, NULL, NULL, &si, &pi);
    if (!ok) {
        log_error_w("CreateProcess(child)");
        CloseHandle(hRead); CloseHandle(hWrite);
        close_logger_win(); return 1;
    }
    snprintf(detail, sizeof(detail), "PID=%lu", pi.dwProcessId);
    log_success_w("CreateProcess(child)", detail);
    printf("[CreateProcess] Child PID=%lu spawned\n", pi.dwProcessId);

    /* Parent closes its copy of the write end */
    CloseHandle(hWrite);

    /* -------------------------------------------------------
     * API CALL 3: ReadFile (parent reads from pipe)
     * Blocks until child writes data or closes write end.
     * ------------------------------------------------------- */
    char buf[512];
    DWORD bytesRead = 0;
    ok = ReadFile(hRead, buf, sizeof(buf) - 1, &bytesRead, NULL);
    if (!ok || bytesRead == 0) {
        log_error_w("ReadFile(pipe)");
    } else {
        buf[bytesRead] = '\0';
        snprintf(detail, sizeof(detail), "%lu bytes", bytesRead);
        log_success_w("ReadFile(pipe)", detail);
        printf("[ReadFile]   Parent received: \"%s\"\n", buf);
    }

    CloseHandle(hRead);

    /* -------------------------------------------------------
     * API CALL 4: WaitForSingleObject  (equivalent to wait())
     * ------------------------------------------------------- */
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    log_success_w("WaitForSingleObject", "child done");
    printf("[Wait] Child exited code %lu\n", exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    printf("IPC demo complete.\n\n");
    close_logger_win();
    return 0;
}
