/* permissions_linux.c — syscalls: chmod, chown, umask */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "logger.h"
#define TESTFILE "perm_test.txt"

int main(void) {
    init_logger("syscalls.log");
    printf("=== Permissions Demo (chmod/chown/umask) ===\n");

    int fd = safe_open(TESTFILE, O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if (fd < 0) { close_logger(); return 1; }
    write(fd, "test\n", 5);
    safe_close(fd, "close(perm_test)");

    if (chmod(TESTFILE, 0755) < 0) log_error("chmod");
    else { log_success("chmod(0755)", 0755);
           printf("[chmod]  0755 applied to %s\n", TESTFILE); }

    uid_t uid = getuid(); gid_t gid = getgid();
    if (chown(TESTFILE, uid, gid) < 0) log_error("chown");
    else { log_success("chown", uid);
           printf("[chown]  uid=%d gid=%d\n", uid, gid); }

    mode_t old = umask(0022);
    log_success("umask(0022)", old);
    printf("[umask]  Old=0%03o New=0022\n", old);

    struct stat st;
    if (stat(TESTFILE, &st)==0)
        printf("[stat]   mode=0%03o uid=%d\n",(int)(st.st_mode&07777),(int)st.st_uid);

    umask(old);
    printf("Permissions demo complete.\n\n");
    close_logger(); return 0;
}
