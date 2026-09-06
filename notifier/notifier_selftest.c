/*
 * Host self-test for notifier.
 *
 * The composed status line is the pure part: it must contain the generation, the firmware and
 * the memory, and it must refuse a buffer too small rather than overrun it. Firing the
 * notification is the console-only half and is not exercised here.
 */

#include <stdio.h>
#include <string.h>

#include "oops/system.h"

#include "notifier.h"

static int contains(const char *hay, const char *needle) {
    return strstr(hay, needle) != NULL;
}

int main(void) {
    oops_system_info_t info;
    memset(&info, 0, sizeof(info));
    info.generation = 5;
    strcpy(info.firmware_str, "12.40");
    info.total_ram_mb = 1784;

    int ok = 1;
    char line[64];
    size_t n = notifier_compose_status(line, sizeof(line), &info);
    if (n == 0) {
        fprintf(stderr, "notifier selftest: compose returned nothing\n");
        ok = 0;
    }
    if (!contains(line, "Prospero") || !contains(line, "12.40") || !contains(line, "1784")) {
        fprintf(stderr, "notifier selftest: line is missing a field: '%s'\n", line);
        ok = 0;
    }
    if (strlen(line) != n) {
        fprintf(stderr, "notifier selftest: returned length disagrees with the string\n");
        ok = 0;
    }

    /* A buffer too small is refused (returns 0), never overrun. */
    char tiny[8];
    tiny[7] = '\x7f';
    if (notifier_compose_status(tiny, sizeof(tiny), &info) != 0) {
        fprintf(stderr, "notifier selftest: did not report truncation\n");
        ok = 0;
    }
    if (tiny[7] != '\x7f' && strlen(tiny) >= sizeof(tiny)) {
        fprintf(stderr, "notifier selftest: overran a small buffer\n");
        ok = 0;
    }

    if (ok) {
        printf("notifier selftest: ok (status line composed and bounded)\n");
        return 0;
    }
    return 1;
}
