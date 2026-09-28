/*
 * Payload entry point.
 *
 * NetSurf's framebuffer front end is an ordinary program - `main` in
 * `frontends/framebuffer/gui.c` - so this sets up what a process would have and calls
 * it with the arguments that pick its surface: `-f sdl`, libnsfb's SDL 1.2 surface,
 * which `sdl12-compat` puts on SDL2. Resources (`Messages`, the style sheets,
 * `Choices`) are found under `/app0/res`, which the Makefile's `NETSURF_FB_RESPATH`
 * names and `make package` fills.
 *
 * The window asked for is 1280x720 rather than the screen's 1920x1080: NetSurf's
 * internal font is a fixed bitmap, and the smaller surface keeps it larger on screen
 * if SDL scales the window up. Whether it does is for the first run to show.
 */
#include "oops/syscall.h"
#include "oops/system.h"

#include <stdlib.h>

void oops_crashtrace_install(void);
int main(int argc, char **argv);

/* app.mk derives the entry symbol from the app name; lld only warns on a mismatch. */
int netsurf_start(const payload_args_t *args);

__attribute__((visibility("default"))) int netsurf_start(const payload_args_t *args) {
    static char arg0[] = "/app0/nsfb";
    static char opt_f[] = "-f";
    static char surface[] = "sdl";
    static char opt_w[] = "-w";
    static char width[] = "1280";
    static char opt_h[] = "-h";
    static char height[] = "720";
    char *argv[] = {arg0, opt_f, surface, opt_w, width, opt_h, height, NULL};

    (void)args;

    oops_log_init(OOPS_APP_ID);
    /* The kernel log drops bursts and this start-up is one. */
    (void)oops_log_enable_disk_sink(OOPS_APP_ID, 0);
    oops_log_info("NSRF", "entry");
    oops_crashtrace_install();

    if (setenv("HOME", OOPS_POSIX_HOME, 1) != 0) {
        oops_log_error("NSRF", "HOME could not be set");
    }

    /* libnsfb registers its surfaces from constructors, and .init_array is not walked
       for this payload. */
    oops_run_init_array();

    /* `exit`, not `return`: the loader called this and has nowhere to return to. */
    exit(main(7, argv));
    return 0; /* not reached; oops-sdk's `exit` is not declared noreturn */
}
