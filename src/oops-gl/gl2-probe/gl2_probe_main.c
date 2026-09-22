/*
 * gl2-probe: the target payload.
 *
 * Runs the same check suite the host self-test runs, and reports each result to the console
 * log. **Comparing the two outputs is the whole point**: a check that passes on the host
 * software rasteriser and fails here is a bug in the console's GL 2.0 path, which is the one
 * place nothing else in this repository can look.
 *
 * The suite in `gl2_probe.c` is shared and unchanged - it was written for two runners before
 * either of them could run it on hardware, which is why nothing here had to be added to it.
 *
 * It draws nothing to the screen and exits. A probe that stayed resident would need a stop file
 * and an input loop, and would be one more thing to get wrong; the result is in the log.
 *
 * # Why this exists now
 *
 * The gate was never this file. It was that a compiled pixel shader had nowhere to run: the
 * draw path refused a program rather than drawing something the program did not ask for, so a
 * payload would have reported every drawing check as failed for one reason upstream of all of
 * them. That is no longer true - gl2-cube put a compiled GL 2.0 program on hardware through
 * `gl_draw.c` on 2026-09-21 - so the suite now measures the thing it was written to measure.
 */

#include "gl2_probe.h"

#include <oops/system.h>
#include <oops/syscall.h>

/* app.mk hands every app a build stamp; the probe reports it because the deploy is otherwise
 * unverifiable from this side. mkmodule normalises the ELF, so two builds can land at the same
 * byte count and an identical log then reads as "the fix did nothing" when the fix was never in
 * the running binary - which is exactly what happened chasing gl1-probe's blend bug on
 * 2026-09-17. The string is greppable in the staged ELF and printed in the log, so both ends
 * can be checked. */
#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#ifndef OOPS_HOST_BUILD
static void probe_klog(const char *msg) {
    oops_klog("gl2-probe", msg);
}

/* Builds "  name            pass" without a printf, which a freestanding payload does not
 * have. Padded so a column of results reads as a column. GL 2.0's check names are longer than
 * GL 1.x's - they carry a stage and an object - so the name field is wider than gl1-probe's and
 * the verdict still lands in one column. */
static void report(const char *name, int passed) {
    char line[80];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 34; i++) line[at++] = name[i];
    while (at < 36) line[at++] = ' ';
    const char *verdict = passed ? "pass" : "FAIL";
    for (int i = 0; verdict[i] && at < (int)sizeof(line) - 1; i++) line[at++] = verdict[i];
    line[at] = '\0';
    probe_klog(line);
}

/*
 * The running commentary: each check named as it starts, and again with its verdict when it
 * ends. The name alone is what a hang leaves behind, and on 2026-09-20 a hang was the first
 * thing the first console run of gl1-probe's suite produced - nine frames and then nothing,
 * with every result still sitting in an array that was never printed.
 *
 * A GL 2.0 run has more ways to hang than a GL 1.x one: a compiled shader is words this
 * repository generated, and a wave that does not retire takes the frame with it. So the
 * commentary matters more here, not less.
 */
static void trace(const char *name, int verdict) {
    char line[80];
    int at = 0;
    const char *head = (verdict < 0) ? "-> " : "   ";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    for (int i = 0; name[i] && at < 38; i++) line[at++] = name[i];
    if (verdict >= 0) {
        while (at < 40) line[at++] = ' ';
        const char *v = verdict ? "pass" : "FAIL";
        for (int i = 0; v[i] && at < (int)sizeof(line) - 1; i++) line[at++] = v[i];
    }
    line[at] = '\0';
    probe_klog(line);
}

/* "   name                saw 0xff204060 err 0x0502" - the colour a failing check left at the
 * centre of the probe region, in the byte order px() returns, and the first GL error it raised.
 * Only failures reach here.
 *
 * **The error is the half that says which kind of failure this is.** The reset colour at the
 * centre reads the same whether a draw was refused before it started or ran and put nothing
 * there, and those are different bugs with different fixes. `err 0x0000` and the reset colour
 * means the draw happened. */
static void saw(const char *name, uint32_t centre, unsigned int err) {
    static const char hex[] = "0123456789abcdef";
    char line[80];
    int at = 0;
    line[at++] = ' '; line[at++] = ' '; line[at++] = ' ';
    for (int i = 0; name[i] && at < 38; i++) line[at++] = name[i];
    while (at < 40) line[at++] = ' ';
    const char *head = "saw 0x";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    for (int s = 28; s >= 0; s -= 4) line[at++] = hex[(centre >> s) & 0xfu];
    const char *mid = " err 0x";
    for (int i = 0; mid[i]; i++) line[at++] = mid[i];
    for (int s = 12; s >= 0; s -= 4) line[at++] = hex[(err >> s) & 0xfu];
    line[at] = '\0';
    probe_klog(line);
}

/* **Three digits, because the suite is allowed a hundred and twenty-eight checks.** gl1-probe's
 * two-digit version would print 41/41 for a hundred and forty-one, which is the failure mode
 * that made `GL2_PROBE_MAX_CASES` exist in the first place. */
static void report_total(int passed, int ran) {
    char line[80];
    int at = 0;
    const char *head = "gl2-probe: ";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    line[at++] = (char)('0' + (passed / 100) % 10);
    line[at++] = (char)('0' + (passed / 10) % 10);
    line[at++] = (char)('0' + passed % 10);
    line[at++] = '/';
    line[at++] = (char)('0' + (ran / 100) % 10);
    line[at++] = (char)('0' + (ran / 10) % 10);
    line[at++] = (char)('0' + ran % 10);
    const char *tail = " passed on hardware";
    for (int i = 0; tail[i] && at < (int)sizeof(line) - 1; i++) line[at++] = tail[i];
    line[at] = '\0';
    probe_klog(line);
}
#endif

int gl2_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl2_probe_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    probe_klog("gl2-probe: running the OpenGL 2.0 check suite [build " OOPS_APP_VERSION "]");
    gl2_probe_trace = trace;
    gl2_probe_saw = saw;
#else
    (void)args;
#endif

    gl2_probe_result_t results[GL2_PROBE_MAX_CASES];
    const int ran = gl2_probe_run(results, (int)(sizeof(results) / sizeof(results[0])));
    if (ran < 0) {
#ifndef OOPS_HOST_BUILD
        probe_klog("gl2-probe: no GL context; nothing measured");
        oops_system_park_until_closed();
#endif
        return -1;
    }

    int passed = 0;
    for (int i = 0; i < ran; i++) {
        if (results[i].passed) passed++;
#ifndef OOPS_HOST_BUILD
        report(results[i].name, results[i].passed);
#endif
    }
#ifndef OOPS_HOST_BUILD
    report_total(passed, ran);
#endif

    /*
     * **Parked rather than returned**, which is not this probe's discovery but gl1-probe's:
     * `exit`, `_Exit` and `sceKernelExit` are absent, `_exit` raises `SIGSYS` because a
     * `big-app` container's credentials do not permit FreeBSD syscall 1, and returning faults
     * at zero because the dynamic linker gives the entry point no caller frame
     * (`REQ-20260917T1450Z-2e71`, resolved 2026-09-17T16:15Z). Process lifecycle belongs to
     * `SceShellCore`; the conforming ending is to print the last line and idle while the host
     * closes the app.
     *
     * The machine-readable verdict is the `gl2-probe: NNN/NNN passed on hardware` line above,
     * printed before this call because this call never comes back.
     *
     * The host build keeps returning, because a host process has a real lifecycle and a host
     * test that never returns is a hang rather than a result.
     */
#ifndef OOPS_HOST_BUILD
    oops_system_park_until_closed();
#else
    return (passed == ran) ? 0 : 1;
#endif
}
