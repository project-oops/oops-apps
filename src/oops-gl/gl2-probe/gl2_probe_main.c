/*
 * gl2-probe: the target payload.
 *
 * Runs the shared check suite in `gl2_probe.c` and reports each result to the console
 * log. A check that passes on the host software reference and fails here is a fault in
 * the console's GL 2.0 path. The result is in the log; the payload has no input loop.
 */

#include "gl2_probe.h"

#include <oops/system.h>
#include <oops/syscall.h>

/* app.mk hands every app a build stamp. mkmodule normalises the ELF, so two builds can
 * have the same byte count; the stamp is greppable in the staged ELF and printed in the
 * log, which proves which build ran. */
#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#ifndef OOPS_HOST_BUILD
#define PROBE_TAG "gl2-probe"

/* Builds "  name            pass" without a printf, which a freestanding payload does
 * not have. Padded so a column of results reads as a column; the name field is wide
 * because GL 2.0 check names carry a stage and an object. */
static void report(const char *name, int passed) {
    char line[80];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 34; i++)
        line[at++] = name[i];
    while (at < 36)
        line[at++] = ' ';
    const char *verdict = passed ? "pass" : "FAIL";
    for (int i = 0; verdict[i] && at < (int)sizeof(line) - 1; i++)
        line[at++] = verdict[i];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/* Names each check as it starts and again with its verdict when it ends, so a hang -
 * such as a generated shader whose wave never retires - leaves the name of the check
 * that hung in the log. */
static void trace(const char *name, int verdict) {
    char line[80];
    int at = 0;
    const char *head = (verdict < 0) ? "-> " : "   ";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    for (int i = 0; name[i] && at < 38; i++)
        line[at++] = name[i];
    if (verdict >= 0) {
        while (at < 40)
            line[at++] = ' ';
        const char *v = verdict ? "pass" : "FAIL";
        for (int i = 0; v[i] && at < (int)sizeof(line) - 1; i++)
            line[at++] = v[i];
    }
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/* "   name                saw 0xff204060 err 0x0502" - the colour a failing check left
 * at the centre of the probe region, in the byte order px() returns, and the first GL
 * error it raised. Only failures reach here. `err 0x0000` with the reset colour means
 * the draw ran and put nothing there; an error means it was refused. */
static void saw(const char *name, uint32_t centre, unsigned int err, int drawn,
                uint32_t left, uint32_t right) {
    static const char hex[] = "0123456789abcdef";
    char line[128];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 38; i++)
        line[at++] = name[i];
    while (at < 40)
        line[at++] = ' ';
    const char *head = "saw 0x";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    for (int s = 28; s >= 0; s -= 4)
        line[at++] = hex[(centre >> s) & 0xfu];
    const char *mid = " err 0x";
    for (int i = 0; mid[i]; i++)
        line[at++] = mid[i];
    for (int s = 12; s >= 0; s -= 4)
        line[at++] = hex[(err >> s) & 0xfu];
    /* " drawn NNNNN" - five digits covers the region's 12,288 pixels. */
    const char *tail = " drawn ";
    for (int i = 0; tail[i]; i++)
        line[at++] = tail[i];
    {
        int d = drawn < 0 ? 0 : drawn;
        int div = 10000;
        while (div > 1 && d < div)
            div /= 10;
        while (div >= 1) {
            line[at++] = (char)('0' + (d / div) % 10);
            div /= 10;
        }
    }
    /* The two pixels flanking the centre on its row, which say whether the gap is the
     * quad's diagonal seam or some other shape. */
    const char *lh = " L 0x";
    for (int i = 0; lh[i]; i++)
        line[at++] = lh[i];
    for (int s = 28; s >= 0; s -= 4)
        line[at++] = hex[(left >> s) & 0xfu];
    const char *rh = " R 0x";
    for (int i = 0; rh[i]; i++)
        line[at++] = rh[i];
    for (int s = 28; s >= 0; s -= 4)
        line[at++] = hex[(right >> s) & 0xfu];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/* The verdict line, which a harness watches for as the completion sentinel. Three
 * digits each side, because `GL2_PROBE_MAX_CASES` exceeds ninety-nine. */
static void report_total(int passed, int ran) {
    char line[80];
    int at = 0;
    const char *head = "gl2-probe: ";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    line[at++] = (char)('0' + (passed / 100) % 10);
    line[at++] = (char)('0' + (passed / 10) % 10);
    line[at++] = (char)('0' + passed % 10);
    line[at++] = '/';
    line[at++] = (char)('0' + (ran / 100) % 10);
    line[at++] = (char)('0' + (ran / 10) % 10);
    line[at++] = (char)('0' + ran % 10);
    const char *tail = " passed on hardware";
    for (int i = 0; tail[i] && at < (int)sizeof(line) - 1; i++)
        line[at++] = tail[i];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}
#endif

int gl2_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl2_probe_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    oops_log_info(
        PROBE_TAG,
        "gl2-probe: running the OpenGL 2.0 check suite [build " OOPS_APP_VERSION "]");
    gl2_probe_trace = trace;
    gl2_probe_saw = saw;
#else
    (void)args;
#endif

    gl2_probe_result_t results[GL2_PROBE_MAX_CASES];
    const int ran = gl2_probe_run(results, (int)(sizeof(results) / sizeof(results[0])));
    if (ran < 0) {
#ifndef OOPS_HOST_BUILD
        oops_log_info(PROBE_TAG, "gl2-probe: no GL context; nothing measured");
        oops_system_park_until_closed();
#endif
        return -1;
    }

    int passed = 0;
    for (int i = 0; i < ran; i++) {
        if (results[i].passed)
            passed++;
#ifndef OOPS_HOST_BUILD
        report(results[i].name, results[i].passed);
#endif
    }
#ifndef OOPS_HOST_BUILD
    report_total(passed, ran);
#endif

    /* A big-app payload cannot end itself: returning faults at zero because the entry
     * point has no caller frame, and `_exit` raises SIGSYS. Process lifecycle belongs
     * to SceShellCore, so the payload parks after the verdict line and the host closes
     * it. The host build returns, since a host test that never returns is a hang. */
#ifndef OOPS_HOST_BUILD
    oops_system_park_until_closed();
#else
    return (passed == ran) ? 0 : 1;
#endif
}
