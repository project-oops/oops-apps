/*
 * The target entry point. Runs the probe and reports over klog, then parks.
 *
 * This half is C, not C++, for the same reason the rest of oops-apps is: the SDK's
 * entry point, display and logging are C, and the C++ is confined to the archive
 * `cxx.mk` builds. The probe is reached through two `extern "C"` functions and nothing
 * else crosses.
 *
 * **The report is the whole output.** There is no display work here on purpose: a probe
 * that also drives the panel has two ways to fail and one line of output, and the
 * question being asked - does an exception survive a round trip - is answered by a
 * number.
 */
#include <stdint.h>

#include "oops/system.h"
#include "oops/time.h"

/* Defined in `probe.cpp`, built into the C++ archive. Declared here rather than
   including `cxx_throw.h`, which is a C++ header. */
extern int cxx_throw_run(void);
extern int cxx_throw_total(void);

void cxx_throw_start(void);

void cxx_throw_start(void) {
    oops_log("cxx-throw: start");

    /* Announce before attempting, the way obSCEne's probes do: if the platform cannot
       unwind, the failure is a fault inside `cxx_throw_run` and there is no second
       line. A report that stops after this one means the throw did not come back, which
       is itself the result. */
    oops_log("cxx-throw: running exception checks");

    const int total = cxx_throw_total();
    const int passed = cxx_throw_run();

    if (passed == total) {
        oops_log(
            "cxx-throw: PASS - every exception check returned through the unwinder");
    } else {
        oops_log("cxx-throw: FAIL - not every check came back");
    }

    /* The counts go out as their own line so a parser does not have to read the
     * verdict. */
    oops_log("cxx-throw: passed=%d total=%d", passed, total);

    oops_log("cxx-throw: done");

    for (;;) {
        oops_time_sleep_ms(1000);
    }
}
