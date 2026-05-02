/*
 * permissions_win.c
 * Windows API equivalent of: chmod, chown, umask
 * Win32 API: SetFileSecurity (DACL), GetFileSecurity,
 *            SetFileAttributes, GetFileAttributes
 *
 * Compile (MinGW): gcc -Wall -o permissions_win.exe permissions_win.c -laclui -ladvapi32
 * Compile (MSVC):  cl permissions_win.c advapi32.lib
 */
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>
#include "logger_win.h"

#define TEST_FILE "perm_test_win.txt"

/*
 * Helper: create a simple DACL that grants GENERIC_ALL to Everyone.
 * This is the Windows equivalent of chmod(file, 0777).
 */
static BOOL set_everyone_full_access(const char *path) {
    PSID pSid = NULL;
    EXPLICIT_ACCESS_A ea;
    SID_IDENTIFIER_AUTHORITY worldAuth = SECURITY_WORLD_SID_AUTHORITY;

    /* Build the "Everyone" SID */
    if (!AllocateAndInitializeSid(&worldAuth, 1,
                                   SECURITY_WORLD_RID, 0,0,0,0,0,0,0,
                                   &pSid)) {
        log_error_w("AllocateAndInitializeSid");
        return FALSE;
    }

    ZeroMemory(&ea, sizeof(ea));
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfAccessMode        = SET_ACCESS;
    ea.grfInheritance       = NO_INHERITANCE;
    ea.Trustee.TrusteeForm  = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType  = TRUSTEE_IS_WELL_KNOWN_GROUP;
    ea.Trustee.ptstrName    = (LPSTR)pSid;

    PACL pDACL = NULL;
    DWORD ret = SetEntriesInAclA(1, &ea, NULL, &pDACL);
    if (ret != ERROR_SUCCESS) {
        log_error_w("SetEntriesInAcl");
        FreeSid(pSid);
        return FALSE;
    }

    /* -------------------------------------------------------
     * API CALL 1: SetFileSecurity  (equivalent to chmod())
     * Applies the new DACL to the file's security descriptor.
     * DACL_SECURITY_INFORMATION = we are setting the DACL.
     * ------------------------------------------------------- */
    SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, pDACL, FALSE);

    BOOL ok = SetFileSecurityA(path, DACL_SECURITY_INFORMATION, &sd);
    if (!ok) log_error_w("SetFileSecurity(DACL)");
    else     log_success_w("SetFileSecurity(DACL)", "Everyone=GENERIC_ALL");

    LocalFree(pDACL);
    FreeSid(pSid);
    return ok;
}

/*
 * Helper: read back the DACL and print it.
 * Equivalent to stat() permission check.
 */
static void read_dacl(const char *path) {
    PSECURITY_DESCRIPTOR pSD = NULL;
    PACL pDACL = NULL;
    BOOL daclPresent, daclDefaulted;

    /* -------------------------------------------------------
     * API CALL 2: GetFileSecurity  (equivalent to stat())
     * Retrieves the security descriptor for the file.
     * ------------------------------------------------------- */
    DWORD sdSize = 0;
    GetFileSecurityA(path, DACL_SECURITY_INFORMATION, NULL, 0, &sdSize);
    pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, sdSize);
    BOOL ok = GetFileSecurityA(path, DACL_SECURITY_INFORMATION,
                                pSD, sdSize, &sdSize);
    if (!ok) {
        log_error_w("GetFileSecurity");
        LocalFree(pSD);
        return;
    }
    log_success_w("GetFileSecurity", "DACL read OK");

    GetSecurityDescriptorDacl(pSD, &daclPresent, &pDACL, &daclDefaulted);
    if (daclPresent && pDACL) {
        printf("[GetFileSecurity] DACL present, ACE count=%u\n",
               (unsigned)pDACL->AceCount);
    }
    LocalFree(pSD);
}

int main(void) {
    init_logger_win("syscalls_win.log");
    printf("=== Permissions Demo (SetFileSecurity/GetFileSecurity/SetFileAttributes) ===\n");

    /* Create a test file */
    HANDLE hf = safe_create_file(TEST_FILE,
                                  GENERIC_WRITE, 0,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL);
    if (hf == INVALID_HANDLE_VALUE) { close_logger_win(); return 1; }
    DWORD bw;
    WriteFile(hf, "permissions test\n", 17, &bw, NULL);
    safe_close_handle(hf, "CloseHandle(test_file)");

    /* -------------------------------------------------------
     * chmod equivalent: SetFileSecurity with new DACL
     * ------------------------------------------------------- */
    printf("\n[chmod equiv] Setting Everyone=FULL_ACCESS on %s\n", TEST_FILE);
    set_everyone_full_access(TEST_FILE);

    /* Verify the DACL was applied */
    read_dacl(TEST_FILE);

    /* -------------------------------------------------------
     * API CALL 3: SetFileAttributes  (equivalent to chmod read-only bit)
     * Sets or clears FILE_ATTRIBUTE_READONLY.
     * On Linux this maps to removing write bits with chmod().
     * ------------------------------------------------------- */
    printf("\n[umask/attrib equiv] SetFileAttributes: NORMAL\n");
    BOOL ok = SetFileAttributesA(TEST_FILE, FILE_ATTRIBUTE_NORMAL);
    if (!ok) log_error_w("SetFileAttributes");
    else     log_success_w("SetFileAttributes", "FILE_ATTRIBUTE_NORMAL");

    /* -------------------------------------------------------
     * API CALL 4: GetFileAttributes  (read back attributes)
     * ------------------------------------------------------- */
    DWORD attrs = GetFileAttributesA(TEST_FILE);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        log_error_w("GetFileAttributes");
    } else {
        char detail[128];
        snprintf(detail, sizeof(detail), "attrs=0x%08lX (%s%s%s)",
                 attrs,
                 (attrs & FILE_ATTRIBUTE_READONLY)  ? "READONLY "  : "",
                 (attrs & FILE_ATTRIBUTE_HIDDEN)     ? "HIDDEN "    : "",
                 (attrs & FILE_ATTRIBUTE_NORMAL)     ? "NORMAL"     : "");
        log_success_w("GetFileAttributes", detail);
        printf("[GetFileAttributes] %s\n", detail);
    }

    printf("\nPermissions demo complete.\n\n");
    close_logger_win();
    return 0;
}
