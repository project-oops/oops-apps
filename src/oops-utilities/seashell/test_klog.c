/*
 * Minimal klog test eboot.
 *
 * This tests whether SYS_klog works in a native eboot context.
 * If this produces klog output, the syscall trampoline works.
 * If not, klog is broken in native eboots.
 */
#include "oops/syscall.h"
#include "oops/freestd.h"

static void simple_klog(const char *msg) {
    char buf[256];
    size_t pos = 0;
    const char *tag = "TEST";
    buf[pos++] = '[';
    for (int i = 0; tag[i] && pos < 200; i++)
        buf[pos++] = tag[i];
    buf[pos++] = ']';
    buf[pos++] = ' ';
    for (int i = 0; msg[i] && pos < 250; i++)
        buf[pos++] = msg[i];
    buf[pos++] = '\n';
    buf[pos] = '\0';
    sys_call(601, 7, (long)buf, 0, 0, 0, 0);
}

int test_start(void);

int test_start(void) {
    simple_klog("minimal test eboot started");
    /* Infinite loop - don't return */
    for (;;) {
        /* Spin */
    }
    return 0;
}
