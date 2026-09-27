/*
 * What the payload was doing when a fatal signal arrived.
 *
 * The system's own report names the faulting `rip`, the registers and the thread, and
 * that is enough when the fault is in our code. It is not enough when the fault is
 * inside a platform library: `rip` then names libkernel, the registers describe
 * libkernel's frame, and nothing in the report says which of our calls got there.
 * Spaghetti Kart and Ship of Harkinian both ended that way - identical faulting
 * instruction, `rax` of 0, a read of address 0x8, on a thread named `libcxx` - and the
 * question "which call?" had no answer in the log.
 *
 * So this reads the stack. Every eight bytes from the handler's own frame upwards is
 * tested against the payload's text range, and the ones that fall inside it are
 * printed: a return address left by each of our frames still on the stack, innermost
 * first. Put those through the title's `.map` and the call chain has names.
 *
 * It is deliberately dumb. A handler that walks frame pointers needs them to exist, and
 * this target is built without them in places; a handler that allocates, locks or
 * formats richly is a handler that faults inside a fault. This reads memory it was
 * given and calls one logging function that writes by syscall.
 */
#include "oops/system.h"
#include "oops/thread.h"

#include <stdint.h>

/*
 * The payload's text. `app.mk` links at 0x400000 and no title yet reaches 16MB, and the
 * range only has to be tight enough that stack data rarely looks like a code address -
 * a false positive costs one printed line that the `.map` then fails to explain, which
 * is a great deal cheaper than a missing frame.
 */
#ifndef OOPS_CRASHTRACE_TEXT_LO
#define OOPS_CRASHTRACE_TEXT_LO 0x0000000000400000ull
#endif
#ifndef OOPS_CRASHTRACE_TEXT_HI
#define OOPS_CRASHTRACE_TEXT_HI 0x0000000001000000ull
#endif

/*
 * How far up the stack to look, and how many addresses to print before stopping.
 *
 * 4096 words was too far: the walk ran off the top of a thread's stack into the guard
 * page and faulted, and that second fault is what the system then reported - address
 * 0x7eedf8000, one page above the frame it started from - burying the first. The
 * addresses printed before it were still the real ones, which is the only reason the
 * answer survived. A kilobyte of stack is far more than a return chain needs, and the
 * re-entry guard below means running off the end even so costs the trace, not the
 * report.
 */
#define OOPS_CRASHTRACE_WORDS 128
#define OOPS_CRASHTRACE_MAX 48

static void crashtrace_hex(char *out, uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[15 - i] = digits[v & 0xfu];
        v >>= 4;
    }
    out[16] = '\0';
}

static void crashtrace_handler(int signum, void *arg1, void *arg2) {
    (void)arg1;
    (void)arg2;

    char msg[64];
    char hex[17];

    /*
     * Once only. A handler that faults re-enters itself, and then the report describes
     * the handler rather than the program: this walk ran off a stack and the second
     * fault replaced the first in the system's report. Letting the second one through
     * without a trace is the right trade - the first has already been printed.
     */
    static volatile int in_handler = 0;
    if (in_handler) {
        return;
    }
    in_handler = 1;

    crashtrace_hex(hex, (uint64_t)(unsigned int)signum);
    oops_klog("crash", "fatal signal - the payload addresses on the stack follow,");
    oops_klog("crash", "innermost first; resolve them in the title's .map");

    /* The handler's own frame is the floor: everything below it belongs to the signal
     * delivery, everything above is the stack as the faulting code left it. */
    uint64_t *sp = (uint64_t *)__builtin_frame_address(0);
    int printed = 0;

    for (int i = 0; i < OOPS_CRASHTRACE_WORDS && printed < OOPS_CRASHTRACE_MAX; i++) {
        const uint64_t v = sp[i];
        if (v < OOPS_CRASHTRACE_TEXT_LO || v >= OOPS_CRASHTRACE_TEXT_HI) {
            continue;
        }
        crashtrace_hex(hex, v);
        for (int k = 0; k < 64; k++) {
            msg[k] = '\0';
        }
        const char *lead = "  0x";
        int n = 0;
        while (lead[n] && n < 8) {
            msg[n] = lead[n];
            n++;
        }
        for (int k = 0; k < 16 && n < 60; k++) {
            msg[n++] = hex[k];
        }
        msg[n] = '\0';
        oops_klog("crash", msg);
        printed++;
    }

    if (printed == 0) {
        oops_klog("crash", "  no payload addresses on this stack - the fault is on a "
                           "thread our code did not enter");
    }
}

void oops_crashtrace_install(void);

void oops_crashtrace_install(void) {
    /* SIGSEGV is 11, SIGBUS 10, SIGILL 4: the three a bad pointer arrives as. A signal
     * the platform refuses to hand over reports negative and costs nothing. */
    static const int signals[] = {11, 10, 4};
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        const int rc =
            oops_thread_install_exception_handler(signals[i], crashtrace_handler);
        oops_log_info("CRASH", "handler for signal %d: rc=%d", signals[i], rc);
    }
}
