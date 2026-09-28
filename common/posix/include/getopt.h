/*
 * GNU's long options, over `<unistd.h>`'s `getopt` and its globals.
 *
 * `--name`, `--name=value` and `--name value`, with unambiguous prefixes accepted as GNU's are.
 * As with the short form, parsing stops at the first operand or at `--`; GNU's permutation of
 * operands to the end is not done. A payload's argv is the one its entry point builds, so this
 * matters only to a port that parses arguments at all (NetSurf's framebuffer front end).
 */
#ifndef OOPS_POSIX_GETOPT_H
#define OOPS_POSIX_GETOPT_H

#include <unistd.h>

#define no_argument 0
#define required_argument 1
#define optional_argument 2

struct option {
    const char *name;
    int has_arg;
    int *flag;
    int val;
};

#ifdef __cplusplus
extern "C" {
#endif
int getopt_long(int argc, char *const argv[], const char *optstring,
                const struct option *longopts, int *longindex);
#ifdef __cplusplus
}
#endif

#endif
