/*
 * The event pump: one pass over the input devices, turned into SDL events.
 *
 * # The keyboard is nearly free, and it is worth knowing why
 *
 * `SDL_Scancode`'s values are USB HID usage codes from page 0x07 - upstream says so at
 * the top of `include/SDL_scancode.h` - and `oops_keyboard_read` hands back the HID
 * usage directly. So the translation is a range check rather than a table, and the two
 * agree by construction instead of by a mapping somebody has to keep correct.
 *
 * The range check is not a formality. `SDL_NUM_SCANCODES` is 512 and the values above
 * the HID page are SDL's own inventions, so a usage code out of range is dropped rather
 * than delivered as some unrelated key - the same discipline `src/gl/glut.c` follows
 * for the same reason.
 *
 * # What is not here
 *
 * **The pad.** It belongs to SDL's joystick subsystem, which is a driver of its own
 * under `src/joystick/`, with its own bootstrap and its own device list. Posting pad
 * buttons from the video pump would put them in the event queue without
 * `SDL_NumJoysticks` ever reporting a device, so a title that opens a joystick - which
 * is what every title here does - would find none and take its no-controller path. It
 * is the next piece, not a line in this file.
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
 * **The character a key press stands for, so that typing works and not just key
 * handling.**
 *
 * This backend sent `SDL_SendKeyboardKey` and nothing else, which gives a program every
 * `SDL_KEYDOWN` it could want and no `SDL_TEXTINPUT` at all. A program that reads keys
 * directly never notices; a program that expects the text of what was typed gets an
 * empty string for ever. Neverball is the second kind - `SDL_StartTextInput` in
 * share/text.c:106, `SDL_TEXTINPUT` in ball/main.c:246 - so its name entry could not be
 * completed, `CONFIG_PLAYER` stayed empty, and `st_title` sent every press of Play to
 * the name screen and every press of Enter there back to the menu. From the sofa that
 * is a menu that will not start a game.
 *
 * `SDL_SendKeyboardText` needs nothing from this backend beyond being called: it posts
 * `SDL_TEXTINPUT` whenever the event is enabled, which `SDL_StartTextInput` does on its
 * own without a `StartTextInput` hook, and it drops unprintable characters itself
 * (SDL_keyboard.c:1042-1050).
 *
 * **US layout, and that is a limitation rather than a decision.** The platform hands
 * over an HID usage and a modifier mask, not a character, so the layout has to be
 * applied here and there is only one written down. A keyboard set to another layout
 * types the wrong punctuation. Caps lock is not represented in the modifier mask at
 * all, so it does nothing; shift does the work. Both are worth fixing when something
 * needs them, and neither stops a name being entered.
 */
static char prospero_key_char(SDL_Scancode sc, uint8_t mods) {
    const int shift = (mods & OOPS_KMOD_SHIFT) != 0;

    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        return (char)((shift ? 'A' : 'a') + (int)(sc - SDL_SCANCODE_A));
    }
    /* SDL orders these 1..9 then 0, which is the order the row is in and not the
     * digits'. */
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

        /* And the text it stands for, on the press only - a release types nothing. */
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
             * Relative, which is what the device reports. SDL keeps the absolute
             * position and synthesises it from these, so passing 1 here is what stops
             * the cursor being pinned to a corner - the same trap glut.c documents from
             * the other side, where GLUT wants absolute and has to keep the position
             * itself.
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

    /* Turn the dashboard's Close into an SDL_QUIT (oops/system.h), so an SDL program
     * that watches for SDL_QUIT - the ordinary way an SDL app is asked to exit - closes
     * cleanly instead of being killed mid-frame. Sent once; the handler was installed
     * in VideoInit. */
    static int quit_sent = 0;
    if (!quit_sent && oops_system_close_requested()) {
        quit_sent = 1;
        (void)SDL_SendQuit();
    }

    /* **Service the system's own event queue every frame** (oops/system.h).
     *
     * The Close signal above is only half of what the system does. After it - and for
     * rest mode without any signal at all - the kernel suspends the process
     * asynchronously and allows 100 seconds to reach a suspend point; a title that
     * never does is killed with `0xa0d0c00f`,
     * `CPU_FAULT_SUSPENDPOINT_TIMEOUT_IN_SUSPEND_ASYNC`. A queue that is serviced only
     * at the end is a queue that was ignored until then, so this runs on every pump
     * rather than on Close. */
    (void)oops_system_pump_events();

    /* On the way out, once: empty the queue and drain whatever the renderer has in
     * flight, so the kernel is freezing a quiescent process rather than one
     * mid-submission. Done here and not in the title, because every SDL title on this
     * backend wants it and none of them should have to know the sequence. */
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
