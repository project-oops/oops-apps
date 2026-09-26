/*
 * The pad as an `SDL_JoystickDriver`, so `SDL_NumJoysticks` reports it.
 *
 * One device on port 0, counted only while the pad reports `connected`.
 * `GetGamepadMapping` declares the layout, so `SDL_GameController` needs no mapping
 * database. Buttons are numbered in `SDL_GameControllerButton` order, so raw joystick
 * readers see the conventional numbering.
 */
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_PROSPERO

#include "SDL_events.h" /* SDL_PRESSED / SDL_RELEASED */
#include "SDL_joystick.h"
#include "joystick/SDL_sysjoystick.h"
#include "joystick/SDL_joystick_c.h"

#include "oops/input.h"
#include "oops/system.h" /* oops_log_info - see PROSPERO_JoystickGetCount */

#define PROSPERO_PAD_PORT 0

/* Button indices, in SDL_GameControllerButton order. */
enum {
    PROSPERO_BTN_A = 0,      /* cross */
    PROSPERO_BTN_B,          /* circle */
    PROSPERO_BTN_X,          /* square */
    PROSPERO_BTN_Y,          /* triangle */
    PROSPERO_BTN_BACK,       /* create */
    PROSPERO_BTN_GUIDE,      /* the home button, when pad privilege allows it */
    PROSPERO_BTN_START,      /* options */
    PROSPERO_BTN_LEFTSTICK,  /* L3 */
    PROSPERO_BTN_RIGHTSTICK, /* R3 */
    PROSPERO_BTN_LEFTSHOULDER,
    PROSPERO_BTN_RIGHTSHOULDER,
    PROSPERO_BTN_DPAD_UP,
    PROSPERO_BTN_DPAD_DOWN,
    PROSPERO_BTN_DPAD_LEFT,
    PROSPERO_BTN_DPAD_RIGHT,
    PROSPERO_BTN_MISC1, /* touchpad click */
    PROSPERO_NUM_BUTTONS
};

enum {
    PROSPERO_AXIS_LEFTX = 0,
    PROSPERO_AXIS_LEFTY,
    PROSPERO_AXIS_RIGHTX,
    PROSPERO_AXIS_RIGHTY,
    PROSPERO_AXIS_TRIGGERLEFT,
    PROSPERO_AXIS_TRIGGERRIGHT,
    PROSPERO_NUM_AXES
};

static SDL_JoystickID prospero_instance_id = -1;
static int prospero_opened;

/*
 * A stick reports -128..127 and SDL wants -32768..32767. This form is exact at both
 * ends (-128 gives -32768, 127 gives 32767), where `v * 256` stops at 32512.
 */
static Sint16 prospero_stick_axis(int8_t v) {
    return (Sint16)(((int)v + 128) * 257 - 32768);
}

/* A trigger reports 0..255 and SDL's trigger axes run 0..32767, not the full signed
 * range. */
static Sint16 prospero_trigger_axis(uint8_t v) {
    return (Sint16)(((int)v * 32767) / 255);
}

static int PROSPERO_JoystickInit(void) {
    /*
     * A failure here is not fatal to the subsystem: the SDK remembers it, `GetCount`
     * answers zero and the title takes its no-controller path.
     */
    (void)oops_input_init();

    /*
     * `oops_input_read_state` folds the keyboard into port 0 by default. Under SDL the
     * keyboard arrives through `PROSPERO_PumpKeyboard`, so folding it into the pad
     * would deliver each key press twice.
     */
    oops_input_set_keyboard_as_pad(0);

    /* `oops_input_init` returns silently when already initialised, so this driver logs
       its own start. */
    oops_log_info("INPUT", "SDL joystick driver initialised");
    return 0;
}

/*
 * Titles often take a silent no-controller path when this answers zero, so the first
 * answer is logged once, with the poll's return to tell a failed poll from an absent
 * pad.
 */
static int PROSPERO_JoystickGetCount(void) {
    static int told = 0;
    oops_pad_state_t st;
    const int rc = oops_input_poll(PROSPERO_PAD_PORT, &st);
    const int count = (rc == 0 && st.connected) ? 1 : 0;

    if (!told) {
        told = 1;
        oops_log_info("INPUT", "SDL_NumJoysticks -> %d (poll rc=%d, connected=%d)",
                      count, rc, (rc == 0) ? (int)st.connected : -1);
    }
    return count;
}

static void PROSPERO_JoystickDetect(void) {}

static const char *PROSPERO_JoystickGetDeviceName(int device_index) {
    (void)device_index;
    return "OOPS Controller";
}

static const char *PROSPERO_JoystickGetDevicePath(int device_index) {
    (void)device_index;
    return NULL;
}

static int PROSPERO_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index) {
    (void)device_index;
    return -1;
}

static int PROSPERO_JoystickGetDevicePlayerIndex(int device_index) {
    (void)device_index;
    return 0;
}

static void PROSPERO_JoystickSetDevicePlayerIndex(int device_index, int player_index) {
    (void)device_index;
    (void)player_index;
    /* One pad on one port, so there is no index to set. */
}

static SDL_JoystickGUID PROSPERO_JoystickGetDeviceGUID(int device_index) {
    (void)device_index;
    /*
     * Derived from the name: the platform input library reports no USB vendor or
     * product id.
     */
    return SDL_CreateJoystickGUIDForName(PROSPERO_JoystickGetDeviceName(0));
}

static SDL_JoystickID PROSPERO_JoystickGetDeviceInstanceID(int device_index) {
    (void)device_index;
    if (prospero_instance_id < 0) {
        prospero_instance_id = SDL_GetNextJoystickInstanceID();
    }
    return prospero_instance_id;
}

static int PROSPERO_JoystickOpen(SDL_Joystick *joystick, int device_index) {
    if (device_index != 0 || PROSPERO_JoystickGetCount() == 0) {
        return SDL_SetError("prospero: no pad on port %d", PROSPERO_PAD_PORT);
    }

    joystick->nbuttons = PROSPERO_NUM_BUTTONS;
    joystick->naxes = PROSPERO_NUM_AXES;
    joystick->nhats =
        1; /* the D-pad again, for a title that reads hats rather than buttons */
    joystick->instance_id = PROSPERO_JoystickGetDeviceInstanceID(0);

    prospero_opened = 1;
    return 0;
}

static int PROSPERO_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                                   Uint16 high_frequency_rumble) {
    (void)joystick;
    /*
     * SDL's channels are the large (low frequency) and small (high frequency) motors;
     * `oops_input_set_rumble` takes them in reverse order, at 8 bits.
     */
    if (oops_input_set_rumble(PROSPERO_PAD_PORT, (uint8_t)(high_frequency_rumble >> 8),
                              (uint8_t)(low_frequency_rumble >> 8)) != 0) {
        return SDL_SetError("prospero: the pad refused the rumble");
    }
    return 0;
}

static int PROSPERO_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble,
                                           Uint16 right_rumble) {
    (void)joystick;
    (void)left_rumble;
    (void)right_rumble;
    /*
     * `oops_input_set_trigger_effect` refuses without a captured parameter block, so
     * trigger rumble is reported unsupported.
     */
    return SDL_Unsupported();
}

static Uint32 PROSPERO_JoystickGetCapabilities(SDL_Joystick *joystick) {
    (void)joystick;
    return SDL_JOYCAP_LED | SDL_JOYCAP_RUMBLE;
}

static int PROSPERO_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green,
                                   Uint8 blue) {
    (void)joystick;
    if (oops_input_set_lightbar(PROSPERO_PAD_PORT, red, green, blue) != 0) {
        return SDL_SetError("prospero: the pad refused the light");
    }
    return 0;
}

static int PROSPERO_JoystickSendEffect(SDL_Joystick *joystick, const void *data,
                                       int size) {
    (void)joystick;
    (void)data;
    (void)size;
    /* The SDK has no pass-through for vendor-specific reports. */
    return SDL_Unsupported();
}

static int PROSPERO_JoystickSetSensorsEnabled(SDL_Joystick *joystick,
                                              SDL_bool enabled) {
    (void)joystick;
    (void)enabled;
    /*
     * The accelerometer and gyroscope in `oops_pad_state_t` are not reported: SDL's
     * `SDL_PrivateJoystickSensor` needs its own units and a timestamp, and the
     * conversion is unmeasured. `SDL_GameControllerHasSensor` answers no.
     */
    return SDL_Unsupported();
}

static void PROSPERO_JoystickUpdate(SDL_Joystick *joystick) {
    oops_pad_state_t st;
    Uint8 hat = SDL_HAT_CENTERED;
    uint32_t b;

    if (oops_input_poll(PROSPERO_PAD_PORT, &st) != 0) {
        return;
    }
    b = st.buttons;

    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_LEFTX,
                            prospero_stick_axis(st.left_stick_x));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_LEFTY,
                            prospero_stick_axis(st.left_stick_y));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_RIGHTX,
                            prospero_stick_axis(st.right_stick_x));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_RIGHTY,
                            prospero_stick_axis(st.right_stick_y));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_TRIGGERLEFT,
                            prospero_trigger_axis(st.l2_trigger));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_TRIGGERRIGHT,
                            prospero_trigger_axis(st.r2_trigger));

#define PROSPERO_SEND(idx, bit)                                                        \
    SDL_PrivateJoystickButton(joystick, (Uint8)(idx),                                  \
                              (b & (bit)) ? SDL_PRESSED : SDL_RELEASED)

    PROSPERO_SEND(PROSPERO_BTN_A, OOPS_BUTTON_CROSS);
    PROSPERO_SEND(PROSPERO_BTN_B, OOPS_BUTTON_CIRCLE);
    PROSPERO_SEND(PROSPERO_BTN_X, OOPS_BUTTON_SQUARE);
    PROSPERO_SEND(PROSPERO_BTN_Y, OOPS_BUTTON_TRIANGLE);
    PROSPERO_SEND(PROSPERO_BTN_BACK, OOPS_BUTTON_CREATE);
    PROSPERO_SEND(PROSPERO_BTN_GUIDE, OOPS_BUTTON_PS);
    PROSPERO_SEND(PROSPERO_BTN_START, OOPS_BUTTON_OPTIONS);
    PROSPERO_SEND(PROSPERO_BTN_LEFTSTICK, OOPS_BUTTON_L3);
    PROSPERO_SEND(PROSPERO_BTN_RIGHTSTICK, OOPS_BUTTON_R3);
    PROSPERO_SEND(PROSPERO_BTN_LEFTSHOULDER, OOPS_BUTTON_L1);
    PROSPERO_SEND(PROSPERO_BTN_RIGHTSHOULDER, OOPS_BUTTON_R1);
    PROSPERO_SEND(PROSPERO_BTN_DPAD_UP, OOPS_BUTTON_UP);
    PROSPERO_SEND(PROSPERO_BTN_DPAD_DOWN, OOPS_BUTTON_DOWN);
    PROSPERO_SEND(PROSPERO_BTN_DPAD_LEFT, OOPS_BUTTON_LEFT);
    PROSPERO_SEND(PROSPERO_BTN_DPAD_RIGHT, OOPS_BUTTON_RIGHT);
    PROSPERO_SEND(PROSPERO_BTN_MISC1, OOPS_BUTTON_TOUCHPAD);

#undef PROSPERO_SEND

    /*
     * The D-pad again as a hat, from the same `buttons` word, for titles that read raw
     * joystick hats (common in SDL 1.2 ports through sdl12-compat).
     */
    if (b & OOPS_BUTTON_UP) {
        hat |= SDL_HAT_UP;
    }
    if (b & OOPS_BUTTON_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (b & OOPS_BUTTON_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (b & OOPS_BUTTON_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_PrivateJoystickHat(joystick, 0, hat);
}

static void PROSPERO_JoystickClose(SDL_Joystick *joystick) {
    (void)joystick;
    prospero_opened = 0;
    /*
     * The port stays open: `oops_input_close` tears down the pad for the whole payload,
     * and a close followed by an open (a hot-plug to SDL) must succeed. `Quit` closes
     * it.
     */
}

static void PROSPERO_JoystickQuit(void) {
    if (prospero_opened) {
        prospero_opened = 0;
    }
    oops_input_close();
}

static SDL_bool PROSPERO_JoystickGetGamepadMapping(int device_index,
                                                   SDL_GamepadMapping *out) {
    (void)device_index;
    if (!out) {
        return SDL_FALSE;
    }
    SDL_zerop(out);

#define PROSPERO_MAP_BUTTON(field, idx)                                                \
    out->field.kind = EMappingKind_Button;                                             \
    out->field.target = (Uint8)(idx)

#define PROSPERO_MAP_AXIS(field, idx)                                                  \
    out->field.kind = EMappingKind_Axis;                                               \
    out->field.target = (Uint8)(idx)

    PROSPERO_MAP_BUTTON(a, PROSPERO_BTN_A);
    PROSPERO_MAP_BUTTON(b, PROSPERO_BTN_B);
    PROSPERO_MAP_BUTTON(x, PROSPERO_BTN_X);
    PROSPERO_MAP_BUTTON(y, PROSPERO_BTN_Y);
    PROSPERO_MAP_BUTTON(back, PROSPERO_BTN_BACK);
    PROSPERO_MAP_BUTTON(guide, PROSPERO_BTN_GUIDE);
    PROSPERO_MAP_BUTTON(start, PROSPERO_BTN_START);
    PROSPERO_MAP_BUTTON(leftstick, PROSPERO_BTN_LEFTSTICK);
    PROSPERO_MAP_BUTTON(rightstick, PROSPERO_BTN_RIGHTSTICK);
    PROSPERO_MAP_BUTTON(leftshoulder, PROSPERO_BTN_LEFTSHOULDER);
    PROSPERO_MAP_BUTTON(rightshoulder, PROSPERO_BTN_RIGHTSHOULDER);
    PROSPERO_MAP_BUTTON(dpup, PROSPERO_BTN_DPAD_UP);
    PROSPERO_MAP_BUTTON(dpdown, PROSPERO_BTN_DPAD_DOWN);
    PROSPERO_MAP_BUTTON(dpleft, PROSPERO_BTN_DPAD_LEFT);
    PROSPERO_MAP_BUTTON(dpright, PROSPERO_BTN_DPAD_RIGHT);
    PROSPERO_MAP_BUTTON(misc1, PROSPERO_BTN_MISC1);

    PROSPERO_MAP_AXIS(leftx, PROSPERO_AXIS_LEFTX);
    PROSPERO_MAP_AXIS(lefty, PROSPERO_AXIS_LEFTY);
    PROSPERO_MAP_AXIS(rightx, PROSPERO_AXIS_RIGHTX);
    PROSPERO_MAP_AXIS(righty, PROSPERO_AXIS_RIGHTY);

    /*
     * The triggers already report 0..32767, so they are positive half-axes. A plain
     * `EMappingKind_Axis` is read as the full signed range and `SDL_gamecontroller.c`
     * would map a released trigger to the midpoint of its [0, 32767] output.
     */
    PROSPERO_MAP_AXIS(lefttrigger, PROSPERO_AXIS_TRIGGERLEFT);
    out->lefttrigger.half_axis_positive = SDL_TRUE;
    PROSPERO_MAP_AXIS(righttrigger, PROSPERO_AXIS_TRIGGERRIGHT);
    out->righttrigger.half_axis_positive = SDL_TRUE;

#undef PROSPERO_MAP_AXIS
#undef PROSPERO_MAP_BUTTON

    /* `paddle1`-`paddle4` and `touchpad` stay `EMappingKind_None`: this pad has no
       paddles, and the touchpad's coordinates are a separate SDL device rather than a
       controller element. */
    return SDL_TRUE;
}

SDL_JoystickDriver SDL_PROSPERO_JoystickDriver = {
    PROSPERO_JoystickInit,
    PROSPERO_JoystickGetCount,
    PROSPERO_JoystickDetect,
    PROSPERO_JoystickGetDeviceName,
    PROSPERO_JoystickGetDevicePath,
    PROSPERO_JoystickGetDeviceSteamVirtualGamepadSlot,
    PROSPERO_JoystickGetDevicePlayerIndex,
    PROSPERO_JoystickSetDevicePlayerIndex,
    PROSPERO_JoystickGetDeviceGUID,
    PROSPERO_JoystickGetDeviceInstanceID,
    PROSPERO_JoystickOpen,
    PROSPERO_JoystickRumble,
    PROSPERO_JoystickRumbleTriggers,
    PROSPERO_JoystickGetCapabilities,
    PROSPERO_JoystickSetLED,
    PROSPERO_JoystickSendEffect,
    PROSPERO_JoystickSetSensorsEnabled,
    PROSPERO_JoystickUpdate,
    PROSPERO_JoystickClose,
    PROSPERO_JoystickQuit,
    PROSPERO_JoystickGetGamepadMapping};

#endif /* SDL_JOYSTICK_PROSPERO */
