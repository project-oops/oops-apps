/*
 * cube_frame.h - the stop file and the frame measurement gl1-cube and gl2-cube share.
 *
 * Both demos end a network-driven run when a per-run stop file appears, and both log
 * the same three numbers about the rendered frame so their logs read side by side.
 */
#ifndef OOPS_APPS_CUBE_FRAME_H
#define OOPS_APPS_CUBE_FRAME_H

#include "oops/freestd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CUBE_FRAME_W 1920u
#define CUBE_FRAME_H 1080u
/* The clear colour both demos use, as the render target stores it. */
#define CUBE_FRAME_CLEAR 0xff0d121fu

/* "/app0/stop.<pid>": the title directory as the sandbox mounts it. The pid makes a
 * file left behind by an earlier run inert, so nothing has to be read or unlinked. */
static inline void cube_stop_path(char *out, size_t size, int pid) {
    oops_snprintf(out, size, "/app0/stop.%d", pid);
}

typedef struct cube_frame_stats {
    uint32_t hash;       /* FNV-1a, 32-bit, over the words scanned */
    uint32_t mod_pixels; /* scanned words that differ from the clear colour */
    uint32_t center;     /* the centre pixel */
} cube_frame_stats_t;

/* Scans every step-th word of a CUBE_FRAME_W x CUBE_FRAME_H frame. With flush set,
 * each cache line is dropped as it is reached, for a source with stale CPU lines. */
static inline cube_frame_stats_t cube_frame_measure(const uint32_t *src, size_t step,
                                                    bool flush) {
    cube_frame_stats_t st = {0x811c9dc5u, 0u, 0u};
    if (!src)
        return st;
    const size_t centre =
        (size_t)(CUBE_FRAME_H / 2u) * CUBE_FRAME_W + CUBE_FRAME_W / 2u;
#ifndef OOPS_HOST_BUILD
    __builtin_ia32_clflush((const void *)&src[centre]);
#endif
    st.center = src[centre];
    for (size_t p = 0; p < (size_t)CUBE_FRAME_W * CUBE_FRAME_H; p += step) {
#ifndef OOPS_HOST_BUILD
        if (flush && (p & 15u) == 0u)
            __builtin_ia32_clflush((const void *)&src[p]);
#else
        (void)flush;
#endif
        const uint32_t v = src[p];
        if (v != CUBE_FRAME_CLEAR)
            st.mod_pixels++;
        st.hash ^= v;
        st.hash *= 0x01000193u;
    }
    return st;
}

#endif
