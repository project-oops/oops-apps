/*
 * GLFW and GLEW over oops-sdk, for Craft.
 *
 * `GLFW/glfw3.h` says which of GLFW's promises are kept; this file supplies the window,
 * the clock and the input. The USB keyboard and mouse and the pad are all read every
 * frame and whichever moved wins, with no mode to select.
 *
 * Devices are drained and callbacks run only inside `glfwPollEvents`, as GLFW does,
 * because Craft's handlers assume the typing state is consistent between frames.
 */
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "oops/display.h"
#include "oops/input.h"
#include "oops/keyboard.h"
#include "oops/mouse.h"
#include "oops/time.h"

#include <string.h>

/* GLEW's one variable, so Craft's assignment to it compiles. */
int glewExperimental = 0;

int glewInit(void) {
    return GLEW_OK;
}
const char *glewGetErrorString(int error) {
    (void)error;
    return "no error";
}

/* ---------------------------------------------------------------------------
 * The one window
 * ------------------------------------------------------------------------- */

struct GLFWwindow {
    oops_display_t *display;
    void *gl;
    int width, height;
    int should_close;
    int cursor_mode;

    GLFWkeyfun on_key;
    GLFWcharfun on_char;
    GLFWmousebuttonfun on_mouse_button;
    GLFWscrollfun on_scroll;

    /* Held state, which is what `glfwGetKey` and Craft's movement code read. Indexed by
     * GLFW key code for the named keys and by ASCII for the letters, which is what
     * Craft passes: `glfwGetKey(window, CRAFT_KEY_FORWARD)` where the config is the
     * character 'W'. */
    unsigned char key_down[512];
    unsigned char mouse_down[8];

    /* The cursor position Craft reads for look-around. The mouse gives relative motion,
     * so this is accumulated here - which is also what `GLFW_CURSOR_DISABLED` means on
     * a desktop. */
    double cursor_x, cursor_y;
};

static struct GLFWwindow s_window;
static GLFWmonitor *const s_monitor =
    (GLFWmonitor *)&s_window; /* one display, one monitor */
static GLFWvidmode s_modes[1];
static double s_time_base;

/* ---------------------------------------------------------------------------
 * Lifetime
 * ------------------------------------------------------------------------- */

int glfwInit(void) {
    memset(&s_window, 0, sizeof(s_window));
    s_window.cursor_mode = GLFW_CURSOR_NORMAL;
    /* Neither device is required: a console with no USB keyboard is the normal case,
     * and the pad below is what that player uses. Both initialisers report and are not
     * checked. */
    (void)oops_keyboard_init();
    (void)oops_mouse_init();
    (void)oops_input_init();
    s_time_base = (double)oops_time_get_ns() / 1e9;
    return 1;
}

void glfwTerminate(void) {
    oops_keyboard_close();
    oops_mouse_close();
    oops_input_close();
    if (s_window.gl) {
        glContextDestroy(s_window.gl);
        s_window.gl = (void *)0;
    }
    if (s_window.display) {
        oops_display_close(s_window.display);
        s_window.display = (oops_display_t *)0;
    }
}

/* The requested size is ignored and the display's own is returned; Craft reads the size
 * back. The display is still opened with `OOPS_DISPLAY_DEFAULT_*`, as `oops_gfx_create`
 * does, because `agc_display_open_adopting` silently refuses a zero dimension. */
GLFWwindow *glfwCreateWindow(int width, int height, const char *title,
                             GLFWmonitor *monitor, GLFWwindow *share) {
    (void)width;
    (void)height;
    (void)title;
    (void)monitor;
    (void)share;
    s_window.display =
        oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, OOPS_DISPLAY_DEFAULT_WIDTH,
                          OOPS_DISPLAY_DEFAULT_HEIGHT);
    if (!s_window.display)
        return (GLFWwindow *)0;
    s_window.width = (int)oops_display_get_width(s_window.display);
    s_window.height = (int)oops_display_get_height(s_window.display);
    s_window.gl = glContextCreate(s_window.display);
    if (!s_window.gl)
        return (GLFWwindow *)0;
    /* Craft is a GL 2.1 program, and oops-gl exposes only the entry points a context's
     * version names, so shaders need at least 2.0. */
    glContextMakeCurrent(s_window.gl);
    glContextSetVersion(2, 0);
    return &s_window;
}

void glfwMakeContextCurrent(GLFWwindow *window) {
    if (window && window->gl)
        glContextMakeCurrent(window->gl);
}

void glfwSwapBuffers(GLFWwindow *window) {
    (void)window;
    glSwapBuffers();
}

/* The display's flip rate is what it is; there is nothing to ask for. */
void glfwSwapInterval(int interval) {
    (void)interval;
}

int glfwWindowShouldClose(GLFWwindow *window) {
    return window ? window->should_close : 1;
}

void glfwGetWindowSize(GLFWwindow *window, int *width, int *height) {
    if (width)
        *width = window ? window->width : 0;
    if (height)
        *height = window ? window->height : 0;
}

/* The same numbers as the window size: there is one pixel grid here. */
void glfwGetFramebufferSize(GLFWwindow *window, int *width, int *height) {
    glfwGetWindowSize(window, width, height);
}

GLFWmonitor *glfwGetPrimaryMonitor(void) {
    return s_monitor;
}

const GLFWvidmode *glfwGetVideoModes(GLFWmonitor *monitor, int *count) {
    (void)monitor;
    s_modes[0].width = s_window.width;
    s_modes[0].height = s_window.height;
    s_modes[0].redBits = 8;
    s_modes[0].greenBits = 8;
    s_modes[0].blueBits = 8;
    s_modes[0].refreshRate = 60;
    if (count)
        *count = 1;
    return s_modes;
}

/* ---------------------------------------------------------------------------
 * Time
 * ------------------------------------------------------------------------- */

double glfwGetTime(void) {
    return (double)oops_time_get_ns() / 1e9 - s_time_base;
}

void glfwSetTime(double time) {
    s_time_base = (double)oops_time_get_ns() / 1e9 - time;
}

/* ---------------------------------------------------------------------------
 * Input
 * ------------------------------------------------------------------------- */

/*
 * A USB HID usage code (as `oops_key_event_t` carries it) to Craft's key number.
 *
 * Craft's `config.h` passes character literals such as `'W'` to `glfwGetKey`, because
 * GLFW's letter key codes are their uppercase ASCII. Letters and digits map to ASCII,
 * named keys to `GLFW_KEY_*`, and anything else returns -1.
 */
static int hid_to_glfw(unsigned int usage) {
    if (usage >= 0x04u && usage <= 0x1du)
        return (int)('A' + (usage - 0x04u)); /* a..z */
    if (usage >= 0x1eu && usage <= 0x26u)
        return (int)('1' + (usage - 0x1eu)); /* 1..9 */
    switch (usage) {
    case 0x27:
        return '0';
    case 0x28:
        return GLFW_KEY_ENTER;
    case 0x29:
        return GLFW_KEY_ESCAPE;
    case 0x2a:
        return GLFW_KEY_BACKSPACE;
    case 0x2b:
        return GLFW_KEY_TAB;
    case 0x2c:
        return GLFW_KEY_SPACE;
    case 0x2d:
        return '-';
    case 0x2e:
        return '=';
    case 0x33:
        return ';';
    case 0x34:
        return '\'';
    case 0x35:
        return '`';
    case 0x36:
        return ',';
    case 0x37:
        return '.';
    case 0x38:
        return '/';
    case 0x4f:
        return GLFW_KEY_RIGHT;
    case 0x50:
        return GLFW_KEY_LEFT;
    case 0x51:
        return GLFW_KEY_DOWN;
    case 0x52:
        return GLFW_KEY_UP;
    case 0xe1:
        return GLFW_KEY_LEFT_SHIFT;
    default:
        return -1;
    }
}

/* The printable character a usage produces, for `glfwSetCharCallback` - which is how
 * Craft's chat and sign text are typed. Shift is the only modifier that changes it
 * here. */
static unsigned int hid_to_char(unsigned int usage, unsigned int mods) {
    const int shift = (mods & OOPS_KMOD_SHIFT) != 0u;
    if (usage >= 0x04u && usage <= 0x1du) {
        const unsigned int base = 'a' + (usage - 0x04u);
        return shift ? base - 32u : base;
    }
    if (usage >= 0x1eu && usage <= 0x26u) {
        static const char shifted[9] = {'!', '@', '#', '$', '%', '^', '&', '*', '('};
        return shift ? (unsigned int)shifted[usage - 0x1eu] : '1' + (usage - 0x1eu);
    }
    switch (usage) {
    case 0x27:
        return shift ? ')' : '0';
    case 0x2c:
        return ' ';
    case 0x2d:
        return shift ? '_' : '-';
    case 0x2e:
        return shift ? '+' : '=';
    case 0x33:
        return shift ? ':' : ';';
    case 0x34:
        return shift ? '"' : '\'';
    case 0x35:
        return shift ? '~' : '`';
    case 0x36:
        return shift ? '<' : ',';
    case 0x37:
        return shift ? '>' : '.';
    case 0x38:
        return shift ? '?' : '/';
    default:
        return 0u;
    }
}

static int glfw_mods_from_oops(unsigned int mods) {
    int out = 0;
    if (mods & OOPS_KMOD_SHIFT)
        out |= GLFW_MOD_SHIFT;
    if (mods & OOPS_KMOD_CTRL)
        out |= GLFW_MOD_CONTROL;
    if (mods & OOPS_KMOD_ALT)
        out |= GLFW_MOD_ALT;
    if (mods & OOPS_KMOD_GUI)
        out |= GLFW_MOD_SUPER;
    return out;
}

static void drain_keyboard(struct GLFWwindow *w) {
    oops_key_event_t events[OOPS_MAX_KEY_EVENTS];
    const int n = oops_keyboard_read(events, OOPS_MAX_KEY_EVENTS);
    for (int i = 0; i < n; i++) {
        const int key = hid_to_glfw(events[i].usage);
        const int action =
            events[i].transition == OOPS_KEY_DOWN ? GLFW_PRESS : GLFW_RELEASE;
        const int mods = glfw_mods_from_oops(events[i].modifiers);
        if (key >= 0 && key < (int)sizeof(w->key_down)) {
            w->key_down[key] = (unsigned char)(action == GLFW_PRESS);
        }
        if (key >= 0 && w->on_key)
            w->on_key((GLFWwindow *)w, key, 0, action, mods);
        /* The character callback fires only on a press of a key that produces one. */
        if (action == GLFW_PRESS && w->on_char) {
            const unsigned int ch = hid_to_char(events[i].usage, events[i].modifiers);
            if (ch)
                w->on_char((GLFWwindow *)w, ch);
        }
    }
}

static void drain_mouse(struct GLFWwindow *w) {
    oops_mouse_state_t samples[OOPS_MAX_MOUSE_SAMPLES];
    const int n = oops_mouse_read(samples, OOPS_MAX_MOUSE_SAMPLES);
    for (int i = 0; i < n; i++) {
        w->cursor_x += (double)samples[i].dx;
        w->cursor_y += (double)samples[i].dy;
        if (samples[i].wheel && w->on_scroll) {
            w->on_scroll((GLFWwindow *)w, 0.0, (double)samples[i].wheel);
        }
        static const unsigned char order[3] = {OOPS_MOUSE_LEFT, OOPS_MOUSE_RIGHT,
                                               OOPS_MOUSE_MIDDLE};
        static const int glfw_button[3] = {
            GLFW_MOUSE_BUTTON_LEFT, GLFW_MOUSE_BUTTON_RIGHT, GLFW_MOUSE_BUTTON_MIDDLE};
        for (int b = 0; b < 3; b++) {
            const unsigned char now =
                (unsigned char)((samples[i].buttons & order[b]) != 0u);
            if (now == w->mouse_down[glfw_button[b]])
                continue;
            w->mouse_down[glfw_button[b]] = now;
            if (w->on_mouse_button) {
                w->on_mouse_button((GLFWwindow *)w, glfw_button[b],
                                   now ? GLFW_PRESS : GLFW_RELEASE, 0);
            }
        }
    }
}

/*
 * The pad, mapped to the same keys and buttons the keyboard and mouse produce, so
 * nothing downstream knows which device was used. Left stick is WASD, right stick is
 * the mouse (scaled per frame, as mouse motion is). Cross jumps, Square breaks (left
 * button), Circle places (right button), L1 and R1 cycle the held block, and Options
 * opens the chat line (`t`).
 */
#define PAD_STICK_DEADZONE 32
#define PAD_LOOK_SCALE 0.35

static void drain_pad(struct GLFWwindow *w) {
    oops_pad_state_t pad;
    if (oops_input_poll(0u, &pad) != 0 || !pad.connected)
        return;

    if (pad.left_stick_y < -PAD_STICK_DEADZONE)
        w->key_down['W'] = 1;
    else if (pad.left_stick_y > PAD_STICK_DEADZONE)
        w->key_down['S'] = 1;
    if (pad.left_stick_x < -PAD_STICK_DEADZONE)
        w->key_down['A'] = 1;
    else if (pad.left_stick_x > PAD_STICK_DEADZONE)
        w->key_down['D'] = 1;

    if (pad.right_stick_x < -PAD_STICK_DEADZONE ||
        pad.right_stick_x > PAD_STICK_DEADZONE) {
        w->cursor_x += (double)pad.right_stick_x * PAD_LOOK_SCALE;
    }
    if (pad.right_stick_y < -PAD_STICK_DEADZONE ||
        pad.right_stick_y > PAD_STICK_DEADZONE) {
        w->cursor_y += (double)pad.right_stick_y * PAD_LOOK_SCALE;
    }

    if (pad.buttons & OOPS_BUTTON_CROSS)
        w->key_down[GLFW_KEY_SPACE] = 1;
    if (pad.buttons & OOPS_BUTTON_R1)
        w->key_down['E'] = 1;
    if (pad.buttons & OOPS_BUTTON_L1)
        w->key_down['R'] = 1;
    if (pad.buttons & OOPS_BUTTON_OPTIONS)
        w->key_down['T'] = 1;

    /* Break and place are mouse buttons, edge-detected like the mouse, because Craft
     * acts on the transition rather than the hold. */
    static const struct {
        unsigned int mask;
        int button;
    } CLICKS[2] = {
        {OOPS_BUTTON_SQUARE, GLFW_MOUSE_BUTTON_LEFT},
        {OOPS_BUTTON_CIRCLE, GLFW_MOUSE_BUTTON_RIGHT},
    };
    for (int i = 0; i < 2; i++) {
        const unsigned char now = (unsigned char)((pad.buttons & CLICKS[i].mask) != 0u);
        if (now == w->mouse_down[CLICKS[i].button])
            continue;
        w->mouse_down[CLICKS[i].button] = now;
        if (w->on_mouse_button) {
            w->on_mouse_button((GLFWwindow *)w, CLICKS[i].button,
                               now ? GLFW_PRESS : GLFW_RELEASE, 0);
        }
    }
}

void glfwPollEvents(void) {
    struct GLFWwindow *w = &s_window;
    /* The keys the pad writes are cleared first: a pad reports its whole state every
     * poll, while a keyboard reports transitions and a key stays down until release. */
    w->key_down['W'] = 0;
    w->key_down['A'] = 0;
    w->key_down['S'] = 0;
    w->key_down['D'] = 0;
    w->key_down[GLFW_KEY_SPACE] = 0;
    w->key_down['E'] = 0;
    w->key_down['R'] = 0;
    w->key_down['T'] = 0;
    drain_keyboard(w);
    drain_mouse(w);
    drain_pad(w);
}

int glfwGetKey(GLFWwindow *window, int key) {
    if (!window || key < 0 || key >= (int)sizeof(window->key_down))
        return GLFW_RELEASE;
    return window->key_down[key] ? GLFW_PRESS : GLFW_RELEASE;
}

void glfwGetCursorPos(GLFWwindow *window, double *xpos, double *ypos) {
    if (xpos)
        *xpos = window ? window->cursor_x : 0.0;
    if (ypos)
        *ypos = window ? window->cursor_y : 0.0;
}

void glfwSetInputMode(GLFWwindow *window, int mode, int value) {
    if (window && mode == GLFW_CURSOR)
        window->cursor_mode = value;
}

int glfwGetInputMode(GLFWwindow *window, int mode) {
    if (window && mode == GLFW_CURSOR)
        return window->cursor_mode;
    return 0;
}

/* Nothing to paste from; Craft's chat-line paste checks for NULL. */
const char *glfwGetClipboardString(GLFWwindow *window) {
    (void)window;
    return (const char *)0;
}

GLFWkeyfun glfwSetKeyCallback(GLFWwindow *window, GLFWkeyfun cbfun) {
    GLFWkeyfun old = window ? window->on_key : (GLFWkeyfun)0;
    if (window)
        window->on_key = cbfun;
    return old;
}

GLFWcharfun glfwSetCharCallback(GLFWwindow *window, GLFWcharfun cbfun) {
    GLFWcharfun old = window ? window->on_char : (GLFWcharfun)0;
    if (window)
        window->on_char = cbfun;
    return old;
}

GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow *window,
                                              GLFWmousebuttonfun cbfun) {
    GLFWmousebuttonfun old = window ? window->on_mouse_button : (GLFWmousebuttonfun)0;
    if (window)
        window->on_mouse_button = cbfun;
    return old;
}

GLFWscrollfun glfwSetScrollCallback(GLFWwindow *window, GLFWscrollfun cbfun) {
    GLFWscrollfun old = window ? window->on_scroll : (GLFWscrollfun)0;
    if (window)
        window->on_scroll = cbfun;
    return old;
}
