/*
 * The compiler-runtime helpers this target has no library for.
 *
 * # Why this file exists
 *
 * clang lowers a few operations to calls rather than instructions, and expects the
 * answers to come from compiler-rt's builtins archive. There is no such archive for
 * `x86_64-unknown-freebsd` in the build container - it ships only the `linux/` and
 * `windows/` ones - and a payload link does not report an unresolved symbol, so the
 * call is written, nothing defines it, and the console faults the first time the
 * operation runs.
 *
 * The linux archive would very probably work: these are pure arithmetic routines with
 * no syscall and no libc, and the ELF x86-64 SysV ABI is the same one. "Very probably"
 * is the problem. A hundred and fifty objects built for another platform, linked into a
 * payload to obtain thirty lines of long division, is a dependency nobody would be able
 * to reason about later.
 *
 * # What is here, and what is not
 *
 * Unsigned 128-bit division. StormLib's bundled libtommath does its `mp_div` in
 * `unsigned __int128`, and that is the one call a link census of this tree reports
 * missing.
 *
 * `__umodti3` comes with it although nothing calls it yet, because it is not a second
 * routine - quotient and remainder fall out of the same loop, and exposing only one
 * face would leave `%` on a 128-bit value faulting for want of a `return r`. The
 * *signed* forms are genuinely separate work (sign extraction around this core) and are
 * absent, as are the 64-bit forms, which the hardware divides in one instruction. Add
 * those when a census asks, with a test: a helper that is defined but never called
 * still has to be correct, and an untested wrong one stays wrong quietly.
 *
 * # These are tested, not asserted
 *
 * `tests/rt_test.c` runs this division against the host's native `unsigned __int128`
 * operator over the edge cases and a fixed pseudo-random sweep. That matters more here
 * than anywhere else in the tree: an ordinary function that is subtly wrong produces a
 * visible wrong answer, while a *division helper* that is wrong produces wrong answers
 * inside a bignum library, which surface as a signature that fails to verify - a
 * plausible-looking result with no trace back to here.
 */

typedef unsigned __int128 oops_tu_int;

/*
 * Leading zeros of a 128-bit value; 128 for zero, which the callers below never pass.
 *
 * `__builtin_clzll` is a compiler intrinsic and not affected by `-fno-builtin` - that
 * flag stops clang recognising *library* names, and this is not one. It lowers to an
 * instruction, so there is no call here for a builtins archive to supply, which is
 * checked: the object this file compiles to must have no undefined symbols at all.
 */
static int oops_clz128(oops_tu_int x) {
    unsigned long long hi = (unsigned long long)(x >> 64);
    if (hi != 0ull) {
        return __builtin_clzll(hi);
    }
    return 64 + __builtin_clzll((unsigned long long)x);
}

/*
 * Unsigned 128-bit division with remainder - the workhorse, with `__udivti3` below as
 * the quotient-only face of it.
 *
 * Restoring shift-and-subtract, one bit per iteration, started at the position where
 * the divisor's top bit lines up with the dividend's. Comparison, subtraction and
 * shifting of `__int128` are all lowered inline by clang; division is the only 128-bit
 * operation that becomes a call, which is why this can be written in terms of the type
 * it implements without recursing into itself.
 *
 * Division by zero is undefined behaviour in C and compiler-rt does not check for it
 * either. It is checked here anyway and answers zero, because the alternative on this
 * platform is a fault with no message in a payload that has no debugger attached.
 */
oops_tu_int __udivmodti4(oops_tu_int a, oops_tu_int b, oops_tu_int *rem);
oops_tu_int __udivmodti4(oops_tu_int a, oops_tu_int b, oops_tu_int *rem) {
    oops_tu_int q = 0;
    oops_tu_int d;
    int sh;

    if (b == 0) {
        if (rem != 0) {
            *rem = 0;
        }
        return 0;
    }
    if (a < b) {
        if (rem != 0) {
            *rem = a;
        }
        return 0;
    }

    /* Non-negative because `a >= b`, so `b` has at least as many leading zeros. */
    sh = oops_clz128(b) - oops_clz128(a);
    d = b << sh;
    for (; sh >= 0; --sh) {
        q <<= 1;
        if (a >= d) {
            a -= d;
            q |= 1;
        }
        d >>= 1;
    }
    if (rem != 0) {
        *rem = a;
    }
    return q;
}

oops_tu_int __udivti3(oops_tu_int a, oops_tu_int b);
oops_tu_int __udivti3(oops_tu_int a, oops_tu_int b) {
    return __udivmodti4(a, b, 0);
}

oops_tu_int __umodti3(oops_tu_int a, oops_tu_int b);
oops_tu_int __umodti3(oops_tu_int a, oops_tu_int b) {
    oops_tu_int r = 0;
    (void)__udivmodti4(a, b, &r);
    return r;
}
