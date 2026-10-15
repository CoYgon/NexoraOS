#include "pkg.h"
#include <stddef.h>

#define PKG_COUNT 9
#define PKG_DESC 58

struct package {
    const char *name;
    const char *version;
    const char *description;
    const char *dependency;
    int installed;
};

/* Built-in repository metadata. This is intentionally offline: there is no
 * network stack, signed repository format, or user-mode executable loader yet. */
static struct package packages[PKG_COUNT] = {
    {"base-utils", "1.0.0", "Core command utilities", "", 1},
    {"pkg", "1.0.0", "EmreOS package manager", "base-utils", 1},
    {"textedit", "0.3.0", "Small text editor application metadata", "base-utils", 0},
    {"calc", "1.0.0", "Desktop calculator application metadata", "base-utils", 0},
    {"sysmon", "0.5.0", "System monitor application metadata", "base-utils", 0},
    {"filetools", "1.2.0", "Additional file management commands", "base-utils", 0},
    {"devkit", "0.4.0", "C development tools package metadata", "base-utils", 0},
    {"docs", "1.0.0", "EmreOS manuals and help documents", "base-utils", 0},
    {"netdiag", "0.2.0", "Network diagnostics package metadata", "base-utils", 0}
};

static int repository_refreshed;

static int eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
}

static int starts(const char *s, const char *prefix) {
    while (*prefix) if (*s++ != *prefix++) return 0;
    return 1;
}

static void append(char *out, unsigned cap, unsigned *pos, const char *s) {
    if (!out || !cap) return;
    while (s && *s && *pos + 1 < cap) out[(*pos)++] = *s++;
    out[*pos] = 0;
}

static void line(char *out, unsigned cap, unsigned *pos, const char *s) {
    append(out, cap, pos, s);
    append(out, cap, pos, "\n");
}

static int find_package(const char *name) {
    for (int i = 0; i < PKG_COUNT; ++i)
        if (eq(packages[i].name, name)) return i;
    return -1;
}

static void install_index(int index, char *out, unsigned cap, unsigned *pos) {
    if (index < 0 || index >= PKG_COUNT) return;
    struct package *p = &packages[index];
    if (p->installed) {
        append(out, cap, pos, "already installed: ");
        line(out, cap, pos, p->name);
        return;
    }
    if (p->dependency[0]) {
        int dep = find_package(p->dependency);
        if (dep >= 0 && !packages[dep].installed) install_index(dep, out, cap, pos);
    }
    p->installed = 1;
    append(out, cap, pos, "installed metadata: ");
    append(out, cap, pos, p->name);
    append(out, cap, pos, " ");
    line(out, cap, pos, p->version);
}

void pkg_init(void) {
    /* Keep package state predictable whenever the kernel initializes. */
    for (int i = 0; i < PKG_COUNT; ++i)
        packages[i].installed = (i == 0 || i == 1) ? 1 : 0;
    repository_refreshed = 0;
}

unsigned pkg_installed_count(void) {
    unsigned count = 0;
    for (int i = 0; i < PKG_COUNT; ++i) if (packages[i].installed) ++count;
    return count;
}

int pkg_execute(const char *args, char *out, unsigned cap) {
    unsigned pos = 0;
    if (!args || !out || !cap) return 0;
    out[0] = 0;
    while (*args == ' ') ++args;
    if (!*args) {
        line(out, cap, &pos, "EmrePkg - package manager");
        line(out, cap, &pos, "pkg search <term> | list | info <name>");
        line(out, cap, &pos, "pkg install <name> | remove <name>");
        line(out, cap, &pos, "pkg update | upgrade | repos | autoremove | store");
        return 1;
    }
    if (eq(args, "help")) {
        line(out, cap, &pos, "pkg search <term> - search built-in catalog");
        line(out, cap, &pos, "pkg list - list installed packages");
        line(out, cap, &pos, "pkg info <name> - show package metadata");
        line(out, cap, &pos, "pkg install/remove <name>");
        line(out, cap, &pos, "pkg update/upgrade/repos/autoremove/store");
        return 1;
    }
    if (eq(args, "list")) {
        line(out, cap, &pos, "INSTALLED PACKAGES (RAM DATABASE):");
        for (int i = 0; i < PKG_COUNT; ++i) {
            if (packages[i].installed) {
                append(out, cap, &pos, packages[i].name);
                append(out, cap, &pos, " ");
                append(out, cap, &pos, packages[i].version);
                line(out, cap, &pos, " [installed]");
            }
        }
        return 1;
    }
    if (eq(args, "repos")) {
        line(out, cap, &pos, "emre-core: built-in offline catalog [enabled]");
        line(out, cap, &pos, "Remote repositories: unavailable (network/repo trust not implemented)");
        return 1;
    }
    if (eq(args, "store")) {
        line(out, cap, &pos, "EMRESTORE - BUILT-IN SOFTWARE CATALOG");
        for (int i = 0; i < PKG_COUNT; ++i) {
            append(out, cap, &pos, packages[i].name);
            append(out, cap, &pos, " ");
            append(out, cap, &pos, packages[i].version);
            append(out, cap, &pos, packages[i].installed ? " [installed] - " : " [available] - ");
            line(out, cap, &pos, packages[i].description);
        }
        line(out, cap, &pos, "Catalog only: package payloads, downloads, and executable installation are not implemented.");
        return 1;
    }
    if (eq(args, "update")) {
        repository_refreshed = 1;
        line(out, cap, &pos, "Package catalog checked: built-in index is current.");
        line(out, cap, &pos, "No network request was made.");
        return 1;
    }
    if (eq(args, "upgrade")) {
        if (!repository_refreshed) {
            line(out, cap, &pos, "Run 'pkg update' first.");
            return 1;
        }
        line(out, cap, &pos, "All installed package metadata is up to date.");
        line(out, cap, &pos, "Binary package upgrades are not available yet.");
        return 1;
    }
    if (eq(args, "autoremove")) {
        line(out, cap, &pos, "No orphaned packages found.");
        return 1;
    }
    if (starts(args, "search ")) {
        const char *term = args + 7;
        int found = 0;
        for (int i = 0; i < PKG_COUNT; ++i) {
            if (starts(packages[i].name, term) || starts(packages[i].description, term)) {
                append(out, cap, &pos, packages[i].name);
                append(out, cap, &pos, " ");
                append(out, cap, &pos, packages[i].version);
                append(out, cap, &pos, " - ");
                line(out, cap, &pos, packages[i].description);
                found = 1;
            }
        }
        if (!found) line(out, cap, &pos, "No matching package in built-in catalog.");
        return 1;
    }
    if (starts(args, "info ")) {
        int i = find_package(args + 5);
        if (i < 0) { line(out, cap, &pos, "Package not found."); return 1; }
        append(out, cap, &pos, "Name: "); line(out, cap, &pos, packages[i].name);
        append(out, cap, &pos, "Version: "); line(out, cap, &pos, packages[i].version);
        append(out, cap, &pos, "Status: "); line(out, cap, &pos, packages[i].installed ? "installed" : "not installed");
        append(out, cap, &pos, "Description: "); line(out, cap, &pos, packages[i].description);
        append(out, cap, &pos, "Dependency: "); line(out, cap, &pos, packages[i].dependency[0] ? packages[i].dependency : "none");
        line(out, cap, &pos, "Payload: catalog metadata only; executable loading is not implemented.");
        return 1;
    }
    if (starts(args, "install ")) {
        const char *name = args + 8;
        int i = find_package(name);
        if (i < 0) { line(out, cap, &pos, "Package not found. Try 'pkg search <term>'."); return 1; }
        if (packages[i].installed) { line(out, cap, &pos, "Package is already installed."); return 1; }
        install_index(i, out, cap, &pos);
        line(out, cap, &pos, "Note: this records package metadata; it does not install a runnable app yet.");
        return 1;
    }
    if (starts(args, "remove ")) {
        int i = find_package(args + 7);
        if (i < 0) { line(out, cap, &pos, "Package not found."); return 1; }
        if (i == 0 || i == 1) { line(out, cap, &pos, "Cannot remove a core package."); return 1; }
        if (!packages[i].installed) { line(out, cap, &pos, "Package is not installed."); return 1; }
        for (int j = 0; j < PKG_COUNT; ++j) {
            if (packages[j].installed && eq(packages[j].dependency, packages[i].name)) {
                line(out, cap, &pos, "Removal blocked: another installed package depends on it.");
                return 1;
            }
        }
        packages[i].installed = 0;
        append(out, cap, &pos, "removed package metadata: ");
        line(out, cap, &pos, packages[i].name);
        return 1;
    }
    line(out, cap, &pos, "Unknown pkg command. Type 'pkg help'.");
    return 1;
}
