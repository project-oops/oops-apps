/*
 * The compiler-runtime helpers this target has no library for.
 *
 * clang lowers unsigned 128-bit division to calls into compiler-rt's builtins archive,
 * and the build container has no such archive for x86_64-unknown-freebsd. A payload
 * link does not report an unresolved symbol, so a missing helper faults at run time.
 * StormLib's bundled libtommath divides in unsigned __int128 for its mp_div.
 *
 * __umodti3 shares the loop with __udivti3. The signed and 64-bit forms are absent.
 * tests/rt_test.c checks the division against the host's native operator.
 */

typedef unsigned __int128 oops_tu_int;

/*
 * Leading zeros of a 128-bit value; 128 for zero, which the callers below never pass.
 *
 * __builtin_clzll is an intrinsic that -fno-builtin does not affect, and it lowers to
 * an instruction: the object this file compiles to has no undefined symbols.
 */
static int oops_clz128(oops_tu_int x) {
    unsigned long long hi = (unsigned long long)(x >> 64);
    if (hi != 0ull) {
        return __builtin_clzll(hi);
    }
    return 64 + __builtin_clzll((unsigned long long)x);
}

/*
 * Unsigned 128-bit division with remainder; __udivti3 and __umodti3 wrap it.
 *
 * Restoring shift-and-subtract, one bit per iteration, starting where the divisor's top
 * bit lines up with the dividend's. clang lowers 128-bit comparison, subtraction and
 * shifts inline, so this does not recurse into itself.
 *
 * Division by zero answers zero rather than faulting with no message in a payload.
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
