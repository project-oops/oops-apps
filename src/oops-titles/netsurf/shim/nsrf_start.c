/*
 * Payload entry point.
 *
 * This calls NetSurf's `main` in `frontends/framebuffer/gui.c`, a C translation unit, so
 * the name is unmangled and a C shim reaches it.
 *
 * The framebuffer front end finds its resources - the default stylesheets, the fonts, the
 * toolbar images, `Choices` - on the path compiled in as `NETSURF_FB_RESPATH` and
 * `NETSURF_FB_FONTPATH` (`frontends/framebuffer/gui.c:2203`), which this build sets to
 * `/app0/res`, where the package mounts. Cookies and the URL database go under HOME.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include <stdlib.h>

void oops_crashtrace_install(void);

int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int netsurf_start(const payload_args_t *args);

__attribute__((visibility("default"))) int netsurf_start(const payload_args_t *args) {
    static char arg0[] = "/app0/netsurf";
    char *argv[] = {arg0, 0};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("NSRF", "entry");
    oops_crashtrace_install();

    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("NSRF", "HOME could not be set; cookies will not persist");
    }

    return main(1, argv);
}
