#ifndef OOPS_APPS_WIPEOUT_PLATFORM_H
#define OOPS_APPS_WIPEOUT_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "oops/display.h"
#include "oops/input.h"
#include "oops/audio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wipeout_vec2i {
    int32_t x;
    int32_t y;
} wipeout_vec2i_t;

typedef struct wipeout_input_state {
    float left_x;       /* -1.0 to 1.0 (steering) */
    float left_y;       /* -1.0 to 1.0 (pitch) */
    float l2_trigger;   /* 0.0 to 1.0 (left airbrake) */
    float r2_trigger;   /* 0.0 to 1.0 (right airbrake) */
    bool thrust;        /* Cross */
    bool fire;          /* Square */
    bool look_back;     /* Circle */
    bool change_view;   /* Triangle */
    bool pause;         /* Options */
    bool exit_combo;    /* L1 + R1 + Options */
} wipeout_input_state_t;

typedef struct wipeout_platform {
    oops_display_t     *disp;
    oops_audio_port_t  *audio;
    uint32_t           *framebuffer;
    uint32_t            width;
    uint32_t            height;
    bool                running;
    wipeout_input_state_t input;
} wipeout_platform_t;

/* Platform lifecycle */
int wipeout_platform_init(wipeout_platform_t *plat, uint32_t width, uint32_t height);
void wipeout_platform_update_input(wipeout_platform_t *plat);
void wipeout_platform_swap_buffers(wipeout_platform_t *plat);
void wipeout_platform_audio_write(wipeout_platform_t *plat, const int16_t *samples, uint32_t count);
void wipeout_platform_exit(wipeout_platform_t *plat);

/* Standard WipEout engine hooks */
wipeout_vec2i_t wipeout_platform_screen_size(const wipeout_platform_t *plat);
double wipeout_platform_now(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_APPS_WIPEOUT_PLATFORM_H */

