/*
 * tls-probe: does a title carrying a PT_TLS program header load at all?
 *
 * # Why this exists
 *
 * `mesa-probe` is refused by the loader before any of its code runs:
 *
 *     ERROR: readHeader(865) invalid state
 *     ERROR: sceSblAuthMgrAuthHeader(324) sceSblAuthMgrAuthHeader:readHeader -37
 *     [KERNEL] kern_get_self_auth_info: error 46
 *     sceSblACMgrGetFsSandboxType(/system_ex/app/PPSA90010/eboot.bin) failed. 0x80020008
 *
 * In the same session, on the same console, `gl1-cube` launched and rendered, seashell was in
 * use and obSCEne ran a sweep - so the loader, the entitlement patch and the deployment path
 * are all working. Every module that loads has **five** program headers and no TLS. mesa-probe
 * has **six**, and the sixth is `PT_TLS`, which Mesa forces because its debug layer and its
 * shader backend both use `__thread`.
 *
 * That is a strong inference and not yet a fact. Two things could produce it:
 *
 *   1. the loader refuses a `PT_TLS` segment in a title's own image, or
 *   2. the loader refuses a sixth program header of any kind, or something else particular to a
 *      20 MB image with 349 KB of dynamic-link data.
 *
 * Which one decides the fix, and they are very different pieces of work, so guessing is
 * expensive. mesa-probe cannot separate them: dropping the segment while keeping the sections
 * does not link, because lld refuses an `STT_TLS` symbol with no `PT_TLS` segment.
 *
 * # What this is
 *
 * The smallest title that carries a `PT_TLS` segment and nothing else unusual. One `__thread`
 * variable, no Mesa, no C++, no display, a few kilobytes. It differs from a title known to load
 * in exactly one property: the segment.
 *
 * So the result is unambiguous either way:
 *
 *   **refused**  - `PT_TLS` is the blocker, and oops-mesa needs thread-local storage without a
 *                  segment. Its runtime shim already owns the thread surface, so `__tls_get_addr`
 *                  over a per-thread table is the shape of the answer.
 *   **loads**    - `PT_TLS` is fine, mesa-probe is refused for some other reason, and the search
 *                  moves to what else is unusual about a 20 MB image. That would be just as
 *                  useful, and it would stop `REQ-20260915T0001Z-8d72` being resolved against a
 *                  segment that was never the problem.
 *
 * # What it checks once it is running
 *
 * Loading is the headline, but a `PT_TLS` segment that loads and is then never initialised would
 * be its own trap - Mesa would read zeroes from thread-local state and misbehave in ways that
 * look like driver bugs. So the probe also writes its `__thread` variable, reads it back, and
 * says whether the value survived. That answers the second half of what `-8d72` asked: whether
 * the loader *honours* the segment, not merely tolerates it.
 */

#include "oops/system.h"

#include <stdint.h>

/*
 * The whole point of the module. `.tbss` only - no initialiser - because that is the shape Mesa
 * produces: mesa-probe's segment is `FileSiz 0x0, MemSiz 0x1108`, all zero-initialised.
 */
static __thread uint32_t tls_word;

/* A second one, so a single variable landing at offset zero cannot pass by accident. */
static __thread uint32_t tls_other;

void tls_probe_start(void);

void tls_probe_start(void)
{
    oops_klog("TLS-PROBE", "loaded: a title with a PT_TLS segment reached its entry point");

    /* If the segment loaded but was never initialised, this reads zero and the write below
     * lands somewhere undefined. Both are reported rather than assumed. */
    oops_kprintf("TLS-PROBE", "before: tls_word=0x%08x tls_other=0x%08x",
                 (unsigned)tls_word, (unsigned)tls_other);

    tls_word = 0xC0FFEE01u;
    tls_other = 0xC0FFEE02u;

    oops_kprintf("TLS-PROBE", "after:  tls_word=0x%08x tls_other=0x%08x",
                 (unsigned)tls_word, (unsigned)tls_other);

    if (tls_word == 0xC0FFEE01u && tls_other == 0xC0FFEE02u) {
        oops_klog("TLS-PROBE", "result: thread-local storage reads back what was written");
        oops_klog("TLS-PROBE", "verdict: the loader honours PT_TLS in a title's own image");
    } else {
        /* Loading and then not working is the outcome REQ-...-8d72 specifically warned about:
         * "a segment that links and then reads nothing". */
        oops_klog("TLS-PROBE", "result: the values did not survive");
        oops_klog("TLS-PROBE", "verdict: the segment loads but is NOT initialised - a title may");
        oops_klog("TLS-PROBE", "         not rely on __thread even though it links and boots");
    }

    oops_klog("TLS-PROBE", "done");

    /*
     * Idle rather than return. Returning from a title's entry point faults at `rip: 0x0`, because
     * the dynamic linker transfers control with no caller frame, and no userland call ends a
     * `big-app` process either - lifecycle belongs to `SceShellCore` (obSCEne
     * `REQ-20260917T1450Z-2e71`; the measurement is written up where the helper is declared).
     *
     * This probe's whole output is the two verdict lines above, so an ending that crashes after
     * printing them is a crash report attached to a good result. The host closes the title once
     * it has read them.
     */
    oops_klog("TLS-PROBE", "idle and finished - close this title from the host");
    oops_system_park_until_closed();
}
