/*
 * dri-probe - brings GL up through the Gallium DRI frontend and says how far it gets.
 *
 * # What this is for
 *
 * oops-mesa's platform shim has existed since 2026-09-17 and nothing has ever called it. It
 * compiles, it links, and a title packages with the whole frontend in it - but `oops_gl_create`
 * has never run, so every statement about it is a statement about source code (oops-mesa worklog
 * 041 and 042 both say so in as many words). This is the title that changes that.
 *
 * It is the frontend's counterpart to `mesa-probe`, which walks the winsys path directly. The two
 * are separate titles on purpose: the frontend creates its own screen, so one title doing both
 * would have two screens and no clean attribution for a failure. `mesa-probe` stays the control.
 *
 * # What it reports, and the order is the point
 *
 * Every step below is a line in the log, and the last line that appears is the answer. The
 * frontend's own failures are reported by the shim, which names the step it stopped on, so this
 * file does not duplicate that - it reports what the shim could not know: whether the handle came
 * back, whether GL then answers, and whether presentation is refused for the reason it is
 * expected to be refused for.
 *
 * Nothing here renders. A frame that did not retire is a failure and never a fallback to software
 * (CLAUDE.md, principle 4), and the flip half of presentation is behind an unanswered hardware
 * question - so this title deliberately stops at the first GL call rather than drawing something
 * and reporting a colour nobody can verify.
 */

#include "oops/system.h"

#include "oops_platform.h"

/*
 * GL's own headers, from the same `mesa/include` a title already compiles against.
 *
 * They are included with the conversion warnings off for the reason mesa-probe's includes are:
 * this title compiles at `-Wconversion -Wsign-conversion -Werror`, and upstream's headers are
 * not ours to make clean. The suppression covers the includes and nothing after them.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wsign-conversion"
#include <GL/gl.h>
#pragma clang diagnostic pop

#include <stdint.h>

static void say(const char *msg)
{
    oops_klog("DRI-PROBE", msg);
}

/*
 * How a title finishes here, which is by not finishing.
 *
 * The measurement is obSCEne `REQ-20260917T1450Z-2e71` and it is written up where the shared
 * helper is declared, in `oops-sdk/include/oops/system.h`: no userland call terminates a
 * `big-app` process, `_exit` raises `SIGSYS` for want of permission on syscall 1, and returning
 * faults at `rip: 0x0` because the dynamic linker provides no caller frame. Printing the last
 * line and idling for the host to close the app is the conforming pattern, and it produces no
 * coredump, no crash report and no hung GPU ring.
 *
 * Before this was known, every title here returned and took a crash report at the end of a
 * successful run. It was always *after* the results, so it cost no measurement - but it cost a
 * clean log tail, and it made a good run look like a bad one.
 *
 * The line is said here so it carries this title's tag; the idling is the helper's.
 */
_Noreturn static void park(void)
{
    say("idle and finished - close this title from the host");
    oops_system_park_until_closed();
}

/*
 * The extent, and why it is small.
 *
 * 1280x720 is not a display mode here and does not have to be: the extent is the title's to
 * choose and the compositor scales, which is the cheaper path at 4K and is the reason
 * `oops_gl_create` takes it as an argument rather than reading it from the display
 * (oops-mesa D009, and the note on `oops_gl_create`).
 *
 * Small is deliberate for a first run. The drawable's colour buffer is allocated by Mesa through
 * `dri_create_image`, and this is the first time anything will have asked the winsys for GPU
 * memory on this path - worklog 022 established the startup path allocates none. A 3.5 MB buffer
 * failing is easier to read than a 33 MB one, and nothing about the path under test changes with
 * the size.
 */
#define PROBE_WIDTH  1280u
#define PROBE_HEIGHT 720u

/*
 * Report one GL string.
 *
 * `glGetString` is the cheapest call that proves the whole dispatch chain, and it is worth being
 * precise about what a null answer means. The frontend's GL entry points reach the driver through
 * libglapi's **thread-local** dispatch table, so a null here does not say "GL is broken" - it says
 * the table this thread sees has no function in that slot, which on this platform is the thread
 * local storage question worklog 040 settled. That is a different failure from a context that
 * never became current, and `oops_gl_create` has already reported the second kind if it happened.
 *
 * So: the string is the interesting result, and null is reported as null rather than smoothed
 * over.
 */
static void report_string(const char *label, GLenum name)
{
    const GLubyte *s = glGetString(name);

    if (s == NULL) {
        oops_klog("DRI-PROBE", label);
        say("  ... came back null: this thread's dispatch table has no entry for it");
        return;
    }
    oops_klog("DRI-PROBE", label);
    oops_klog("DRI-PROBE", (const char *)s);
}

void dri_probe_start(void);

void dri_probe_start(void)
{
    say("bringing GL up through the DRI frontend (v" OOPS_APP_VERSION ")");

    struct oops_gl *gl = oops_gl_create(PROBE_WIDTH, PROBE_HEIGHT);

    if (gl == NULL) {
        /*
         * States no cause, deliberately. `oops_gl_create` logs the step it stopped on - the
         * winsys, the screen, the config, the drawable, the context or make-current - and that
         * line is the result. Adding a guess here would put two accounts of one failure in the
         * log, and the shim's is the one with the information.
         */
        say("GL did not come up; the shim's last line above names the step");
        say("done");
        park();
    }

    say("oops_gl_create returned a handle: this is the first time that has happened");

    uint32_t w = 0;
    uint32_t h = 0;
    oops_gl_extent(gl, &w, &h);
    if (w == PROBE_WIDTH && h == PROBE_HEIGHT) {
        say("the drawable reports the extent that was asked for");
    } else {
        say("the drawable reports a different extent than was asked for");
    }

    /* The first GL call ever made on this platform, whatever it answers. */
    report_string("GL_VERSION:", GL_VERSION);
    report_string("GL_RENDERER:", GL_RENDERER);
    report_string("GL_VENDOR:", GL_VENDOR);

    /*
     * Presentation, which is expected to refuse.
     *
     * `oops_gl_present` does the flush half and not the flip half: the flip needs the buffer
     * registered with the display controller, and whether `sceVideoOutRegisterBuffers2`
     * constrains a buffer's address is the open unknown on roadmap unit 6 - a question that cannot
     * be asked until a surface exists, which is what this title is creating.
     *
     * It is called anyway. The flush half runs, which means the frontend finishes the frame and
     * calls back into the shim's `getBuffers`, and that callback is where Mesa allocates the
     * colour buffer. So a refusal here is a *successful* exercise of the callback path, and the
     * shim's own line distinguishes the two. `true` would be the surprise.
     */
    if (oops_gl_present(gl)) {
        say("presentation reported success, which is not expected yet - read the shim's lines");
    } else {
        say("presentation refused, as expected: the flip half waits on a hardware question");
    }

    oops_gl_destroy(gl);
    say("torn down");

    say("done");
    park();
}
