/*
 * Host self-test for shell-skin.
 *
 * The composed userscript is the testable part: it must contain the pieces each recipe asks
 * for, be idempotent (so a re-apply cannot stack copies), and refuse a buffer too small rather
 * than emit a half-written script. Getting it into the shell is the console-only half and is
 * not exercised here.
 */

#include <stdio.h>
#include <string.h>

#include "shell-skin.h"

static int has(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

int main(void) {
    int ok = 1;
    char js[2048];

    size_t n = shell_skin_compose(js, sizeof(js),
                                  SHELL_SKIN_BACKGROUND | SHELL_SKIN_BANNER, 0x0d1116u);
    if (n == 0) {
        fprintf(stderr, "shell-skin selftest: compose returned nothing\n");
        ok = 0;
    }
    /* The background layer and its colour are present. */
    if (!has(js, "oops-skin-bg") || !has(js, "#0d1116")) {
        fprintf(stderr, "shell-skin selftest: background layer missing or wrong colour\n");
        ok = 0;
    }
    /* Idempotent: it checks for an existing element before adding. */
    if (!has(js, "getElementById")) {
        fprintf(stderr, "shell-skin selftest: script is not idempotent\n");
        ok = 0;
    }
    /* The banner recipe is present. */
    if (!has(js, "oops-skin-banner")) {
        fprintf(stderr, "shell-skin selftest: banner missing\n");
        ok = 0;
    }
    /* The store recipe, when off, leaves nothing behind. */
    if (has(js, "oops-skin-hide-store")) {
        fprintf(stderr, "shell-skin selftest: hide-store leaked in when not requested\n");
        ok = 0;
    }
    /* The reported length agrees with the string, and the script is closed. */
    if (strlen(js) != n || !has(js, "})();")) {
        fprintf(stderr, "shell-skin selftest: script is malformed\n");
        ok = 0;
    }

    /* Requesting hide-store adds its (honestly-placeholder) rule. */
    n = shell_skin_compose(js, sizeof(js), SHELL_SKIN_HIDE_STORE, 0);
    if (n == 0 || !has(js, "oops-skin-hide-store") || !has(js, "unconfirmed")) {
        fprintf(stderr, "shell-skin selftest: hide-store recipe wrong\n");
        ok = 0;
    }

    /* A buffer too small is refused (0), never a truncated script. */
    char tiny[16];
    if (shell_skin_compose(tiny, sizeof(tiny), SHELL_SKIN_BACKGROUND, 0) != 0) {
        fprintf(stderr, "shell-skin selftest: did not refuse a small buffer\n");
        ok = 0;
    }

    if (ok) {
        printf("shell-skin selftest: ok (userscript composes, is idempotent and bounded)\n");
        return 0;
    }
    return 1;
}
