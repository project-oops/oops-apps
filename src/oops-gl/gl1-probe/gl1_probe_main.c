/*
 * gl1-probe: the target payload.
 *
 * Runs the same check suite the host self-test runs, and reports each result to the console
 * log. **Comparing the two outputs is the whole point**: a check that passes on the host
 * software rasteriser and fails here is a bug in the hardware path, which is the one place
 * nothing else in this repository can look.
 *
 * It draws nothing to the screen and exits. A probe that stayed resident would need a stop
 * file and an input loop, and would be one more thing to get wrong; the result is in the log.
 */

#include "gl1_probe.h"

#include <oops/system.h>
#include <oops/syscall.h>

/* app.mk hands every app a build stamp; the probe reports it because the deploy is otherwise
 * unverifiable from this side. mkmodule normalises the ELF, so two builds can land at the same
 * byte count and an identical log then reads as "the fix did nothing" when the fix was never in
 * the running binary - which is exactly what happened chasing the blend bug on 2026-09-17. The
 * string is greppable in the staged ELF and printed in the log, so both ends can be checked. */
#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#ifndef OOPS_HOST_BUILD
static void probe_klog(const char *msg) {
    oops_klog("gl1-probe", msg);
}

/* Builds "  name            pass" without a printf, which a freestanding payload does not
 * have. Padded so a column of results reads as a column. */
static void report(const char *name, int passed) {
    char line[64];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 20; i++) line[at++] = name[i];
    while (at < 22) line[at++] = ' ';
    const char *verdict = passed ? "pass" : "FAIL";
    for (int i = 0; verdict[i] && at < (int)sizeof(line) - 1; i++) line[at++] = verdict[i];
    line[at] = '\0';
    probe_klog(line);
}

/*
 * The running commentary: each check named as it starts, and again with its verdict when it
 * ends. The name alone is what a hang leaves behind, and on 2026-09-20 a hang was the first
 * thing the first console run of this suite produced - nine frames and then nothing, with every
 * result still sitting in an array that was never printed.
 */
static void trace(const char *name, int verdict) {
    char line[64];
    int at = 0;
    const char *head = (verdict < 0) ? "-> " : "   ";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    for (int i = 0; name[i] && at < 24; i++) line[at++] = name[i];
    if (verdict >= 0) {
        while (at < 26) line[at++] = ' ';
        const char *v = verdict ? "pass" : "FAIL";
        for (int i = 0; v[i] && at < (int)sizeof(line) - 1; i++) line[at++] = v[i];
    }
    line[at] = '\0';
    probe_klog(line);
}

/* "   name                saw 0xff204060" - the colour a failing check left at the centre of the
 * probe region, in the byte order px() returns. Only failures reach here. */
static void saw(const char *name, uint32_t centre) {
    static const char hex[] = "0123456789abcdef";
    char line[64];
    int at = 0;
    line[at++] = ' '; line[at++] = ' '; line[at++] = ' ';
    for (int i = 0; name[i] && at < 24; i++) line[at++] = name[i];
    while (at < 26) line[at++] = ' ';
    const char *head = "saw 0x";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    for (int s = 28; s >= 0; s -= 4) line[at++] = hex[(centre >> s) & 0xfu];
    line[at] = '\0';
    probe_klog(line);
}

static void report_total(int passed, int ran) {
    char line[64];
    int at = 0;
    const char *head = "gl1-probe: ";
    for (int i = 0; head[i]; i++) line[at++] = head[i];
    line[at++] = (char)('0' + (passed / 10) % 10);
    line[at++] = (char)('0' + passed % 10);
    line[at++] = '/';
    line[at++] = (char)('0' + (ran / 10) % 10);
    line[at++] = (char)('0' + ran % 10);
    const char *tail = " passed on hardware";
    for (int i = 0; tail[i] && at < (int)sizeof(line) - 1; i++) line[at++] = tail[i];
    line[at] = '\0';
    probe_klog(line);
}
#endif

int gl1_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl1_probe_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    probe_klog("gl1-probe: running the OpenGL 1.x check suite [build " OOPS_APP_VERSION "]");
    gl1_probe_trace = trace;
    gl1_probe_saw = saw;
#else
    (void)args;
#endif

    gl1_probe_result_t results[GL1_PROBE_MAX_CASES];
    const int ran = gl1_probe_run(results, (int)(sizeof(results) / sizeof(results[0])));
    if (ran < 0) {
#ifndef OOPS_HOST_BUILD
        probe_klog("gl1-probe: no GL context; nothing measured");
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
     * **The answer arrived, and it is that nothing here may finish.**
     *
     * This used to return `(passed == ran) ? 0 : 1` and take a crash at the end of every run,
     * including the successful ones - `SIGSEGV`, `rip: 0x0`, the return value still in `rax`.
     * The note here said the right exit was a platform question rather than a guess, that it was
     * filed as `REQ-20260917T1450Z-2e71`, and that returning was what every other app did until
     * an answer came. It came, on 2026-09-17 at 16:15Z, and it is architectural:
     *
     *   - `exit`, `_Exit`, `sceKernelExit` and every shell-level kill: **absent**.
     *   - `_exit` is present and raises `SIGSYS`, because a `big-app` container's credentials do
     *     not permit FreeBSD syscall 1 - which is why `SYS_exit` "did not help" above.
     *   - returning faults at zero because the dynamic linker gives the entry point no caller
     *     frame, which is exactly what the old note observed.
     *
     * Process lifecycle belongs to `SceShellCore`, and the conforming ending is to print the
     * last line and idle while the host closes the app. `oops_system_park_until_closed` carries
     * that, and its comment carries the measurement.
     *
     * # What happened to the exit code
     *
     * Nothing that mattered. The old note wanted it so that "a loader that reports the exit code
     * says something useful without the log" - but there is no loader left to report one, since
     * the process cannot exit to hand one over. And the verdict was never only in the code:
     * `report_total` above prints `gl1-probe: NN/NN passed on hardware`, and the resolution names
     * **that exact line** as the completion sentinel a harness watches for. So the machine
     * readable result already lives in the log, and it is printed before this call, which is the
     * order this ending requires.
     *
     * The host build keeps returning, because a host process has a real lifecycle and a host test
     * that never returns is a hang rather than a result.
     */
#ifndef OOPS_HOST_BUILD
    oops_system_park_until_closed();
#else
    return (passed == ran) ? 0 : 1;
#endif
}
