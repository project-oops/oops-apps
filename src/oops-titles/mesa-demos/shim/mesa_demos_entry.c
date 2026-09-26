/*
 * mesa-demos entry point. A demo is a GLUT program and oops-sdk provides GLUT, so the
 * shim only calls `main`, first running `oops_mesa_run_init_array()` because no
 * start-up object walks `.init_array` and Mesa's constructors (ACO's opcode table)
 * must run.
 *
 * oops-sdk's GLUT calls `glutLeaveMainLoop` on OPTIONS, so `main` returns here.
 * A big-app cannot exit; it logs a last line and idles (oops_system_park_until_closed).
 */

#include "oops/system.h"

/* The demo's own entry, from `upstream/src/demos/<DEMO>.c`, which has no header. */
extern int main(int argc, char *argv[]);

/* oops-mesa's runtime shim (`src/runtime/abi.c`). */
extern void oops_mesa_run_init_array(void);

void mesa_demos_start(void);

void mesa_demos_start(void) {
    /* First, before anything reaches Mesa. */
    oops_mesa_run_init_array();

    /* A writable argv: a program name and no arguments, as `glutInit` expects. */
    static char arg0[] = "mesa-demos";
    static char *argv[] = {arg0, (char *)0};

    oops_klog("MESA-DEMOS",
              "starting " OOPS_DEMOS_NAME " - OPTIONS on the pad ends it");

    (void)main(1, argv);

    oops_klog("MESA-DEMOS", OOPS_DEMOS_NAME " returned; parking for the host to close");
    oops_system_park_until_closed();
}
