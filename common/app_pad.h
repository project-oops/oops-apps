/*
 * app_pad.h - controller button helpers the utility apps share.
 */
#ifndef OOPS_APPS_APP_PAD_H
#define OOPS_APPS_APP_PAD_H

#include "oops/input.h"

#include <stdint.h>

/* Down now and up in the previous sample: an edge, so one press is one action. */
static inline int app_pressed(uint32_t now, uint32_t was, uint32_t mask) {
    return ((now & mask) != 0u) && ((was & mask) == 0u);
}

/* The exit gesture: L1, R1 and Options together, a combo no single press triggers, so
 * every other button stays free for the app. */
static inline int app_exit_combo(uint32_t buttons) {
    return ((buttons & OOPS_BUTTON_L1) != 0u) && ((buttons & OOPS_BUTTON_R1) != 0u) &&
           ((buttons & OOPS_BUTTON_OPTIONS) != 0u);
}

#endif
