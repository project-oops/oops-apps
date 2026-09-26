/*
 * mesa-demos: the entry point, and nothing else.
 *
 * This is the whole shim. A mesa-demos program is a GLUT program - `glutInit`, some
 * callbacks, `glutMainLoop` - and oops-sdk already provides GLUT, so there is nothing
 * to translate. What is left is the three things a console title needs that a desktop
 * program does not, and they are the same three `mesa-cube` names one at a time:
 *
 *   1. an entry point with the linker's name for it, because a payload has no `crt0` to
 * call `main` for it;
 *   2. `oops_mesa_run_init_array()`, because Mesa has global constructors and a hosted
 * title has no start-up object to walk `.init_array` (oops-mesa worklog 062: ACO's
 * opcode table stayed zeroed, every emitted instruction had opcode 0, and the GPU
 * faulted);
 *   3. parking instead of returning, because no userland call terminates a `big-app`
 * process (obSCEne `REQ-20260917T1450Z-2e71`).
 *
 * **Which GLUT answers, and why that is the point of this title.** `oops-sdk`'s
 * `glut.c` goes entirely through `oops/gfx.h` - it names no backend anywhere - so
 * `OOPS_RENDERER` in the Makefile decides whether these demos run on oops-gl or on
 * upstream Mesa. This title sets `mesa`. It is the first thing in the collection that
 * is not our own code running on oops-mesa.
 *
 * **How a run ends.** `glutMainLoop` returns when something calls `glutLeaveMainLoop`,
 * which oops-sdk's GLUT does on OPTIONS. So a demo is closed with the pad, control
 * comes back here, and the title parks. No stop file, and nothing to clean up from the
 * last run - which is worth having after a stale `/app0/stop` cost a session a
 * confusing one-frame capture.
 */

#include "oops/system.h"

/* The demo's own entry, compiled from `upstream/src/demos/<DEMO>.c` unmodified.
 * Declared rather than included: there is no header, and the signature is the same in
 * all 56 of them. */
extern int main(int argc, char *argv[]);

/* oops-mesa's runtime shim (`src/runtime/abi.c`). */
extern void oops_mesa_run_init_array(void);

void mesa_demos_start(void);

void mesa_demos_start(void) {
    /* First, before anything reaches Mesa. */
    oops_mesa_run_init_array();

    /*
     * `glutInit(&argc, argv)` wants a writable argv and a program name in it; several
     * demos read `argv[0]` for their window title and a few parse flags. One entry and
     * a NULL terminator is what a desktop shell would hand a program invoked with no
     * arguments, so that is what this hands it. Not `const`, because GLUT's signature
     * is `char **`.
     */
    static char arg0[] = "mesa-demos";
    static char *argv[] = {arg0, (char *)0};

    oops_klog("MESA-DEMOS",
              "starting " OOPS_DEMOS_NAME " - OPTIONS on the pad ends it");

    (void)main(1, argv);

    oops_klog("MESA-DEMOS", OOPS_DEMOS_NAME " returned; parking for the host to close");
    oops_system_park_until_closed();
}
