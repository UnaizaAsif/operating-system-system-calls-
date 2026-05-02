/* memory_linux.c — syscalls: mmap, munmap */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "logger.h"
#define MAPSIZE 4096
#define MAPFILE "mmap_data.bin"

int main(void) {
    init_logger("syscalls.log");
    printf("=== Memory Demo (mmap/munmap) ===\n");

    int fd = safe_open(MAPFILE, O_RDWR|O_CREAT|O_TRUNC, 0644);
    if (fd < 0) { close_logger(); return 1; }
    if (ftruncate(fd, MAPSIZE) < 0) { log_error("ftruncate"); close_logger(); return 1; }
    log_success("ftruncate", MAPSIZE);

    void *map = mmap(NULL, MAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { log_error("mmap"); close_logger(); return 1; }
    log_success("mmap", 1);
    printf("[mmap]  Mapped %d bytes\n", MAPSIZE);

    const char *msg = "Memory-mapped I/O via mmap() syscall!";
    memcpy(map, msg, strlen(msg)+1);
    printf("[mmap]  Wrote: \"%s\"\n", (char*)map);
    printf("[mmap]  Read : \"%s\"\n", (char*)map);

    if (munmap(map, MAPSIZE) < 0) log_error("munmap");
    else                          log_success("munmap", 0);
    printf("[munmap] Done\n");

    safe_close(fd, "close(mmap_file)");
    printf("Memory demo complete.\n\n");
    close_logger(); return 0;
}
