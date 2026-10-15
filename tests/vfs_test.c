#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "vfs.h"

int main(void) {
    char buffer[256];
    vfs_init();
    assert(strcmp(vfs_pwd(), "/") == 0);
    assert(vfs_mkdir("docs") == 0);
    assert(vfs_cd("docs") == 0);
    assert(strcmp(vfs_pwd(), "/docs") == 0);
    assert(vfs_mkdir("notes") == 0);
    assert(vfs_cd("notes") == 0);
    assert(strcmp(vfs_pwd(), "/docs/notes") == 0);
    assert(vfs_write("readme.txt", "EmreOS VFS test") == 0);
    assert(vfs_cat("readme.txt", buffer, sizeof(buffer)) >= 0);
    assert(strcmp(buffer, "EmreOS VFS test") == 0);
    assert(vfs_rename("readme.txt", "info.txt") == 0);
    assert(vfs_cat("info.txt", buffer, sizeof(buffer)) >= 0);
    assert(strcmp(buffer, "EmreOS VFS test") == 0);
    assert(vfs_cd("..") == 0 && strcmp(vfs_pwd(), "/docs") == 0);
    assert(vfs_rmdir("notes") != 0); /* non-empty */
    assert(vfs_cd("notes") == 0);
    assert(vfs_rm("info.txt") == 0);
    assert(vfs_cd("..") == 0);
    assert(vfs_rmdir("notes") == 0);
    assert(vfs_cd("..") == 0 && strcmp(vfs_pwd(), "/") == 0);
    assert(vfs_used_nodes() == 1);
    assert(vfs_total_nodes() == 47);
    puts("EmreOS VFS tests passed");
    return 0;
}
