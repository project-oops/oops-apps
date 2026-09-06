#include "notifier.h"

/* A tiny appender that never overruns `max` and reports whether it had to stop. */
typedef struct sink {
    char *out;
    size_t max;
    size_t len;
    int truncated;
} sink_t;

static void put_str(sink_t *s, const char *str) {
    while (*str) {
        if (s->len + 1 >= s->max) {
            s->truncated = 1;
            return;
        }
        s->out[s->len++] = *str++;
    }
}

static void put_u32(sink_t *s, uint32_t value) {
    char rev[11];
    int n = 0;
    do {
        rev[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && n < 10);
    while (n > 0) {
        if (s->len + 1 >= s->max) {
            s->truncated = 1;
            return;
        }
        s->out[s->len++] = rev[--n];
    }
}

size_t notifier_compose_status(char *out, size_t max, const oops_system_info_t *info) {
    if (!out || max == 0 || !info) {
        return 0;
    }
    sink_t s = { out, max, 0, 0 };

    put_str(&s, "OOPS: ");
    put_str(&s, info->generation == 5   ? "Prospero"
                : info->generation == 4 ? "Orbis"
                                        : "unknown");
    put_str(&s, " fw ");
    put_str(&s, info->firmware_str[0] ? info->firmware_str : "?");
    put_str(&s, " mem ");
    put_u32(&s, (uint32_t)info->total_ram_mb);
    put_str(&s, "MB");

    s.out[s.len] = '\0';
    return s.truncated ? 0 : s.len;
}
