/*
 * memory_win.c
 * Windows API equivalents of: mmap, munmap
 * Win32 API: VirtualAlloc, VirtualFree
 *            + CreateFileMapping / MapViewOfFile (file-backed)
 *
 * Compile (MinGW): gcc -Wall -o memory_win.exe memory_win.c
 * Compile (MSVC):  cl memory_win.c
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "logger_win.h"

#define ALLOC_SIZE  4096
#define MAP_FILE    "mmap_data_win.bin"

int main(void) {
    init_logger_win("syscalls_win.log");
    printf("=== Memory Demo (VirtualAlloc/VirtualFree + CreateFileMapping/MapViewOfFile) ===\n");

    /* =======================================================
     * PART A: VirtualAlloc / VirtualFree
     * (equivalent to mmap(NULL, size, PROT_READ|PROT_WRITE,
     *                     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0))
     * ======================================================= */
    printf("\n-- VirtualAlloc (anonymous memory) --\n");

    /* -------------------------------------------------------
     * API CALL 1: VirtualAlloc
     * lpAddress       = NULL  (OS chooses address)
     * dwSize          = bytes to allocate (rounded to page)
     * flAllocationType= MEM_COMMIT|MEM_RESERVE
     *   MEM_RESERVE   = reserve address space range
     *   MEM_COMMIT    = back with physical pages / pagefile
     * flProtect       = PAGE_READWRITE (read + write access)
     * ------------------------------------------------------- */
    LPVOID pMem = VirtualAlloc(
        NULL,
        ALLOC_SIZE,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (!pMem) {
        log_error_w("VirtualAlloc");
        close_logger_win(); return 1;
    }
    char detail[128];
    snprintf(detail, sizeof(detail), "addr=0x%p size=%d", pMem, ALLOC_SIZE);
    log_success_w("VirtualAlloc", detail);
    printf("[VirtualAlloc]  Allocated %d bytes at %p\n", ALLOC_SIZE, pMem);

    /* Write and read through the virtual allocation */
    const char *msg = "VirtualAlloc memory — Windows anonymous allocation!";
    memcpy(pMem, msg, strlen(msg) + 1);
    printf("[VirtualAlloc]  Wrote: \"%s\"\n", (char*)pMem);
    printf("[VirtualAlloc]  Read : \"%s\"\n", (char*)pMem);

    /* -------------------------------------------------------
     * API CALL 2: VirtualFree  (equivalent to munmap())
     * dwFreeType = MEM_RELEASE  (release entire region)
     * dwSize     = 0 when using MEM_RELEASE
     * ------------------------------------------------------- */
    if (!VirtualFree(pMem, 0, MEM_RELEASE)) {
        log_error_w("VirtualFree");
    } else {
        log_success_w("VirtualFree", "MEM_RELEASE OK");
        printf("[VirtualFree]   Memory released\n");
    }

    /* =======================================================
     * PART B: CreateFileMapping / MapViewOfFile / UnmapViewOfFile
     * (equivalent to mmap() with a real file backing)
     * ======================================================= */
    printf("\n-- CreateFileMapping + MapViewOfFile (file-backed) --\n");

    /* Create backing file */
    HANDLE hFile = safe_create_file(
        MAP_FILE,
        GENERIC_READ | GENERIC_WRITE,
        0,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL
    );
    if (hFile == INVALID_HANDLE_VALUE) { close_logger_win(); return 1; }

    /* Extend file to ALLOC_SIZE */
    SetFilePointer(hFile, ALLOC_SIZE, NULL, FILE_BEGIN);
    SetEndOfFile(hFile);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    log_success_w("SetEndOfFile", "4096 bytes");

    /* -------------------------------------------------------
     * API CALL 3: CreateFileMapping
     * Creates a named or unnamed file mapping object.
     * hFile            = backing file handle
     * flProtect        = PAGE_READWRITE
     * dwMaximumSizeHigh/Low = max mapping size (0 = use file size)
     * lpName           = NULL (unnamed mapping)
     * ------------------------------------------------------- */
    HANDLE hMap = CreateFileMappingA(
        hFile,
        NULL,
        PAGE_READWRITE,
        0, ALLOC_SIZE,
        NULL
    );
    if (!hMap) {
        log_error_w("CreateFileMapping");
        CloseHandle(hFile);
        close_logger_win(); return 1;
    }
    log_success_w("CreateFileMapping", "PAGE_READWRITE OK");
    printf("[CreateFileMapping] Created file mapping object\n");

    /* -------------------------------------------------------
     * API CALL 4: MapViewOfFile  (equivalent to mmap() address return)
     * Maps the file mapping into the process's address space.
     * dwDesiredAccess = FILE_MAP_ALL_ACCESS
     * dwFileOffsetHigh/Low = starting offset in file (0 = start)
     * dwNumberOfBytesToMap = 0 = map entire file
     * ------------------------------------------------------- */
    LPVOID pView = MapViewOfFile(
        hMap,
        FILE_MAP_ALL_ACCESS,
        0, 0,
        ALLOC_SIZE
    );
    if (!pView) {
        log_error_w("MapViewOfFile");
        CloseHandle(hMap); CloseHandle(hFile);
        close_logger_win(); return 1;
    }
    snprintf(detail, sizeof(detail), "addr=0x%p", pView);
    log_success_w("MapViewOfFile", detail);
    printf("[MapViewOfFile] Mapped file at %p\n", pView);

    const char *msg2 = "File-mapped memory via CreateFileMapping + MapViewOfFile!";
    memcpy(pView, msg2, strlen(msg2) + 1);
    printf("[MapViewOfFile] Wrote: \"%s\"\n", (char*)pView);
    printf("[MapViewOfFile] Read : \"%s\"\n", (char*)pView);

    /* -------------------------------------------------------
     * API CALL 5: UnmapViewOfFile  (equivalent to munmap())
     * ------------------------------------------------------- */
    if (!UnmapViewOfFile(pView)) {
        log_error_w("UnmapViewOfFile");
    } else {
        log_success_w("UnmapViewOfFile", "OK");
        printf("[UnmapViewOfFile] View unmapped\n");
    }

    CloseHandle(hMap);
    log_success_w("CloseHandle(hMap)", "OK");
    safe_close_handle(hFile, "CloseHandle(hFile)");

    printf("Memory demo complete.\n\n");
    close_logger_win();
    return 0;
}
