/*
 * SDL3's joystick driver for this console, over `oops/input.h`.
 *
 * Buttons are numbered in `SDL_GamepadButton` order, so a title reading raw joystick
 * buttons gets cross as 0, circle 1, square 2, triangle 3. The driver reports buttons,
 * two sticks, two analogue triggers, the touchpad click, rumble and the light bar; the
 * IMU is not reported because `SDL_SENSOR_DISABLED` compiles the sensor subsystem out.
 *
 * `OOPS_BUTTON_CREATE` and `OOPS_BUTTON_PS` are the same bit, so it is reported as Back
 * only: the system software intercepts the PS button before a payload sees it.
 */
#include "SDL_internal.h"

#ifdef SDL_JOYSTICK_PRIVATE

#include "joystick/SDL_sysjoystick.h"
#include "joystick/SDL_joystick_c.h"

#include "oops/input.h"

/* `SDL_GamepadButton`'s order. */
enum {
    PROSPERO_BTN_SOUTH = 0,  /* cross */
    PROSPERO_BTN_EAST,       /* circle */
    PROSPERO_BTN_WEST,       /* square */
    PROSPERO_BTN_NORTH,      /* triangle */
    PROSPERO_BTN_BACK,       /* create (shares its bit with PS) */
    PROSPERO_BTN_START,      /* options */
    PROSPERO_BTN_LEFTSTICK,  /* L3 */
    PROSPERO_BTN_RIGHTSTICK, /* R3 */
    PROSPERO_BTN_LEFTSHOULDER,
    PROSPERO_BTN_RIGHTSHOULDER,
    PROSPERO_BTN_TOUCHPAD, /* the touchpad click, which SDL calls a "misc" button */
    PROSPERO_BTN_COUNT
};

/* Left X/Y, right X/Y, L2, R2 - `SDL_GamepadAxis`'s order too. */
#define PROSPERO_AXIS_COUNT 6

typedef struct {
    unsigned int port;
    bool connected;
    SDL_JoystickID instance_id;
} PROSPERO_PadSlot;

static PROSPERO_PadSlot prospero_pads[OOPS_MAX_PADS];
static bool prospero_input_ready;

/* Device list. */

static bool PROSPERO_JoystickInit(void) {
    /* Reported, because nothing downstream would otherwise mention the missing pads. */
    if (oops_input_init() != 0) {
        return SDL_SetError("prospero: oops_input_init() failed");
    }
    prospero_input_ready = true;

    for (unsigned int i = 0; i < OOPS_MAX_PADS; i++) {
        prospero_pads[i].port = i;
        prospero_pads[i].connected = false;
        prospero_pads[i].instance_id = 0;
    }
    return true;
}

/* The slot count, fixed so SDL's device index stays stable between `GetCount` and
 * `Open`. `Detect` reports connections and removals. */
static int PROSPERO_JoystickGetCount(void) {
    return OOPS_MAX_PADS;
}

static void PROSPERO_JoystickDetect(void) {
    oops_pad_state_t st;

    for (unsigned int i = 0; i < OOPS_MAX_PADS; i++) {
        const bool now = (oops_input_poll(i, &st) == 0) && st.connected;
        if (now == prospero_pads[i].connected) {
            continue;
        }
        prospero_pads[i].connected = now;
        if (now) {
            prospero_pads[i].instance_id = SDL_GetNextObjectID();
            SDL_PrivateJoystickAdded(prospero_pads[i].instance_id);
        } else if (prospero_pads[i].instance_id) {
            SDL_PrivateJoystickRemoved(prospero_pads[i].instance_id);
            prospero_pads[i].instance_id = 0;
        }
    }
}

static bool PROSPERO_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id,
                                             Uint16 version, const char *name) {
    /* Asked by the HID layer, which is not built here. */
    (void)vendor_id;
    (void)product_id;
    (void)version;
    (void)name;
    return false;
}

static const char *PROSPERO_JoystickGetDeviceName(int device_index) {
    (void)device_index;
    return "DualSense Wireless Controller";
}

static const char *PROSPERO_JoystickGetDevicePath(int device_index) {
    /* No device node; NULL is SDL's "no path". */
    (void)device_index;
    return NULL;
}

static int PROSPERO_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index) {
    (void)device_index;
    return -1;
}

/* The pad's port is its player index. */
static int PROSPERO_JoystickGetDevicePlayerIndex(int device_index) {
    return device_index;
}

static void PROSPERO_JoystickSetDevicePlayerIndex(int device_index, int player_index) {
    /* The system assigns ports and a payload cannot renumber them, as in SDL's own
     * fixed-port backends. */
    (void)device_index;
    (void)player_index;
}

static SDL_GUID PROSPERO_JoystickGetDeviceGUID(int device_index) {
    /* Sony's vendor id and the DualSense's product id. The bus is virtual: to a payload
     * the pad arrives over neither USB nor Bluetooth. */
    (void)device_index;
    return SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_VIRTUAL, 0x054C, 0x0CE6, 0x0100,
                                  "Sony Interactive Entertainment",
                                  "DualSense Wireless Controller", 0, 0);
}

static SDL_JoystickID PROSPERO_JoystickGetDeviceInstanceID(int device_index) {
    return prospero_pads[device_index].instance_id;
}

/* An open pad. */

static bool PROSPERO_JoystickOpen(SDL_Joystick *joystick, int device_index) {
    joystick->instance_id = prospero_pads[device_index].instance_id;
    joystick->nbuttons = PROSPERO_BTN_COUNT;
    joystick->naxes = PROSPERO_AXIS_COUNT;
    joystick->nhats = 1; /* the D-pad */

    /* The port number plus one, so `Update` finds its pad without a search. */
    joystick->hwdata = (struct joystick_hwdata *)(uintptr_t)(device_index + 1);
    return true;
}

static void PROSPERO_JoystickClose(SDL_Joystick *joystick) {
    joystick->hwdata = NULL;
}

static unsigned int PROSPERO_PortOf(SDL_Joystick *joystick) {
    return (unsigned int)((uintptr_t)joystick->hwdata - 1u);
}

static bool PROSPERO_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                                    Uint16 high_frequency_rumble) {
    /* `oops_input_set_rumble` takes 8-bit motors; SDL's 16-bit value keeps its high
     * byte. */
    if (oops_input_set_rumble(PROSPERO_PortOf(joystick),
                              (uint8_t)(low_frequency_rumble >> 8),
                              (uint8_t)(high_frequency_rumble >> 8)) != 0) {
        return SDL_SetError("prospero: oops_input_set_rumble() failed");
    }
    return true;
}

static bool PROSPERO_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left,
                                            Uint16 right) {
    /*
     * Unsupported: `oops_input_set_trigger_effect` takes a resistance effect, not a
     * rumble amplitude. Callers fall back to ordinary rumble.
     */
    (void)joystick;
    (void)left;
    (void)right;
    return SDL_Unsupported();
}

static bool PROSPERO_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green,
                                    Uint8 blue) {
    if (oops_input_set_lightbar(PROSPERO_PortOf(joystick), red, green, blue) != 0) {
        return SDL_SetError("prospero: oops_input_set_lightbar() failed");
    }
    return true;
}

static bool PROSPERO_JoystickSendEffect(SDL_Joystick *joystick, const void *data,
                                        int size) {
    /* A raw HID report; there is no path for one. */
    (void)joystick;
    (void)data;
    (void)size;
    return SDL_Unsupported();
}

static bool PROSPERO_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled) {
    /* `oops_pad_state_t` carries the IMU, but `SDL_SENSOR_DISABLED` leaves SDL no
     * sensor subsystem to deliver it to. */
    (void)joystick;
    (void)enabled;
    return SDL_Unsupported();
}

/* Polling. */

/* An 8-bit stick scaled by 256 to a 16-bit axis (127 becomes 32512), as SDL's own
 * 8-bit backends do, so both halves scale alike and the centre stays at zero. */
static Sint16 PROSPERO_Axis8(int8_t v) {
    return (Sint16)(v * 256);
}

/* A trigger is unsigned 0..255 and SDL wants a signed axis over the same full range. */
static Sint16 PROSPERO_Trigger8(uint8_t v) {
    return (Sint16)((int)v * 257 - 32768);
}

static void PROSPERO_JoystickUpdate(SDL_Joystick *joystick) {
    const unsigned int port = PROSPERO_PortOf(joystick);
    const Uint64 now = SDL_GetTicksNS();
    oops_pad_state_t st;
    Uint8 hat = SDL_HAT_CENTERED;

    if (oops_input_poll(port, &st) != 0) {
        return;
    }

#define PROSPERO_BTN(sdl_button, oops_bit)                                             \
    SDL_SendJoystickButton(now, joystick, (Uint8)(sdl_button),                         \
                           (st.buttons & (oops_bit)) != 0)

    PROSPERO_BTN(PROSPERO_BTN_SOUTH, OOPS_BUTTON_CROSS);
    PROSPERO_BTN(PROSPERO_BTN_EAST, OOPS_BUTTON_CIRCLE);
    PROSPERO_BTN(PROSPERO_BTN_WEST, OOPS_BUTTON_SQUARE);
    PROSPERO_BTN(PROSPERO_BTN_NORTH, OOPS_BUTTON_TRIANGLE);
    PROSPERO_BTN(PROSPERO_BTN_BACK, OOPS_BUTTON_CREATE);
    PROSPERO_BTN(PROSPERO_BTN_START, OOPS_BUTTON_OPTIONS);
    PROSPERO_BTN(PROSPERO_BTN_LEFTSTICK, OOPS_BUTTON_L3);
    PROSPERO_BTN(PROSPERO_BTN_RIGHTSTICK, OOPS_BUTTON_R3);
    PROSPERO_BTN(PROSPERO_BTN_LEFTSHOULDER, OOPS_BUTTON_L1);
    PROSPERO_BTN(PROSPERO_BTN_RIGHTSHOULDER, OOPS_BUTTON_R1);
    PROSPERO_BTN(PROSPERO_BTN_TOUCHPAD, OOPS_BUTTON_TOUCHPAD);

#undef PROSPERO_BTN

    /* No Guide: `OOPS_BUTTON_PS` is the same bit as `OOPS_BUTTON_CREATE`. */

    SDL_SendJoystickAxis(now, joystick, 0, PROSPERO_Axis8(st.left_stick_x));
    SDL_SendJoystickAxis(now, joystick, 1, PROSPERO_Axis8(st.left_stick_y));
    SDL_SendJoystickAxis(now, joystick, 2, PROSPERO_Axis8(st.right_stick_x));
    SDL_SendJoystickAxis(now, joystick, 3, PROSPERO_Axis8(st.right_stick_y));
    SDL_SendJoystickAxis(now, joystick, 4, PROSPERO_Trigger8(st.l2_trigger));
    SDL_SendJoystickAxis(now, joystick, 5, PROSPERO_Trigger8(st.r2_trigger));

    if (st.buttons & OOPS_BUTTON_UP) {
        hat |= SDL_HAT_UP;
    }
    if (st.buttons & OOPS_BUTTON_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (st.buttons & OOPS_BUTTON_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (st.buttons & OOPS_BUTTON_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_SendJoystickHat(now, joystick, 0, hat);
}

static void PROSPERO_JoystickQuit(void) {
    if (prospero_input_ready) {
        oops_input_close();
        prospero_input_ready = false;
    }
}

/*
 * The gamepad layout, stated in the numbering this file sends rather than as a mapping
 * string in SDL's database. Guide is left out because its bit is Create's.
 */
static bool PROSPERO_JoystickGetGamepadMapping(int device_index,
                                               SDL_GamepadMapping *out) {
    (void)device_index;

    out->a.kind = EMappingKind_Button;
    out->a.target = PROSPERO_BTN_SOUTH;
    out->b.kind = EMappingKind_Button;
    out->b.target = PROSPERO_BTN_EAST;
    out->x.kind = EMappingKind_Button;
    out->x.target = PROSPERO_BTN_WEST;
    out->y.kind = EMappingKind_Button;
    out->y.target = PROSPERO_BTN_NORTH;
    out->back.kind = EMappingKind_Button;
    out->back.target = PROSPERO_BTN_BACK;
    out->start.kind = EMappingKind_Button;
    out->start.target = PROSPERO_BTN_START;
    out->leftstick.kind = EMappingKind_Button;
    out->leftstick.target = PROSPERO_BTN_LEFTSTICK;
    out->rightstick.kind = EMappingKind_Button;
    out->rightstick.target = PROSPERO_BTN_RIGHTSTICK;
    out->leftshoulder.kind = EMappingKind_Button;
    out->leftshoulder.target = PROSPERO_BTN_LEFTSHOULDER;
    out->rightshoulder.kind = EMappingKind_Button;
    out->rightshoulder.target = PROSPERO_BTN_RIGHTSHOULDER;

    out->dpup.kind = EMappingKind_Hat;
    out->dpup.target = (0 << 4) | SDL_HAT_UP;
    out->dpdown.kind = EMappingKind_Hat;
    out->dpdown.target = (0 << 4) | SDL_HAT_DOWN;
    out->dpleft.kind = EMappingKind_Hat;
    out->dpleft.target = (0 << 4) | SDL_HAT_LEFT;
    out->dpright.kind = EMappingKind_Hat;
    out->dpright.target = (0 << 4) | SDL_HAT_RIGHT;

    out->leftx.kind = EMappingKind_Axis;
    out->leftx.target = 0;
    out->lefty.kind = EMappingKind_Axis;
    out->lefty.target = 1;
    out->rightx.kind = EMappingKind_Axis;
    out->rightx.target = 2;
    out->righty.kind = EMappingKind_Axis;
    out->righty.target = 3;
    out->lefttrigger.kind = EMappingKind_Axis;
    out->lefttrigger.target = 4;
    out->righttrigger.kind = EMappingKind_Axis;
    out->righttrigger.target = 5;

    out->misc1.kind = EMappingKind_Button;
    out->misc1.target = PROSPERO_BTN_TOUCHPAD;

    return true;
}

SDL_JoystickDriver SDL_PRIVATE_JoystickDriver = {
    PROSPERO_JoystickInit,
    PROSPERO_JoystickGetCount,
    PROSPERO_JoystickDetect,
    PROSPERO_JoystickIsDevicePresent,
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
    PROSPERO_JoystickSetLED,
    PROSPERO_JoystickSendEffect,
    PROSPERO_JoystickSetSensorsEnabled,
    PROSPERO_JoystickUpdate,
    PROSPERO_JoystickClose,
    PROSPERO_JoystickQuit,
    PROSPERO_JoystickGetGamepadMapping,
};

#endif /* SDL_JOYSTICK_PRIVATE */
