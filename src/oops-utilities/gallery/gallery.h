#ifndef OOPS_APPS_GALLERY_H
#define OOPS_APPS_GALLERY_H

#include "oops/draw.h"
#include "oops/input.h"
#include "oops/system.h"

/*
 * The subsystem gallery: one page per SDK subsystem, driven by hand.
 *
 * Render is a pure function of a page number and the state to show, so every page can
 * be drawn into a plain buffer and checked on a host - the seam that makes a drawing
 * app testable off the console, exactly as the SDK itself is built.
 */

enum {
    GALLERY_PAGE_SHAPES = 0, /* draw: gradient, primitives, text scales */
    GALLERY_PAGE_INPUT,      /* input: live pad state */
    GALLERY_PAGE_SYSTEM,     /* system: what the machine is */
    GALLERY_PAGE_AUDIO,      /* audio: port status */
    GALLERY_PAGE_NET,        /* netctl: link and address */
    GALLERY_PAGE_CAPS,       /* media-decode + input-device capability matrix */
    GALLERY_PAGE_RUNTIME,    /* runtime: heap, fs, math, dns */
    GALLERY_PAGE_COUNT
};

/*
 * What this console offers the SDK, gathered so the render stays pure. Each field is a
 * plain yes/no the payload fills from the matching oops_*_available() call - the
 * consumer-side readout of the media-decode and input-device subsystems. The paths
 * whose data layouts are still capture-gated report their capability honestly here
 * (available means the library and entry points resolved), not whether a full
 * decode/read yet works.
 */
typedef struct gallery_caps {
    int videodec;          /* oops_videodec_available */
    int audiodec;          /* oops_audiodec_available */
    int audiodec_offload;  /* oops_audiodec_offload_available (AJM) */
    int keyboard;          /* oops_keyboard_available */
    int mouse;             /* oops_mouse_available */
    int adaptive_triggers; /* oops_input_adaptive_triggers_available(port 0) */
    int agc_gpu;           /* oops_display_is_gpu_accelerated */
} gallery_caps_t;

typedef struct gallery_runtime {
    size_t heap_allocated;
    size_t heap_active;
    int fs_ready;
    int math_ready;
    int dns_ready;
} gallery_runtime_t;

/* What the gallery draws for a page, gathered so render stays a pure function of it. */
typedef struct gallery_state {
    int page;                  /* which page, wrapped into [0, GALLERY_PAGE_COUNT) */
    oops_pad_state_t pad;      /* for the input page */
    oops_system_info_t system; /* for the system page */
    int audio_open;            /* for the audio page: is a port up */
    int net_linked;            /* for the net page: is the link up */
    const char *net_ip;        /* for the net page: address, or NULL */
    gallery_caps_t caps;       /* for the capabilities page */
    gallery_runtime_t runtime; /* for the runtime page */
} gallery_state_t;

/* The number of pages. */
int gallery_page_count(void);

/* Wrap a page index into range, so left/right navigation never falls off the ends. */
int gallery_wrap_page(int page);

/* Draw the current page into `surf`. Returns the page actually drawn. */
int gallery_render(oops_surface_t *surf, const gallery_state_t *state);

#endif /* OOPS_APPS_GALLERY_H */
