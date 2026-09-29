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
/*
 * 16MB was too low and silently threw away every real frame. Ship of Harkinian's text
 * runs to about 0x1eed52f - `HintTable_Init` alone is 943KB - so a call to 0x18ff170
 * sat outside the range while stack words that happened to fall inside it did not. What
 * came back looked like a call chain and was not: one of the addresses disassembled to
 * the middle of a five-byte `callq`, which no return address can be.
 */
#ifndef OOPS_CRASHTRACE_TEXT_HI
#define OOPS_CRASHTRACE_TEXT_HI 0x0000000004000000ull
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
#define OOPS_CRASHTRACE_WORDS 512
#define OOPS_CRASHTRACE_MAX 48

/*
 * Where `rip` and `rsp` sit in the context `arg1` points at, as 64-bit words.
 *
 * Measured, not assumed: the handler printed forty words and five matched the system's
 * own register dump for the same fault exactly - rip 0x80003bac9 at word 28, rsp
 * 0x7eedf7d90 at 31, rbp 0x7eedf7e30 at 17, eflags 0x10256 at 30, and rdi/rsi at 9 and
 * 10. Five agreeing is not a coincidence of layout.
 *
 * Counted off a kernel log the first time, which interleaves lines from other writers,
 * and every index came out one low: `rip` then read as 4 and `rsp` as 0x10256, the
 * eflags beside it. Printing the two by name is what caught it, and is why they are
 * still printed - an index that drifts gives a plausible address rather than an obvious
 * error, and this file has already reported one of those as an answer.
 *
 * This is what makes a backtrace possible at all. The handler runs on a small stack of
 * its own, so walking from its own frame reaches none of the faulting thread's frames -
 * proven three ways: the walk hit unmapped memory 8KB up, widening the accepted text
 * range from 16MB to 64MB produced no new addresses, and the one value that did survive
 * disassembled to the middle of a five-byte `callq`, which no return address can be.
 * `rsp` from this context is the faulting thread's stack, and that one does.
 */
#define OOPS_CRASHTRACE_CTX_RBP 17u
#define OOPS_CRASHTRACE_CTX_RIP 28u
#define OOPS_CRASHTRACE_CTX_RSP 31u

/*
 * The trap number and the address that faulted, derived rather than guessed.
 *
 * The three indices above are spaced exactly as FreeBSD's `mcontext` spaces those
 * registers: `mc_rbp` to `mc_rip` is eleven words there and 17 to 28 here, `mc_rip` to
 * `mc_rsp` is three and 28 to 31 here. Three measured values agreeing with one layout at
 * a uniform offset of eight words is a stronger statement than any one of them, and it
 * puts `mc_trapno` at 24 and `mc_addr` at 25.
 *
 * They are printed together because each checks the other, and because this file warns
 * above that a drifted index gives a plausible address rather than an obvious error. A
 * page fault is trap 12 on this architecture: a `trap` that reads 12 alongside an `addr`
 * that explains the signal is two independent confirmations, and a `trap` that reads
 * anything else says to believe neither.
 */
#define OOPS_CRASHTRACE_CTX_TRAPNO 24u
#define OOPS_CRASHTRACE_CTX_ADDR 25u

static void crashtrace_hex(char *out, uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        out[15 - i] = digits[v & 0xfu];
        v >>= 4;
    }
    out[16] = '\0';
}

static void crashtrace_handler(int signum, void *arg1, void *arg2) {
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
        /* Faulted inside the trace. Hand the signal back before returning, or the
         * instruction that faulted runs again into a handler that keeps refusing it and
         * the title spins there instead of dying. */
        (void)oops_thread_remove_exception_handler(signum);
        return;
    }
    in_handler = 1;

    oops_klog("crash", "fatal signal - the payload addresses on the stack follow,");
    oops_klog("crash", "innermost first; resolve them in the title's .map");

    /*
     * Which signal, and the second argument.
     *
     * The number was being formatted here and then thrown away - the next `crashtrace_hex`
     * overwrote the buffer with `rip` before anything printed it. Every report this handler
     * has produced was therefore silent about the one thing that separates a bad
     * dereference (11) from a bad instruction (4), which is the difference between "a
     * pointer was null" and "execution reached a guard that should be unreachable". A day
     * went into guessing between those two readings from `rip` alone.
     *
     * `arg2` is printed as the value it is, not read through. `oops/thread.h` records the
     * handler as receiving (signum, arg1, arg2) and only arg1 is known to be the context,
     * so this says what arrived and leaves interpreting it to whoever reads the log with
     * the platform's headers to hand. A pointer that looks like an address is a lead; one
     * that looks like a small integer is a code.
     */
    crashtrace_hex(hex, (uint64_t)(unsigned int)signum);
    oops_klog("crash", "  signal:");
    oops_klog("crash", hex);
    crashtrace_hex(hex, (uint64_t)(uintptr_t)arg2);
    oops_klog("crash", "  arg2:");
    oops_klog("crash", hex);

    /*
     * What the platform handed us, and the words behind it.
     *
     * Scanning this handler's own stack does not work: it runs on a small stack of its
     * own, a walk of 8KB reaches unmapped memory, and the one value that survived the
     * text-range filter disassembled to the middle of a five-byte `callq` - a spill,
     * not a return address. The faulting thread's frames are not on this stack to be
     * found.
     *
     * `oops/thread.h` records the handler as receiving (signum, arg1, arg2), measured
     * on hardware. If either carries the interrupted context then `rip` and the real
     * `rsp` are in it, and those are worth more than any amount of guessing at this
     * one. So print them: a word in the text range is a candidate `rip`, and one near
     * the other stack addresses is a candidate `rsp`.
     *
     * Reading through a pointer the platform may not have set is itself a risk, which
     * is what the re-entry guard above is for: a fault here removes the handler and
     * lets the original signal stand rather than looping.
     */
    if (arg1 != (void *)0) {
        const uint64_t *ctx = (const uint64_t *)arg1;
        crashtrace_hex(hex, ctx[OOPS_CRASHTRACE_CTX_TRAPNO]);
        oops_klog("crash", "  trap (12 = page fault):");
        oops_klog("crash", hex);
        crashtrace_hex(hex, ctx[OOPS_CRASHTRACE_CTX_ADDR]);
        oops_klog("crash", "  faulting address:");
        oops_klog("crash", hex);
        crashtrace_hex(hex, ctx[OOPS_CRASHTRACE_CTX_RIP]);
        oops_klog("crash", "  rip:");
        oops_klog("crash", hex);
        crashtrace_hex(hex, ctx[OOPS_CRASHTRACE_CTX_RSP]);
        oops_klog("crash", "  rsp:");
        oops_klog("crash", hex);
    }

    /*
     * The faulting thread's own stack, from its `rsp`, which is the only place its
     * return chain exists. Without a context this walked the handler's frame instead
     * and found nothing but spills - see the note on the context offsets above.
     */
    uint64_t *sp =
        (arg1 != (void *)0)
            ? (uint64_t *)(uintptr_t)((const uint64_t *)arg1)[OOPS_CRASHTRACE_CTX_RSP]
            : (uint64_t *)__builtin_frame_address(0);
    /*
     * Stop at the page boundary above `rsp`, because that is where this stack ends.
     *
     * Measured: `rsp` was 0x7eedf7d90 and the guard page 0x7eedf8000 - 624 bytes above
     * it. A walk of 512 words is 4KB and runs straight off into the guard, and that
     * second fault then replaces the real one in the system's report. It did exactly
     * that twice, once reporting a `rip` of 0x223cf5a that resolves to `.rela.dyn`
     * rather than to any code.
     *
     * A deeper stack loses the frames above this boundary, which is the right trade: a
     * truncated trace is a smaller lie than a fabricated fault address.
     */
    const uintptr_t sp_end = ((uintptr_t)sp + 0xfffu) & ~(uintptr_t)0xfffu;
    const int sp_words = (int)(((uintptr_t)sp_end - (uintptr_t)sp) / sizeof(uint64_t));
    int printed = 0;

    const int limit =
        sp_words < OOPS_CRASHTRACE_WORDS ? sp_words : OOPS_CRASHTRACE_WORDS;
    for (int i = 0; i < limit && printed < OOPS_CRASHTRACE_MAX; i++) {
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

    /*
     * Hand the signal back to the platform.
     *
     * A handler that logs and returns does not report a fault, it absorbs one: the
     * thread carries on, the process stops dying, and a title that was crashing looks
     * fixed. That happened here - Spaghetti Kart went from a coredump to parking, with
     * and without an unrelated patch, and the only thing that had changed was this
     * handler existing. An instrument that alters the outcome it measures is worthless.
     *
     * Removing the handler restores the default, and the faulting instruction runs
     * again on return: the same fault, now fatal, with the log above it. The trace is
     * free; the behaviour is the program's own.
     */
    (void)oops_thread_remove_exception_handler(signum);
}

void oops_crashtrace_install(void);

void oops_crashtrace_install(void) {
    /* SIGSEGV is 11, SIGBUS 10, SIGILL 4: the three a bad pointer arrives as. A signal
     * the platform refuses to hand over reports negative and costs nothing. */
    /*
     * 11 SEGV, 10 BUS, 4 ILL - and 6 ABRT, which was missing and mattered.
     *
     * `abort` is how a C++ program ends when an exception escapes a thread, when
     * `std::terminate` runs, and when an assert fires. None of those raise any of the
     * other three, so a worker dying that way produced a title that simply stopped: no
     * trace, no message, and a log whose last line was whatever it had been doing. Ship of
     * Harkinian writing its archive stopped exactly like that, and the absence of a trace
     * was read as "still running, just slow" for longer than it should have been.
     */
    static const int signals[] = {11, 10, 4, 6};
    for (unsigned i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
        const int rc =
            oops_thread_install_exception_handler(signals[i], crashtrace_handler);
        oops_log_info("CRASH", "handler for signal %d: rc=%d", signals[i], rc);
    }
}
