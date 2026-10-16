#include "vfs.h"
#include <stddef.h>

#define VFS_NODES 48
#define VFS_NAME 24
#define VFS_DATA 192

struct node {
    int used;
    int dir;
    int parent;
    char name[VFS_NAME];
    char data[VFS_DATA];
};

static struct node nodes[VFS_NODES];
static int cwd;
static char path[64];

static size_t slen(const char *s) {
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

static int eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void cp(char *d, const char *s, size_t n) {
    size_t i = 0;
    if (!n) return;
    while (s && s[i] && i + 1 < n) { d[i] = s[i]; ++i; }
    d[i] = 0;
}

static void append(char *out, unsigned cap, unsigned *pos, const char *s) {
    if (!cap) return;
    while (s && *s && *pos + 1 < cap) out[(*pos)++] = *s++;
    out[*pos] = 0;
}

static int find(int parent, const char *name) {
    for (int i = 1; i < VFS_NODES; ++i)
        if (nodes[i].used && nodes[i].parent == parent && eq(nodes[i].name, name))
            return i;
    return -1;
}

static void rebuild_path(void) {
    int chain[VFS_NODES];
    unsigned depth = 0, pos = 1;
    path[0] = '/'; path[1] = 0;
    int n = cwd;
    while (n > 0 && depth < VFS_NODES) {
        chain[depth++] = n;
        n = nodes[n].parent;
    }
    while (depth) {
        const char *name = nodes[chain[--depth]].name;
        size_t len = slen(name);
        if (pos > 1 && path[pos - 1] != '/' && pos + 1 < sizeof(path)) path[pos++] = '/';
        for (size_t i = 0; i < len && pos + 1 < sizeof(path); ++i) path[pos++] = name[i];
        path[pos] = 0;
    }
    if (pos == 1) path[1] = 0;
}

void vfs_init(void) {
    for (int i = 0; i < VFS_NODES; ++i) nodes[i].used = 0;
    nodes[0].used = 1;
    nodes[0].dir = 1;
    nodes[0].parent = 0;
    cp(nodes[0].name, "/", VFS_NAME);
    nodes[0].data[0] = 0;
    cwd = 0;
    cp(path, "/", sizeof(path));
}

static int create(const char *name, int dir) {
    if (!name || !*name || slen(name) >= VFS_NAME || eq(name, ".") ||
        eq(name, "..") || name[0] == '/' || find(cwd, name) >= 0)
        return -1;
    for (int i = 1; i < VFS_NODES; ++i) {
        if (!nodes[i].used) {
            nodes[i].used = 1;
            nodes[i].dir = dir;
            nodes[i].parent = cwd;
            cp(nodes[i].name, name, VFS_NAME);
            nodes[i].data[0] = 0;
            return i;
        }
    }
    return -1;
}

int vfs_ls(char *out, unsigned cap) {
    unsigned pos = 0;
    int count = 0;
    if (!out || !cap) return -1;
    out[0] = 0;
    for (int i = 1; i < VFS_NODES; ++i) {
        if (nodes[i].used && nodes[i].parent == cwd) {
            append(out, cap, &pos, nodes[i].name);
            append(out, cap, &pos, nodes[i].dir ? "/\n" : "\n");
            ++count;
        }
    }
    if (!count) append(out, cap, &pos, "(empty)\n");
    return count;
}

int vfs_cd(const char *name) {
    if (!name || !*name || eq(name, "/")) cwd = 0;
    else if (eq(name, "..")) {
        if (cwd) cwd = nodes[cwd].parent;
    } else {
        int i = find(cwd, name);
        if (i < 0 || !nodes[i].dir) return -1;
        cwd = i;
    }
    rebuild_path();
    return 0;
}

int vfs_mkdir(const char *name) { return create(name, 1) >= 0 ? 0 : -1; }
int vfs_touch(const char *name) { return create(name, 0) >= 0 ? 0 : -1; }

int vfs_write(const char *name, const char *text) {
    int i = find(cwd, name);
    if (i < 0) i = create(name, 0);
    if (i < 0 || nodes[i].dir) return -1;
    cp(nodes[i].data, text ? text : "", VFS_DATA);
    return 0;
}

int vfs_cat(const char *name, char *out, unsigned cap) {
    int i = find(cwd, name);
    if (i < 0 || nodes[i].dir || !out || !cap) return -1;
    cp(out, nodes[i].data, cap);
    return (int)slen(out);
}

int vfs_rm(const char *name) {
    int i = find(cwd, name);
    if (i < 1 || nodes[i].dir) return -1;
    nodes[i].used = 0;
    return 0;
}

int vfs_rmdir(const char *name) {
    int i = find(cwd, name);
    if (i < 1 || !nodes[i].dir || i == cwd) return -1;
    for (int j = 1; j < VFS_NODES; ++j)
        if (nodes[j].used && nodes[j].parent == i) return -1;
    nodes[i].used = 0;
    return 0;
}

int vfs_rename(const char *old_name, const char *new_name) {
    int i = find(cwd, old_name);
    if (i < 1 || !new_name || !*new_name || slen(new_name) >= VFS_NAME ||
        eq(new_name, ".") || eq(new_name, "..") || new_name[0] == '/' ||
        find(cwd, new_name) >= 0) return -1;
    cp(nodes[i].name, new_name, VFS_NAME);
    rebuild_path();
    return 0;
}

unsigned vfs_used_nodes(void) {
    unsigned n = 0;
    for (int i = 1; i < VFS_NODES; ++i) if (nodes[i].used) ++n;
    return n;
}

unsigned vfs_total_nodes(void) { return VFS_NODES - 1; }
const char *vfs_pwd(void) { return path; }
