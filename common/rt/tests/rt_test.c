/*
 * `common/rt/rt.c` against the host's native operators.
 *
 * Run it with the script beside this file:
 *
 *   ./run.sh
 *
 * # The oracle cannot be the subject, and by default it is
 *
 * On x86-64 the expression `a / b` on an `unsigned __int128` **is a call to
 * `__udivti3`** - the compiler does not have a 128-bit divide instruction to emit. So
 * linking `rt.c` into this test unchanged makes the host's `/` resolve to the very
 * function under test, and every comparison becomes `f(a,b) == f(a,b)`. That version of
 * this file reported 998,996 checks and 0 failures while proving nothing whatsoever.
 *
 * `run.sh` therefore compiles `rt.c` with its three names redefined to `oops_test_*`,
 * leaving
 * `/` and `%` here to bind to the toolchain's own compiler-rt. `oracle_is_distinct()`
 * below checks that this actually happened rather than trusting the build line, because
 * the failure mode of forgetting it is a silent pass.
 *
 * # And the oracle has to be the operator
 *
 * Checking `q * b + r == a` instead would pass for a routine that is consistently wrong
 * in both quotient and remainder - which is exactly what a shift-and-subtract loop
 * produces when its shift count is off by one. Only comparing against a division that
 * was implemented independently can catch that.
 */
#include <stdio.h>

typedef unsigned __int128 oops_tu_int;

/* The subject, under the names `run.sh` gives it. */
oops_tu_int oops_test_udivti3(oops_tu_int a, oops_tu_int b);
oops_tu_int oops_test_umodti3(oops_tu_int a, oops_tu_int b);
oops_tu_int oops_test_udivmodti4(oops_tu_int a, oops_tu_int b, oops_tu_int *rem);

/* The oracle, as the linker resolved it for this translation unit's own `/`. */
oops_tu_int __udivti3(oops_tu_int a, oops_tu_int b);

static int failures = 0;
static unsigned long long checks = 0;

static void print_u128(oops_tu_int v) {
    /* Decimal would need the very division under test. Hex halves say everything a
     * failure needs and cannot be wrong for the same reason the subject is. */
    printf("0x%016llx%016llx", (unsigned long long)(v >> 64), (unsigned long long)v);
}

static void check(oops_tu_int a, oops_tu_int b) {
    oops_tu_int wq, wr, gq, gr, mq, mr;

    if (b == 0) {
        return; /* undefined for the oracle too; rt.c's own answer is documented, not
                   compared */
    }
    wq = a / b;
    wr = a % b;
    gq = oops_test_udivti3(a, b);
    gr = oops_test_umodti3(a, b);
    mr = 0;
    mq = oops_test_udivmodti4(a, b, &mr);
    checks++;

    if (gq == wq && gr == wr && mq == wq && mr == wr) {
        return;
    }
    if (failures < 10) {
        printf("FAIL  a=");
        print_u128(a);
        printf("  b=");
        print_u128(b);
        printf("\n      want q=");
        print_u128(wq);
        printf(" r=");
        print_u128(wr);
        printf("\n      got  q=");
        print_u128(gq);
        printf(" r=");
        print_u128(gr);
        printf("\n");
    }
    failures++;
}

/* xorshift64*, so the sweep is the same every run - a randomised test that only fails
 * sometimes is worse than no test, because the run that passes is the one that gets
 * believed. */
static unsigned long long rng_state = 0x243f6a8885a308d3ull;
static unsigned long long rng(void) {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ull;
}
static oops_tu_int rng128(void) {
    return ((oops_tu_int)rng() << 64) | (oops_tu_int)rng();
}

/*
 * Is `/` in this file something other than the function being tested?
 *
 * The addresses have to differ. If `run.sh` were run without its renames, or a future
 * build rule dropped them, `__udivti3` here would be `rt.c`'s definition and this
 * returns 0 - which is the whole point: the test that cannot fail is the one that
 * reports it.
 */
typedef oops_tu_int (*oops_div_fn)(oops_tu_int, oops_tu_int);
static int oracle_is_distinct(void) {
    /* `volatile`, or clang folds the comparison of two distinct declarations to true at
     * compile time and the check disappears into the binary as a constant. */
    oops_div_fn volatile oracle = __udivti3;
    oops_div_fn volatile subject = oops_test_udivti3;
    return oracle != subject;
}

int main(void) {
    const oops_tu_int one = 1;
    oops_tu_int edge[64];
    int n = 0, i, j;
    unsigned long long k;

    if (!oracle_is_distinct()) {
        printf("rt: ORACLE IS THE SUBJECT - `/` resolved to rt.c's own __udivti3.\n");
        printf("    Build with run.sh, which renames rt.c's symbols to oops_test_*.\n");
        return 1;
    }

    /* The edges: zero and one, the two word boundaries in both directions, all-ones,
     * and the values either side of each power of two - which is where a leading-zero
     * count is wrong if it is wrong at all. */
    edge[n++] = 0;
    edge[n++] = 1;
    edge[n++] = 2;
    edge[n++] = 3;
    edge[n++] = (oops_tu_int)0xffffffffffffffffull; /* 2^64 - 1 */
    edge[n++] = (one << 64);                        /* 2^64     */
    edge[n++] = (one << 64) + 1;
    edge[n++] = (one << 127);
    edge[n++] = (one << 127) - 1;
    edge[n++] = ~(oops_tu_int)0; /* 2^128 - 1 */
    edge[n++] = ((oops_tu_int)0xffffffffull << 64);
    edge[n++] = (oops_tu_int)0x100000000ull;
    for (i = 1; i < 128; i += 17) {
        edge[n++] = (one << i);
        edge[n++] = (one << i) - 1;
        edge[n++] = (one << i) + 1;
    }

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            check(edge[i], edge[j]);
        }
    }

    /* Random pairs, plus each against a small divisor and a single-word divisor: the
     * three shapes that take different branches through the loop's shift count. */
    for (k = 0; k < 200000ull; k++) {
        oops_tu_int a = rng128();
        oops_tu_int b = rng128();
        check(a, b);
        check(a, b >> (rng() & 127u));
        check(a, (oops_tu_int)(unsigned long long)rng());
        check(a, (oops_tu_int)(rng() & 0xffu));
        check(a >> (rng() & 127u), b);
    }

    printf("rt: %llu checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
