#include "pkg.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run(const char *cmd, char *out, unsigned cap) {
    assert(pkg_execute(cmd, out, cap) == 1);
}
int main(void) {
    char out[512];
    pkg_init();
    assert(pkg_installed_count() == 2);
    run("store", out, sizeof(out));
    assert(strstr(out, "EMRESTORE") != 0);
    run("search calc", out, sizeof(out));
    assert(strstr(out, "calc") != 0);
    run("install calc", out, sizeof(out));
    assert(strstr(out, "installed metadata: calc") != 0);
    assert(pkg_installed_count() == 3);
    run("list", out, sizeof(out));
    assert(strstr(out, "calc 1.0.0 [installed]") != 0);
    run("remove base-utils", out, sizeof(out));
    assert(strstr(out, "Cannot remove a core package") != 0);
    run("remove calc", out, sizeof(out));
    assert(strstr(out, "removed package metadata: calc") != 0);
    assert(pkg_installed_count() == 2);
    run("update", out, sizeof(out));
    run("upgrade", out, sizeof(out));
    assert(strstr(out, "Binary package upgrades are not available yet") != 0);
    puts("NexoraPkg tests passed");
    return 0;
}
