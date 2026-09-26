/*
 * sandbox-daemon host selftest.
 *
 * Verifies that the freestanding string helpers compile and
 * run correctly on the host build machine.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pull in the freestanding implementation */
#include "oops/freestd.h"

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(cond, msg)                                                                \
    do {                                                                               \
        tests_run++;                                                                   \
        if (!(cond)) {                                                                 \
            tests_failed++;                                                            \
            fprintf(stderr, "FAIL: %s\n", msg);                                        \
        } else {                                                                       \
            printf("PASS: %s\n", msg);                                                 \
        }                                                                              \
    } while (0)

static void test_obs_strlen(void) {
    TEST(obs_strlen("") == 0, "obs_strlen of empty string");
    TEST(obs_strlen("hello") == 5, "obs_strlen of 'hello'");
    TEST(obs_strlen("0") == 1, "obs_strlen of '0'");
}

static void test_obs_strcmp(void) {
    TEST(obs_strcmp("a", "a") == 0, "obs_strcmp equal strings");
    TEST(obs_strcmp("a", "b") < 0, "obs_strcmp less than");
    TEST(obs_strcmp("b", "a") > 0, "obs_strcmp greater than");
}

static void test_obs_strncmp(void) {
    TEST(obs_strncmp("hello", "hello world", 5) == 0, "obs_strncmp prefix match");
    TEST(obs_strncmp("hello", "hallo", 3) != 0, "obs_strncmp prefix mismatch");
}

static void test_obs_strncpy(void) {
    char dst[8];
    obs_strncpy(dst, "hello", sizeof(dst));
    TEST(memcmp(dst, "hello", 5) == 0, "obs_strncpy copy short string");
    TEST(dst[5] == '\0', "obs_strncpy null termination");
}

static void test_obs_format_i64(void) {
    char buf[32];
    size_t n = obs_format_i64(buf, 0);
    buf[n] = '\0';
    TEST(n == 1 && obs_strcmp(buf, "0") == 0, "obs_format_i64 zero");
    n = obs_format_i64(buf, 42);
    buf[n] = '\0';
    TEST(n == 2 && obs_strcmp(buf, "42") == 0, "obs_format_i64 42");
    n = obs_format_i64(buf, -1);
    buf[n] = '\0';
    TEST(n == 2 && obs_strcmp(buf, "-1") == 0, "obs_format_i64 -1");
}

static void test_obs_format_hex(void) {
    char buf[32];
    size_t n = obs_format_hex(buf, 0);
    buf[n] = '\0';
    TEST(n == 3 && obs_strcmp(buf, "0x0") == 0, "obs_format_hex zero");
    n = obs_format_hex(buf, 0xFF);
    buf[n] = '\0';
    TEST(n == 4 && obs_strcmp(buf, "0xff") == 0, "obs_format_hex 0xFF");
}

static void test_oops_snprintf(void) {
    char buf[64];
    int n = oops_snprintf(buf, sizeof(buf), "hello %s", "world");
    TEST(n == 11 && obs_strcmp(buf, "hello world") == 0, "oops_snprintf basic format");
    n = oops_snprintf(buf, 6, "hello world");
    TEST(n == 11 && obs_strncmp(buf, "hello", 5) == 0 && buf[5] == '\0',
         "oops_snprintf truncation");
}

int main(void) {
    printf("sandbox-daemon host selftest\n");
    printf("============================\n");

    test_obs_strlen();
    test_obs_strcmp();
    test_obs_strncmp();
    test_obs_strncpy();
    test_obs_format_i64();
    test_obs_format_hex();
    test_oops_snprintf();

    printf("============================\n");
    printf("Results: %d tests, %d failed\n", tests_run, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
