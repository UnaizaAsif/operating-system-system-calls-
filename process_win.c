/*
 * process_win.c
 * Windows API equivalents of: fork, exec, wait
 * Win32 API: CreateProcess, WaitForSingleObject, GetExitCodeProcess
 *
 * Compile (MinGW): gcc -Wall -o process_win.exe process_win.c
 * Compile (MSVC):  cl process_win.c
 */
#include <windows.h>
#include <stdio.h>
#include "logger_win.h"

int main(void) {
    init_logger_win("syscalls_win.log");
    printf("=== Process Demo (CreateProcess/WaitForSingleObject/GetExitCodeProcess) ===\n");

    /* -------------------------------------------------------
     * API CALL 1: CreateProcess  (equivalent to fork() + exec())
     *
     * Unlike Linux where fork() + exec() are two separate calls,
     * Windows combines them: CreateProcess() both creates a new
     * process AND specifies which executable to run.
     *
     * STARTUPINFO      - specifies window/console properties
     * PROCESS_INFORMATION - receives new process/thread HANDLEs
     *
     * lpApplicationName = NULL (use command line instead)
     * lpCommandLine     = command to run (cmd /c echo ...)
     * bInheritHandles   = TRUE so child can inherit console
     * dwCreationFlags   = 0 (default)
     * ------------------------------------------------------- */
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    /* Command equivalent to: execl("/bin/echo", "echo", "Hello!", NULL) */
    char cmdline[] = "cmd.exe /C echo [child] CreateProcess: Hello from child process!";

    BOOL ok = CreateProcessA(
        NULL,       /* lpApplicationName  */
        cmdline,    /* lpCommandLine      */
        NULL,       /* lpProcessAttributes */
        NULL,       /* lpThreadAttributes  */
        TRUE,       /* bInheritHandles     */
        0,          /* dwCreationFlags     */
        NULL,       /* lpEnvironment       */
        NULL,       /* lpCurrentDirectory  */
        &si,        /* lpStartupInfo       */
        &pi         /* lpProcessInformation */
    );

    if (!ok) {
        log_error_w("CreateProcess");
        close_logger_win(); return 1;
    }

    char detail[128];
    snprintf(detail, sizeof(detail), "PID=%lu TID=%lu",
             pi.dwProcessId, pi.dwThreadId);
    log_success_w("CreateProcess", detail);
    printf("[CreateProcess] Spawned child PID=%lu\n", pi.dwProcessId);

    /* -------------------------------------------------------
     * API CALL 2: WaitForSingleObject  (equivalent to wait())
     * Blocks the parent until the child process terminates.
     * INFINITE = wait forever (no timeout)
     * ------------------------------------------------------- */
    DWORD waitResult = WaitForSingleObject(pi.hProcess, INFINITE);
    if (waitResult == WAIT_FAILED) {
        log_error_w("WaitForSingleObject");
    } else {
        log_success_w("WaitForSingleObject", "WAIT_OBJECT_0");
        printf("[WaitForSingleObject] Child finished\n");
    }

    /* -------------------------------------------------------
     * API CALL 3: GetExitCodeProcess  (equivalent to WEXITSTATUS)
     * Retrieves the exit code of the terminated child process.
     * ------------------------------------------------------- */
    DWORD exitCode = 0;
    if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
        snprintf(detail, sizeof(detail), "exit_code=%lu", exitCode);
        log_success_w("GetExitCodeProcess", detail);
        printf("[GetExitCodeProcess] Child exit code: %lu\n", exitCode);
    } else {
        log_error_w("GetExitCodeProcess");
    }

    /* Always close process and thread handles */
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    log_success_w("CloseHandle(process+thread)", "OK");

    printf("Process demo complete.\n\n");
    close_logger_win();
    return 0;
}
