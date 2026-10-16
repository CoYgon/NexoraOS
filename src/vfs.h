#ifndef NEXORAOS_VFS_H
#define NEXORAOS_VFS_H

void vfs_init(void);
int vfs_ls(char *out, unsigned cap);
int vfs_cd(const char *name);
int vfs_mkdir(const char *name);
int vfs_touch(const char *name);
int vfs_write(const char *name, const char *text);
int vfs_cat(const char *name, char *out, unsigned cap);
int vfs_rm(const char *name);
int vfs_rmdir(const char *name);
int vfs_rename(const char *old_name, const char *new_name);
unsigned vfs_used_nodes(void);
unsigned vfs_total_nodes(void);
const char *vfs_pwd(void);
#endif
