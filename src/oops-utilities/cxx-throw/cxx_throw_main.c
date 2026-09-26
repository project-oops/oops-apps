/*
 * The target entry point. Runs the probe, reports over klog, then parks. No display.
 *
 * This half is C; the C++ is in the archive `cxx.mk` builds and is reached through two
 * `extern "C"` functions.
 */
#include <stdint.h>

#include "oops/system.h"
#include "oops/time.h"

/* Defined in `probe.cpp`; `cxx_throw.h` is a C++ header. */
extern int cxx_throw_run(void);
extern int cxx_throw_total(void);

void cxx_throw_start(void);

void cxx_throw_start(void) {
    oops_log("cxx-throw: start");

    /* Logged before the checks: a report that stops here means a throw did not come
       back. */
    oops_log("cxx-throw: running exception checks");

    const int total = cxx_throw_total();
    const int passed = cxx_throw_run();

    if (passed == total) {
        oops_log(
            "cxx-throw: PASS - every exception check returned through the unwinder");
    } else {
        oops_log("cxx-throw: FAIL - not every check came back");
    }

    /* The counts on their own line, for parsers. */
    oops_log("cxx-throw: passed=%d total=%d", passed, total);

    oops_log("cxx-throw: done");

    for (;;) {
        oops_time_sleep_ms(1000);
    }
}
