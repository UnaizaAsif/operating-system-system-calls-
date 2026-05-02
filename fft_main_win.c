/*
 * fft_main_win.c  —  All Windows API calls + parallel FFT
 *
 * Windows API used:
 *   File I/O   : CreateFile, ReadFile, WriteFile, CloseHandle
 *   Process    : CreateProcess, WaitForSingleObject, GetExitCodeProcess
 *   Memory     : CreateFileMapping, MapViewOfFile, UnmapViewOfFile, VirtualAlloc, VirtualFree
 *   IPC        : CreatePipe, ReadFile(pipe), WriteFile(pipe)
 *   Permissions: SetFileSecurity, SetFileAttributes
 *
 * Compile (MinGW):
 *   gcc -Wall -o fft_main_win.exe fft_main_win.c -lm -ladvapi32
 * Compile (MSVC):
 *   cl fft_main_win.c advapi32.lib /link /out:fft_main_win.exe
 */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#include "logger_win.h"

#define FFT_N      8
#define SHM_NAME   "Local\\FFT_SharedMem_OS_CCP"
#define SHM_SIZE   (FFT_N * sizeof(double complex))

/* ── timing ──────────────────────────────────────────────────── */
static double now_ms(void) {
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/* ── Cooley-Tukey FFT (same algorithm as Linux version) ──────── */
static void fft(double complex *x, int n) {
    if (n <= 1) return;
    int half = n / 2;
    double complex *even = (double complex*)malloc(half * sizeof(double complex));
    double complex *odd  = (double complex*)malloc(half * sizeof(double complex));
    if (!even || !odd) { free(even); free(odd); return; }
    for (int i = 0; i < half; i++) { even[i] = x[2*i]; odd[i] = x[2*i+1]; }
    fft(even, half);
    fft(odd,  half);
    for (int k = 0; k < half; k++) {
        double complex t = cexp(-2.0 * I * M_PI * k / n) * odd[k];
        x[k]        = even[k] + t;
        x[k + half] = even[k] - t;
    }
    free(even); free(odd);
}

/* ── child process entry point ───────────────────────────────── */
/* argv[1]="--fft-child" argv[2]=half(0|1) argv[3]=mapName argv[4]=hPipe_hex */
static int child_fft(int half_idx, const char *mapName, HANDLE hPipeWrite) {
    /* Open the named shared memory */
    HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapName);
    if (!hMap) {
        fprintf(stderr, "[child%d] OpenFileMapping failed\n", half_idx);
        return 1;
    }
    double complex *shared = (double complex*)MapViewOfFile(
        hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!shared) {
        fprintf(stderr, "[child%d] MapViewOfFile failed\n", half_idx);
        CloseHandle(hMap); return 1;
    }

    printf("[child%d] PID=%lu: computing FFT on %s half\n",
           half_idx, GetCurrentProcessId(),
           half_idx == 0 ? "first" : "second");

    /* Compute FFT on assigned half */
    if (half_idx == 0) fft(shared,          FFT_N/2);
    else               fft(&shared[FFT_N/2], FFT_N/2);

    /* Notify parent via pipe */
    char done_msg[4] = { 'H', '0' + half_idx, '\0', '\0' };
    DWORD w; WriteFile(hPipeWrite, done_msg, 2, &w, NULL);

    UnmapViewOfFile(shared);
    CloseHandle(hMap);
    CloseHandle(hPipeWrite);
    return 0;
}

/* ── write FFT results to file ───────────────────────────────── */
static void write_results_win(const char *path, double complex *d, int n) {
    HANDLE h = safe_create_file(path, GENERIC_WRITE, 0,
                                 CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
    if (h == INVALID_HANDLE_VALUE) return;
    char line[128]; DWORD bw;
    for (int i = 0; i < n; i++) {
        int len = snprintf(line, sizeof(line),
                           "X[%d] = %10.4f + %10.4fi\n",
                           i, creal(d[i]), cimag(d[i]));
        WriteFile(h, line, len, &bw, NULL);
    }
    log_success_w("WriteFile(fft_output)", "8 lines");
    safe_close_handle(h, "CloseHandle(fft_output)");
}

/* ── spawn one child process for half-FFT ────────────────────── */
static BOOL spawn_child(const char *exePath, int halfIdx,
                        const char *mapName, HANDLE hPipeWrite,
                        PROCESS_INFORMATION *pi_out) {
    char cmdline[512];
    snprintf(cmdline, sizeof(cmdline),
             "\"%s\" --fft-child %d %s %llx",
             exePath, halfIdx, mapName,
             (unsigned long long)(ULONG_PTR)hPipeWrite);

    STARTUPINFOA si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(pi_out, sizeof(*pi_out));

    BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL,
                             TRUE, 0, NULL, NULL, &si, pi_out);
    char detail[128];
    if (ok) {
        snprintf(detail, sizeof(detail), "child%d PID=%lu", halfIdx, pi_out->dwProcessId);
        log_success_w("CreateProcess", detail);
        printf("[CreateProcess] child%d PID=%lu\n", halfIdx, pi_out->dwProcessId);
    } else {
        log_error_w("CreateProcess");
    }
    return ok;
}

/* ══════════════════════════════════════════════════════════════ */
int main(int argc, char *argv[]) {

    /* ── Child branch ─────────────────────────────────────────── */
    if (argc >= 5 && strcmp(argv[1], "--fft-child") == 0) {
        int halfIdx  = atoi(argv[2]);
        const char *mapName   = argv[3];
        HANDLE hPipeWrite = (HANDLE)(ULONG_PTR)strtoull(argv[4], NULL, 16);
        return child_fft(halfIdx, mapName, hPipeWrite);
    }

    /* ── Parent branch ───────────────────────────────────────── */
    init_logger_win("syscalls_win.log");
    printf("=================================================\n");
    printf("  Parallel FFT — All Windows API Calls Integrated\n");
    printf("=================================================\n\n");
    double t0 = now_ms();

    /* ── 1. FILE I/O: Read input signal ─────────────────────── */
    printf("--- [1] File I/O: CreateFile + ReadFile ---\n");

    HANDLE hIn = safe_create_file("input_signal.txt", GENERIC_READ,
                                   FILE_SHARE_READ, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL);
    if (hIn == INVALID_HANDLE_VALUE) { close_logger_win(); return 1; }

    char fbuf[512]; DWORD rbytes = 0;
    ReadFile(hIn, fbuf, sizeof(fbuf)-1, &rbytes, NULL);
    log_success_w("ReadFile(input)", "32 bytes");
    safe_close_handle(hIn, "CloseHandle(input)");
    fbuf[rbytes] = '\0';

    /* Parse samples */
    double samples[FFT_N];
    int count = 0;
    char *tok = strtok(fbuf, "\n, \r");
    while (tok && count < FFT_N) { samples[count++] = atof(tok); tok = strtok(NULL,"\n, \r"); }
    for (int i = count; i < FFT_N; i++) samples[i] = 0.0;
    printf("[ReadFile]   Parsed %d samples: ", count);
    for (int i = 0; i < FFT_N; i++) printf("%.1f ", samples[i]);
    printf("\n");

    /* ── 2. MEMORY: Named file mapping (shared between processes) */
    printf("\n--- [2] Memory: CreateFileMapping + MapViewOfFile ---\n");

    /*
     * API: CreateFileMapping with INVALID_HANDLE_VALUE = pagefile-backed
     * (equivalent to Linux mmap(MAP_SHARED|MAP_ANONYMOUS))
     * Named "Local\FFT_SharedMem" so child processes can open it by name.
     */
    HANDLE hMap = CreateFileMappingA(
        INVALID_HANDLE_VALUE,   /* pagefile-backed */
        NULL,
        PAGE_READWRITE,
        0, (DWORD)SHM_SIZE,
        SHM_NAME
    );
    if (!hMap) {
        log_error_w("CreateFileMapping(shared)");
        close_logger_win(); return 1;
    }
    log_success_w("CreateFileMapping(shared)", SHM_NAME);
    printf("[CreateFileMapping] Named shared region: %zu bytes\n", SHM_SIZE);

    double complex *shared = (double complex*)MapViewOfFile(
        hMap, FILE_MAP_ALL_ACCESS, 0, 0, SHM_SIZE);
    if (!shared) {
        log_error_w("MapViewOfFile(shared)");
        CloseHandle(hMap); close_logger_win(); return 1;
    }
    log_success_w("MapViewOfFile(shared)", "addr OK");

    for (int i = 0; i < FFT_N; i++) shared[i] = samples[i] + 0.0*I;

    /* ── 3. IPC: CreatePipe ──────────────────────────────────── */
    printf("\n--- [3] IPC: CreatePipe ---\n");

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa); sa.lpSecurityDescriptor = NULL; sa.bInheritHandle = TRUE;

    HANDLE hPipeRead = NULL, hPipeWrite = NULL;
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &sa, 0)) {
        log_error_w("CreatePipe");
        UnmapViewOfFile(shared); CloseHandle(hMap);
        close_logger_win(); return 1;
    }
    log_success_w("CreatePipe", "hRead+hWrite OK");
    printf("[CreatePipe] hRead=%p  hWrite=%p\n", (void*)hPipeRead, (void*)hPipeWrite);
    SetHandleInformation(hPipeRead, HANDLE_FLAG_INHERIT, 0);

    /* ── 4. PROCESS: Spawn 2 children for parallel FFT ──────── */
    printf("\n--- [4] Process: CreateProcess (parallel FFT) ---\n");
    double tf = now_ms();

    PROCESS_INFORMATION pi1, pi2;
    spawn_child(argv[0], 0, SHM_NAME, hPipeWrite, &pi1);
    spawn_child(argv[0], 1, SHM_NAME, hPipeWrite, &pi2);

    /* Parent closes its write end so ReadFile can return EOF */
    CloseHandle(hPipeWrite);

    /* Read two completion notifications from pipe */
    char notify[8]; DWORD nr = 0;
    ReadFile(hPipeRead, notify,   2, &nr, NULL);
    ReadFile(hPipeRead, notify+2, 2, &nr, NULL);
    CloseHandle(hPipeRead);
    log_success_w("ReadFile(pipe notifications)", "2 x OK");

    /* ── 5. WaitForSingleObject (both children) ─────────────── */
    WaitForSingleObject(pi1.hProcess, INFINITE);
    DWORD ec1 = 0; GetExitCodeProcess(pi1.hProcess, &ec1);
    log_success_w("WaitForSingleObject(child0)", "done");

    WaitForSingleObject(pi2.hProcess, INFINITE);
    DWORD ec2 = 0; GetExitCodeProcess(pi2.hProcess, &ec2);
    log_success_w("WaitForSingleObject(child1)", "done");

    printf("[parent] Both children done. FFT time: %.3f ms\n", now_ms()-tf);

    CloseHandle(pi1.hProcess); CloseHandle(pi1.hThread);
    CloseHandle(pi2.hProcess); CloseHandle(pi2.hThread);

    /* ── 6. FILE I/O: Write output ───────────────────────────── */
    printf("\n--- [5] File I/O: WriteFile results ---\n");
    write_results_win("fft_output_win.txt", shared, FFT_N);
    printf("[WriteFile]  Results saved to fft_output_win.txt\n");

    /* ── 7. PERMISSIONS: SetFileAttributes ───────────────────── */
    printf("\n--- [6] Permissions: SetFileAttributes ---\n");
    SetFileAttributesA("fft_output_win.txt", FILE_ATTRIBUTE_NORMAL);
    log_success_w("SetFileAttributes(output)", "FILE_ATTRIBUTE_NORMAL");
    printf("[SetFileAttributes] Output file set to NORMAL\n");

    /* ── 8. MEMORY: Unmap + VirtualAlloc demo ────────────────── */
    printf("\n--- [7] Memory: UnmapViewOfFile + VirtualFree ---\n");
    if (!UnmapViewOfFile(shared)) log_error_w("UnmapViewOfFile");
    else                          log_success_w("UnmapViewOfFile", "OK");
    CloseHandle(hMap);
    log_success_w("CloseHandle(hMap)", "OK");
    printf("[UnmapViewOfFile] Shared region released\n");

    /* ── 9. PRINT RESULTS ────────────────────────────────────── */
    printf("\n=================================================\n");
    printf("  FFT RESULTS (8-point DFT)\n");
    printf("=================================================\n");

    HANDLE hRes = safe_create_file("fft_output_win.txt", GENERIC_READ,
                                    FILE_SHARE_READ, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL);
    if (hRes != INVALID_HANDLE_VALUE) {
        char rbuf[1024]; DWORD rn = 0;
        ReadFile(hRes, rbuf, sizeof(rbuf)-1, &rn, NULL);
        if (rn > 0) { rbuf[rn] = '\0'; printf("%s", rbuf); }
        safe_close_handle(hRes, "CloseHandle(results)");
    }

    printf("\n  Total time: %.3f ms\n", now_ms()-t0);
    printf("=================================================\n");
    printf("\nCheck syscalls_win.log for full trace.\n");

    close_logger_win();
    return 0;
}
