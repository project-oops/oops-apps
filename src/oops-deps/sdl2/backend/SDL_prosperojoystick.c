/*
 * The pad, as SDL's joystick subsystem sees it.
 *
 * # Why this is a driver and not four lines in the event pump
 *
 * A title asks `SDL_NumJoysticks` before it reads a button, and takes its no-controller path
 * when the answer is zero. Posting pad buttons from the video pump would put events in a queue
 * that nobody had opened a device to read - the buttons would arrive and the title would still
 * believe no pad was plugged in. So the pad belongs here, behind `SDL_JoystickDriver`, where
 * `GetCount`, `Open` and `Update` are the same three questions SDL asks every platform.
 *
 * # One device, and it is honest about being absent
 *
 * `GetCount` polls port 0 and answers 1 only when the pad reports `connected`. A title started
 * with no pad sees zero devices, which is true, rather than a device whose buttons never change.
 *
 * # The mapping is declared, not looked up
 *
 * `GetGamepadMapping` hands SDL the button and axis layout directly, so `SDL_GameController`
 * works with no entry in the community mapping database and no `SDL_GameControllerAddMapping`
 * call from the title. That database is a text file keyed by GUID which we would otherwise have
 * to ship and keep current; declaring the mapping is both smaller and exact.
 *
 * Buttons are numbered in `SDL_GameControllerButton` order deliberately. A title that ignores
 * the controller API and reads raw joystick buttons then still gets the conventional numbering
 * rather than whatever order this file happened to poll in.
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
enum
{
    PROSPERO_BTN_A = 0,       /* cross */
    PROSPERO_BTN_B,           /* circle */
    PROSPERO_BTN_X,           /* square */
    PROSPERO_BTN_Y,           /* triangle */
    PROSPERO_BTN_BACK,        /* create */
    PROSPERO_BTN_GUIDE,       /* the home button, when pad privilege allows it */
    PROSPERO_BTN_START,       /* options */
    PROSPERO_BTN_LEFTSTICK,   /* L3 */
    PROSPERO_BTN_RIGHTSTICK,  /* R3 */
    PROSPERO_BTN_LEFTSHOULDER,
    PROSPERO_BTN_RIGHTSHOULDER,
    PROSPERO_BTN_DPAD_UP,
    PROSPERO_BTN_DPAD_DOWN,
    PROSPERO_BTN_DPAD_LEFT,
    PROSPERO_BTN_DPAD_RIGHT,
    PROSPERO_BTN_MISC1, /* touchpad click */
    PROSPERO_NUM_BUTTONS
};

enum
{
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
 * A stick reports -128..127 and SDL wants -32768..32767, and the obvious `v * 256` reaches only
 * 32512 at full deflection - so a title calibrating against `SDL_JOYSTICK_AXIS_MAX` never sees
 * the stick reach the edge. This form is exact at both ends: -128 gives -32768, 127 gives 32767.
 */
static Sint16 prospero_stick_axis(int8_t v)
{
    return (Sint16)(((int)v + 128) * 257 - 32768);
}

/* A trigger reports 0..255 and SDL's trigger axes run 0..32767, not the full signed range. */
static Sint16 prospero_trigger_axis(uint8_t v)
{
    return (Sint16)(((int)v * 32767) / 255);
}

static int PROSPERO_JoystickInit(void)
{
    /*
     * A failure here is remembered by the SDK and repeated, so it is not fatal to the subsystem:
     * `GetCount` will answer zero and the title takes its no-controller path. Refusing to
     * initialise the whole joystick subsystem because no pad is signed in would be worse.
     */
    (void)oops_input_init();

    /*
     * **SDL delivers the keyboard as a keyboard, so the pad must not deliver it as a pad.**
     *
     * `oops_input_read_state` folds the keyboard into port 0 by default: it decodes Enter,
     * Escape, the arrows, WASD and the rest into `OOPS_BUTTON_*` so that an application with one
     * input call still gets a keyboard. That is the right default for an application using the
     * SDK directly - and exactly wrong underneath SDL, which already has a keyboard backend of
     * its own.
     *
     * With both, one press arrives twice: once as `SDL_KEYDOWN`, which Neverball turns into
     * `st_buttn(A)`, and again as `SDL_JOYBUTTONDOWN` from this driver, which it turns into
     * `st_buttn(A)` a second time. Two activations of one keypress, and every menu ran itself
     * twice - Play opened the level select and then that screen's Back, so the menu appeared to
     * bounce off itself. Measured on 2026-09-24: one `[KBD] event usage=0x28 down` in the log,
     * two `st_buttn: b=0 d=1` after it, and one key decoding to buttons 4 *and* 5.
     *
     * The keyboard is not lost - `PROSPERO_PumpKeyboard` is where it belongs.
     */
    oops_input_set_keyboard_as_pad(0);

    /* Said explicitly, because `oops_input_init` returns early and silently when something already
       called it - so its own line is not proof that this driver ran, and its absence is not proof
       that it did not. This one is. */
    oops_log_info("INPUT", "SDL joystick driver initialised");
    return 0;
}

/*
 * **This is the quietest failure in the whole input path, so it says what it answered.**
 *
 * A title asks `SDL_NumJoysticks()` once, at startup, and takes its no-controller branch if the
 * answer is zero - Extreme Tux Racer's `InitJoystick` sets `joystick = NULL` and returns without
 * printing anything at all. Nothing downstream of that ever mentions a pad again, so "the buttons
 * do nothing" arrives with no evidence attached and three layers to search.
 *
 * Logged once rather than per call: SDL asks this repeatedly and the answer is what matters, not
 * how often it was wanted. `oops_input_poll`'s own return is included because "the poll failed"
 * and "the poll worked and there is no pad" are different problems.
 */
static int PROSPERO_JoystickGetCount(void)
{
    static int told = 0;
    oops_pad_state_t st;
    const int rc = oops_input_poll(PROSPERO_PAD_PORT, &st);
    const int count = (rc == 0 && st.connected) ? 1 : 0;

    if (!told) {
        told = 1;
        oops_log_info("INPUT", "SDL_NumJoysticks -> %d (poll rc=%d, connected=%d)", count, rc,
                      (rc == 0) ? (int)st.connected : -1);
    }
    return count;
}

static void PROSPERO_JoystickDetect(void)
{
}

static const char *PROSPERO_JoystickGetDeviceName(int device_index)
{
    (void)device_index;
    return "OOPS Controller";
}

static const char *PROSPERO_JoystickGetDevicePath(int device_index)
{
    (void)device_index;
    return NULL;
}

static int PROSPERO_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    (void)device_index;
    return -1;
}

static int PROSPERO_JoystickGetDevicePlayerIndex(int device_index)
{
    (void)device_index;
    return 0;
}

static void PROSPERO_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    (void)device_index;
    (void)player_index;
    /* One pad on one port, so there is no index to set. */
}

static SDL_JoystickGUID PROSPERO_JoystickGetDeviceGUID(int device_index)
{
    (void)device_index;
    /*
     * Derived from the name rather than a vendor and product id. This is not the USB device - it
     * is the pad as the platform's own input library reports it - so inventing a vendor/product
     * pair would be a claim about hardware that nothing here read.
     */
    return SDL_CreateJoystickGUIDForName(PROSPERO_JoystickGetDeviceName(0));
}

static SDL_JoystickID PROSPERO_JoystickGetDeviceInstanceID(int device_index)
{
    (void)device_index;
    if (prospero_instance_id < 0) {
        prospero_instance_id = SDL_GetNextJoystickInstanceID();
    }
    return prospero_instance_id;
}

static int PROSPERO_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    if (device_index != 0 || PROSPERO_JoystickGetCount() == 0) {
        return SDL_SetError("prospero: no pad on port %d", PROSPERO_PAD_PORT);
    }

    joystick->nbuttons = PROSPERO_NUM_BUTTONS;
    joystick->naxes = PROSPERO_NUM_AXES;
    joystick->nhats = 1; /* the D-pad again, for a title that reads hats rather than buttons */
    joystick->instance_id = PROSPERO_JoystickGetDeviceInstanceID(0);

    prospero_opened = 1;
    return 0;
}

static int PROSPERO_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                                   Uint16 high_frequency_rumble)
{
    (void)joystick;
    /*
     * SDL's two channels are the large (low frequency) and small (high frequency) motors, which
     * is the order `oops_input_set_rumble` takes them in reverse. 16 bits down to 8 loses the
     * bottom byte and nothing a motor could express.
     */
    if (oops_input_set_rumble(PROSPERO_PAD_PORT, (uint8_t)(high_frequency_rumble >> 8),
                              (uint8_t)(low_frequency_rumble >> 8)) != 0) {
        return SDL_SetError("prospero: the pad refused the rumble");
    }
    return 0;
}

static int PROSPERO_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble,
                                            Uint16 right_rumble)
{
    (void)joystick;
    (void)left_rumble;
    (void)right_rumble;
    /*
     * The adaptive triggers can vibrate, but `oops_input_set_trigger_effect` is capture-gated and
     * refuses rather than guessing the platform's parameter block. Reporting unsupported is the
     * true answer until that lands; returning success would make a title believe it had haptics
     * it cannot feel.
     */
    return SDL_Unsupported();
}

static Uint32 PROSPERO_JoystickGetCapabilities(SDL_Joystick *joystick)
{
    (void)joystick;
    return SDL_JOYCAP_LED | SDL_JOYCAP_RUMBLE;
}

static int PROSPERO_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    (void)joystick;
    if (oops_input_set_lightbar(PROSPERO_PAD_PORT, red, green, blue) != 0) {
        return SDL_SetError("prospero: the pad refused the light");
    }
    return 0;
}

static int PROSPERO_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    (void)joystick;
    (void)data;
    (void)size;
    /* A pass-through for a vendor-specific report, which this SDK deliberately does not have. */
    return SDL_Unsupported();
}

static int PROSPERO_JoystickSetSensorsEnabled(SDL_Joystick *joystick, SDL_bool enabled)
{
    (void)joystick;
    (void)enabled;
    /*
     * The pad's accelerometer and gyroscope are in `oops_pad_state_t` and are not reported here
     * yet: SDL wants them through `SDL_PrivateJoystickSensor` with a timestamp and its own units,
     * and inventing a conversion is how a plausible-but-wrong orientation gets shipped. A
     * refusal leaves `SDL_GameControllerHasSensor` answering no, which is true today.
     */
    return SDL_Unsupported();
}

static void PROSPERO_JoystickUpdate(SDL_Joystick *joystick)
{
    oops_pad_state_t st;
    Uint8 hat = SDL_HAT_CENTERED;
    uint32_t b;

    if (oops_input_poll(PROSPERO_PAD_PORT, &st) != 0) {
        return;
    }
    b = st.buttons;

    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_LEFTX, prospero_stick_axis(st.left_stick_x));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_LEFTY, prospero_stick_axis(st.left_stick_y));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_RIGHTX, prospero_stick_axis(st.right_stick_x));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_RIGHTY, prospero_stick_axis(st.right_stick_y));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_TRIGGERLEFT,
                            prospero_trigger_axis(st.l2_trigger));
    SDL_PrivateJoystickAxis(joystick, PROSPERO_AXIS_TRIGGERRIGHT,
                            prospero_trigger_axis(st.r2_trigger));

#define PROSPERO_SEND(idx, bit) \
    SDL_PrivateJoystickButton(joystick, (Uint8)(idx), (b & (bit)) ? SDL_PRESSED : SDL_RELEASED)

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
     * The same D-pad as a hat. It is not a second device and it cannot disagree with the buttons
     * above - both are read from one `buttons` word in one poll. The declared gamepad mapping
     * uses the buttons, so this is visible only to a title that reads raw joystick hats, which
     * an SDL 1.2-era port reached through `sdl12-compat` is quite likely to do.
     *
     * L1 and L2 are not in `OOPS_BUTTON_*` order with the d-pad, so this reads the bits rather
     * than assuming a layout.
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

static void PROSPERO_JoystickClose(SDL_Joystick *joystick)
{
    (void)joystick;
    prospero_opened = 0;
    /*
     * The port is not closed here. `oops_input_close` tears down the pad for the whole payload,
     * and a title that closes one joystick and opens another - which is what a controller
     * hot-plug looks like to SDL - would find the second open failing. `Quit` owns it.
     */
}

static void PROSPERO_JoystickQuit(void)
{
    if (prospero_opened) {
        prospero_opened = 0;
    }
    oops_input_close();
}

static SDL_bool PROSPERO_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    (void)device_index;
    if (!out) {
        return SDL_FALSE;
    }
    SDL_zerop(out);

#define PROSPERO_MAP_BUTTON(field, idx)    \
    out->field.kind = EMappingKind_Button; \
    out->field.target = (Uint8)(idx)

#define PROSPERO_MAP_AXIS(field, idx)    \
    out->field.kind = EMappingKind_Axis; \
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
     * **The triggers are positive half-axes, and saying so is not optional.**
     *
     * `PROSPERO_AXIS_TRIGGERLEFT` already reports 0 to 32767 - `prospero_trigger_axis` builds it
     * that way, because that is the range SDL's trigger *outputs* use. But SDL reads the input
     * range from this mapping, and a plain `EMappingKind_Axis` means the full signed range: it
     * then maps [-32768, 32767] onto [0, 32767], and a released trigger sending 0 comes out at
     * the midpoint.
     *
     * That is not a guess. The first hardware run with a pad in hand logged
     * `axis 4 moved to 16383` and `axis 5 moved to 16383` with nothing held - half scale on both
     * triggers, at rest. `SDL_gamecontroller.c` forces the trigger output range to [0, 32767]
     * whatever the mapping says, so only the input side can be corrected, and this is where.
     *
     * The stick axes above are genuinely full-range on both sides and need no flag.
     */
    PROSPERO_MAP_AXIS(lefttrigger, PROSPERO_AXIS_TRIGGERLEFT);
    out->lefttrigger.half_axis_positive = SDL_TRUE;
    PROSPERO_MAP_AXIS(righttrigger, PROSPERO_AXIS_TRIGGERRIGHT);
    out->righttrigger.half_axis_positive = SDL_TRUE;

#undef PROSPERO_MAP_AXIS
#undef PROSPERO_MAP_BUTTON

    /* `paddle1`-`paddle4` and `touchpad` stay `EMappingKind_None`: this pad has no paddles, and
       the touchpad's coordinates are a separate SDL device rather than a controller element. */
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
    PROSPERO_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_PROSPERO */
