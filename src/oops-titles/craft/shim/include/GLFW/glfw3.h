/*
 * GLFW 3, as much of it as Craft calls - over oops-sdk's display, input and clock.
 *
 * # Why a shim rather than a port of GLFW
 *
 * GLFW is a *window system* abstraction, and this console has no window system: there is one
 * display, it is always full screen, and it is already open by the time a title runs. Porting
 * GLFW would mean writing a new platform backend for it - X11's and Win32's siblings - and then
 * having that backend immediately answer "one window, no chrome, no resize" to everything it was
 * asked. The twenty-three entry points below are the whole of what Craft asks GLFW for, and
 * answering those directly is a smaller and far more readable thing than a backend.
 *
 * **The surface is measured, not guessed.** It is every `glfw*` and `GLFW_*` token that appears
 * in Craft's own sources - `grep -ohE "glfw[A-Za-z]+" src/*.c src/*.h` and the same for the
 * constants - so a function that is missing here is one Craft does not call, and adding one
 * speculatively would be adding code nothing exercises.
 *
 * # What is deliberately not GLFW's behaviour
 *
 *   - **`glfwCreateWindow` ignores its size and returns the display's own.** There is nothing to
 *     resize. Craft reads the size back with `glfwGetWindowSize` and `glfwGetFramebufferSize`
 *     and lays out from that, so it gets the truth; what it does not get is the size it asked
 *     for, which on a desktop it would not necessarily get either.
 *   - **`glfwSetInputMode(GLFW_CURSOR, ...)` is remembered and otherwise does nothing.** There
 *     is no cursor to hide. Craft uses the mode to decide whether mouse motion is look-around or
 *     menu pointing, and reads it back with `glfwGetInputMode`, so remembering it is the whole
 *     of what the game needs.
 *   - **The clipboard is empty.** `glfwGetClipboardString` returns NULL, which Craft already
 *     handles - it is the paste key in the chat line.
 *   - **`glfwSwapInterval` is a no-op.** The display's flip is what it is.
 */
#ifndef OOPS_CRAFT_GLFW3_H
#define OOPS_CRAFT_GLFW3_H

#ifdef __cplusplus
extern "C" {
#endif

/* Craft includes <GL/glew.h> before this and expects GL to be declared by then; GLFW's own
 * header does the same include dance. Ours just makes sure GL is there either way. */
#include <GL/gl.h>

/* ---------------------------------------------------------------------------
 * Types. `GLFWwindow` is opaque to Craft - it only ever passes the pointer back.
 * ------------------------------------------------------------------------- */
typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;

typedef struct GLFWvidmode {
    int width;
    int height;
    int redBits;
    int greenBits;
    int blueBits;
    int refreshRate;
} GLFWvidmode;

typedef void (*GLFWkeyfun)(GLFWwindow *, int, int, int, int);
typedef void (*GLFWcharfun)(GLFWwindow *, unsigned int);
typedef void (*GLFWmousebuttonfun)(GLFWwindow *, int, int, int);
typedef void (*GLFWscrollfun)(GLFWwindow *, double, double);

/* ---------------------------------------------------------------------------
 * Constants. GLFW's own values, so that a Craft source comparing against them
 * means what it says - these are not ours to choose.
 * ------------------------------------------------------------------------- */
#define GLFW_RELEASE 0
#define GLFW_PRESS   1

#define GLFW_MOD_SHIFT   0x0001
#define GLFW_MOD_CONTROL 0x0002
#define GLFW_MOD_ALT     0x0004
#define GLFW_MOD_SUPER   0x0008

#define GLFW_MOUSE_BUTTON_LEFT   0
#define GLFW_MOUSE_BUTTON_RIGHT  1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

#define GLFW_CURSOR          0x00033001
#define GLFW_CURSOR_NORMAL   0x00034001
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_KEY_SPACE      32
#define GLFW_KEY_ESCAPE     256
#define GLFW_KEY_ENTER      257
#define GLFW_KEY_TAB        258
#define GLFW_KEY_BACKSPACE  259
#define GLFW_KEY_RIGHT      262
#define GLFW_KEY_LEFT       263
#define GLFW_KEY_DOWN       264
#define GLFW_KEY_UP         265
#define GLFW_KEY_LEFT_SHIFT 340

/* ---------------------------------------------------------------------------
 * The twenty-three entry points Craft calls
 * ------------------------------------------------------------------------- */
int  glfwInit(void);
void glfwTerminate(void);

GLFWwindow *glfwCreateWindow(int width, int height, const char *title,
                             GLFWmonitor *monitor, GLFWwindow *share);
void glfwMakeContextCurrent(GLFWwindow *window);
void glfwSwapBuffers(GLFWwindow *window);
void glfwSwapInterval(int interval);
int  glfwWindowShouldClose(GLFWwindow *window);
void glfwGetWindowSize(GLFWwindow *window, int *width, int *height);
void glfwGetFramebufferSize(GLFWwindow *window, int *width, int *height);

GLFWmonitor *glfwGetPrimaryMonitor(void);
const GLFWvidmode *glfwGetVideoModes(GLFWmonitor *monitor, int *count);

void glfwPollEvents(void);
int  glfwGetKey(GLFWwindow *window, int key);
void glfwGetCursorPos(GLFWwindow *window, double *xpos, double *ypos);
void glfwSetInputMode(GLFWwindow *window, int mode, int value);
int  glfwGetInputMode(GLFWwindow *window, int mode);
const char *glfwGetClipboardString(GLFWwindow *window);

GLFWkeyfun         glfwSetKeyCallback(GLFWwindow *window, GLFWkeyfun cbfun);
GLFWcharfun        glfwSetCharCallback(GLFWwindow *window, GLFWcharfun cbfun);
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow *window, GLFWmousebuttonfun cbfun);
GLFWscrollfun      glfwSetScrollCallback(GLFWwindow *window, GLFWscrollfun cbfun);

double glfwGetTime(void);
void   glfwSetTime(double time);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_GLFW3_H */
