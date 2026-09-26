/*
 * common/rt/rt.c against the host's native operators. Run it with ./run.sh beside it.
 *
 * On x86-64, / on an unsigned __int128 is a call to __udivti3, so run.sh compiles rt.c
 * under oops_test_* names and / and % here bind to the toolchain's compiler-rt.
 * oracle_is_distinct() confirms that at run time.
 *
 * The oracle is the operator, not q * b + r == a, which passes for a loop that is
 * consistently wrong in both quotient and remainder.
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
    /* Hex halves, because decimal would need the division under test. */
    printf("0x%016llx%016llx", (unsigned long long)(v >> 64), (unsigned long long)v);
}

/* All three helpers agree with the native / and % for one pair. */
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

/* xorshift64* with a fixed seed, so the sweep is the same every run. */
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
 * The native / is not the function under test. Without run.sh's renames __udivti3
 * here is rt.c's own definition and this returns 0.
 */
typedef oops_tu_int (*oops_div_fn)(oops_tu_int, oops_tu_int);
static int oracle_is_distinct(void) {
    /* volatile, or clang folds the comparison of two distinct declarations to true. */
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

    /* Edge pairs: zero and one, the word boundaries, all-ones, and the values either
     * side of powers of two, where a wrong leading-zero count shows. */
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

    /* Random pairs, plus small and single-word divisors: the shapes that take
     * different branches through the loop's shift count. */
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
