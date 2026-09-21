/*
 * The event pump: one pass over the input devices, turned into SDL events.
 *
 * # The keyboard is nearly free, and it is worth knowing why
 *
 * `SDL_Scancode`'s values are USB HID usage codes from page 0x07 - upstream says so at the top
 * of `include/SDL_scancode.h` - and `oops_keyboard_read` hands back the HID usage directly. So
 * the translation is a range check rather than a table, and the two agree by construction
 * instead of by a mapping somebody has to keep correct.
 *
 * The range check is not a formality. `SDL_NUM_SCANCODES` is 512 and the values above the HID
 * page are SDL's own inventions, so a usage code out of range is dropped rather than delivered
 * as some unrelated key - the same discipline `src/gl/glut.c` follows for the same reason.
 *
 * # What is not here
 *
 * **The pad.** It belongs to SDL's joystick subsystem, which is a driver of its own under
 * `src/joystick/`, with its own bootstrap and its own device list. Posting pad buttons from the
 * video pump would put them in the event queue without `SDL_NumJoysticks` ever reporting a
 * device, so a title that opens a joystick - which is what every title here does - would find
 * none and take its no-controller path. It is the next piece, not a line in this file.
 */
#include "SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_PROSPERO

#include "events/SDL_events_c.h"
#include "events/SDL_keyboard_c.h"
#include "events/SDL_mouse_c.h"

#include "SDL_prosperovideo.h"
#include "SDL_prosperoevents_c.h"

#include "oops/keyboard.h"
#include "oops/mouse.h"

static void PROSPERO_PumpKeyboard(PROSPERO_VideoData *data)
{
    oops_key_event_t events[OOPS_MAX_KEY_EVENTS];
    int n;
    int i;

    n = oops_keyboard_read(events, OOPS_MAX_KEY_EVENTS);
    if (n <= 0) {
        return;
    }

    for (i = 0; i < n; i++) {
        SDL_Scancode scancode;

        if (events[i].usage == 0 || events[i].usage >= SDL_NUM_SCANCODES) {
            continue;
        }
        scancode = (SDL_Scancode)events[i].usage;
        SDL_SendKeyboardKey(events[i].transition == OOPS_KEY_DOWN ? SDL_PRESSED : SDL_RELEASED,
                            scancode);
    }
    (void)data;
}

static void PROSPERO_PumpMouse(PROSPERO_VideoData *data)
{
    oops_mouse_state_t samples[OOPS_MAX_MOUSE_SAMPLES];
    int n;
    int i;
    static uint8_t last_buttons;

    n = oops_mouse_read(samples, OOPS_MAX_MOUSE_SAMPLES);
    if (n <= 0) {
        return;
    }

    for (i = 0; i < n; i++) {
        const oops_mouse_state_t *s = &samples[i];
        uint8_t changed;

        if (s->dx != 0 || s->dy != 0) {
            /*
             * Relative, which is what the device reports. SDL keeps the absolute position and
             * synthesises it from these, so passing 1 here is what stops the cursor being pinned
             * to a corner - the same trap glut.c documents from the other side, where GLUT wants
             * absolute and has to keep the position itself.
             */
            SDL_SendMouseMotion(data->window, 0, 1, (int)s->dx, (int)s->dy);
        }

        if (s->wheel != 0 || s->tilt != 0) {
            SDL_SendMouseWheel(data->window, 0, (float)s->tilt, (float)s->wheel,
                               SDL_MOUSEWHEEL_NORMAL);
        }

        changed = (uint8_t)(s->buttons ^ last_buttons);
        if (changed & OOPS_MOUSE_LEFT) {
            SDL_SendMouseButton(data->window, 0,
                                (s->buttons & OOPS_MOUSE_LEFT) ? SDL_PRESSED : SDL_RELEASED,
                                SDL_BUTTON_LEFT);
        }
        if (changed & OOPS_MOUSE_RIGHT) {
            SDL_SendMouseButton(data->window, 0,
                                (s->buttons & OOPS_MOUSE_RIGHT) ? SDL_PRESSED : SDL_RELEASED,
                                SDL_BUTTON_RIGHT);
        }
        if (changed & OOPS_MOUSE_MIDDLE) {
            SDL_SendMouseButton(data->window, 0,
                                (s->buttons & OOPS_MOUSE_MIDDLE) ? SDL_PRESSED : SDL_RELEASED,
                                SDL_BUTTON_MIDDLE);
        }
        last_buttons = s->buttons;
    }
}

void PROSPERO_PumpEvents(_THIS)
{
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    if (data->keyboard_ready) {
        PROSPERO_PumpKeyboard(data);
    }
    if (data->mouse_ready) {
        PROSPERO_PumpMouse(data);
    }
}

#endif /* SDL_VIDEO_DRIVER_PROSPERO */
