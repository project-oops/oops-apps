/*
 * The event pump: keyboard and mouse input turned into SDL events, plus the system's
 * close and suspend handling. The pad is `SDL_prosperojoystick.c`.
 *
 * `SDL_Scancode` values are USB HID page 0x07 usages (`include/SDL_scancode.h`), which
 * is what `oops_keyboard_read` returns, so translation is a range check. Out-of-range
 * usages are dropped rather than delivered as an unrelated key.
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
#include "oops/system.h"

/*
 * The character a key press types, for `SDL_TEXTINPUT`. `SDL_SendKeyboardText` posts
 * it when text input is enabled and drops unprintable characters
 * (SDL_keyboard.c:1042-1050).
 * The platform reports a HID usage and a modifier mask, so a US layout is applied here;
 * caps lock is not in the mask.
 */
static char prospero_key_char(SDL_Scancode sc, uint8_t mods) {
    const int shift = (mods & OOPS_KMOD_SHIFT) != 0;

    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        return (char)((shift ? 'A' : 'a') + (int)(sc - SDL_SCANCODE_A));
    }
    /* SDL orders these 1..9 then 0, the order of the keyboard row. */
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) {
        static const char plain[] = "1234567890";
        static const char upper[] = "!@#$%^&*()";
        return (shift ? upper : plain)[sc - SDL_SCANCODE_1];
    }
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_0) {
        static const char kp[] = "1234567890";
        return kp[sc - SDL_SCANCODE_KP_1];
    }

    switch (sc) {
    case SDL_SCANCODE_SPACE:
        return ' ';
    case SDL_SCANCODE_MINUS:
        return shift ? '_' : '-';
    case SDL_SCANCODE_EQUALS:
        return shift ? '+' : '=';
    case SDL_SCANCODE_LEFTBRACKET:
        return shift ? '{' : '[';
    case SDL_SCANCODE_RIGHTBRACKET:
        return shift ? '}' : ']';
    case SDL_SCANCODE_BACKSLASH:
        return shift ? '|' : '\\';
    case SDL_SCANCODE_SEMICOLON:
        return shift ? ':' : ';';
    case SDL_SCANCODE_APOSTROPHE:
        return shift ? '"' : '\'';
    case SDL_SCANCODE_GRAVE:
        return shift ? '~' : '`';
    case SDL_SCANCODE_COMMA:
        return shift ? '<' : ',';
    case SDL_SCANCODE_PERIOD:
        return shift ? '>' : '.';
    case SDL_SCANCODE_SLASH:
        return shift ? '?' : '/';
    case SDL_SCANCODE_KP_DIVIDE:
        return '/';
    case SDL_SCANCODE_KP_MULTIPLY:
        return '*';
    case SDL_SCANCODE_KP_MINUS:
        return '-';
    case SDL_SCANCODE_KP_PLUS:
        return '+';
    case SDL_SCANCODE_KP_PERIOD:
        return '.';
    default:
        return '\0';
    }
}

static void PROSPERO_PumpKeyboard(PROSPERO_VideoData *data) {
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
        SDL_SendKeyboardKey(events[i].transition == OOPS_KEY_DOWN ? SDL_PRESSED
                                                                  : SDL_RELEASED,
                            scancode);

        /* The text it types, on the press only. */
        if (events[i].transition == OOPS_KEY_DOWN) {
            const char c = prospero_key_char(scancode, events[i].modifiers);
            if (c != '\0') {
                const char text[2] = {c, '\0'};
                SDL_SendKeyboardText(text);
            }
        }
    }
    (void)data;
}

static void PROSPERO_PumpMouse(PROSPERO_VideoData *data) {
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
             * Relative, as the device reports; SDL derives the absolute position from
             * these deltas.
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
                                (s->buttons & OOPS_MOUSE_LEFT) ? SDL_PRESSED
                                                               : SDL_RELEASED,
                                SDL_BUTTON_LEFT);
        }
        if (changed & OOPS_MOUSE_RIGHT) {
            SDL_SendMouseButton(data->window, 0,
                                (s->buttons & OOPS_MOUSE_RIGHT) ? SDL_PRESSED
                                                                : SDL_RELEASED,
                                SDL_BUTTON_RIGHT);
        }
        if (changed & OOPS_MOUSE_MIDDLE) {
            SDL_SendMouseButton(data->window, 0,
                                (s->buttons & OOPS_MOUSE_MIDDLE) ? SDL_PRESSED
                                                                 : SDL_RELEASED,
                                SDL_BUTTON_MIDDLE);
        }
        last_buttons = s->buttons;
    }
}

void PROSPERO_PumpEvents(_THIS) {
    PROSPERO_VideoData *data = (PROSPERO_VideoData *)_this->driverdata;

    /* The dashboard's Close becomes one SDL_QUIT (oops/system.h), so the title exits
     * cleanly. The handler is installed in VideoInit. */
    static int quit_sent = 0;
    if (!quit_sent && oops_system_close_requested()) {
        quit_sent = 1;
        (void)SDL_SendQuit();
    }

    /* The system event queue is serviced on every pump (oops/system.h): after Close,
     * and for rest mode, the kernel suspends the process and kills a title that does
     * not reach a suspend point in time (`0xa0d0c00f`,
     * `CPU_FAULT_SUSPENDPOINT_TIMEOUT_IN_SUSPEND_ASYNC`). */
    (void)oops_system_pump_events();

    /* Once, after quit: drain the renderer's in-flight work so the process is suspended
     * quiescent rather than mid-submission. */
    static int suspend_prepared = 0;
    if (quit_sent && !suspend_prepared) {
        suspend_prepared = 1;
        oops_system_prepare_for_suspend();
    }

    if (data->keyboard_ready) {
        PROSPERO_PumpKeyboard(data);
    }
    if (data->mouse_ready) {
        PROSPERO_PumpMouse(data);
    }
}

#endif /* SDL_VIDEO_DRIVER_PROSPERO */
