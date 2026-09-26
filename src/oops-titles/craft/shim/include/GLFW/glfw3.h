/*
 * GLFW 3, as much of it as Craft calls, over oops-sdk's display, input and clock.
 *
 * The console has one full-screen display and no window system, so this answers
 * Craft's calls directly instead of porting a GLFW backend. The surface is every
 * `glfw*` and `GLFW_*` token in Craft's own sources. Departures from GLFW:
 * `glfwCreateWindow` returns the display's size, not the requested one;
 * `glfwSetInputMode(GLFW_CURSOR, ...)` is only remembered; the clipboard is empty;
 * `glfwSwapInterval` is a no-op.
 */
#ifndef OOPS_CRAFT_GLFW3_H
#define OOPS_CRAFT_GLFW3_H

#ifdef __cplusplus
extern "C" {
#endif

/* Declares GL whether or not <GL/glew.h> was included first. */
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
 * Constants, with GLFW's own values.
 * ------------------------------------------------------------------------- */
#define GLFW_RELEASE 0
#define GLFW_PRESS 1

#define GLFW_MOD_SHIFT 0x0001
#define GLFW_MOD_CONTROL 0x0002
#define GLFW_MOD_ALT 0x0004
#define GLFW_MOD_SUPER 0x0008

#define GLFW_MOUSE_BUTTON_LEFT 0
#define GLFW_MOUSE_BUTTON_RIGHT 1
#define GLFW_MOUSE_BUTTON_MIDDLE 2

#define GLFW_CURSOR 0x00033001
#define GLFW_CURSOR_NORMAL 0x00034001
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_KEY_SPACE 32
#define GLFW_KEY_ESCAPE 256
#define GLFW_KEY_ENTER 257
#define GLFW_KEY_TAB 258
#define GLFW_KEY_BACKSPACE 259
#define GLFW_KEY_RIGHT 262
#define GLFW_KEY_LEFT 263
#define GLFW_KEY_DOWN 264
#define GLFW_KEY_UP 265
#define GLFW_KEY_LEFT_SHIFT 340

/* ---------------------------------------------------------------------------
 * The entry points Craft calls
 * ------------------------------------------------------------------------- */
int glfwInit(void);
void glfwTerminate(void);

GLFWwindow *glfwCreateWindow(int width, int height, const char *title,
                             GLFWmonitor *monitor, GLFWwindow *share);
void glfwMakeContextCurrent(GLFWwindow *window);
void glfwSwapBuffers(GLFWwindow *window);
void glfwSwapInterval(int interval);
int glfwWindowShouldClose(GLFWwindow *window);
void glfwGetWindowSize(GLFWwindow *window, int *width, int *height);
void glfwGetFramebufferSize(GLFWwindow *window, int *width, int *height);

GLFWmonitor *glfwGetPrimaryMonitor(void);
const GLFWvidmode *glfwGetVideoModes(GLFWmonitor *monitor, int *count);

void glfwPollEvents(void);
int glfwGetKey(GLFWwindow *window, int key);
void glfwGetCursorPos(GLFWwindow *window, double *xpos, double *ypos);
void glfwSetInputMode(GLFWwindow *window, int mode, int value);
int glfwGetInputMode(GLFWwindow *window, int mode);
const char *glfwGetClipboardString(GLFWwindow *window);

GLFWkeyfun glfwSetKeyCallback(GLFWwindow *window, GLFWkeyfun cbfun);
GLFWcharfun glfwSetCharCallback(GLFWwindow *window, GLFWcharfun cbfun);
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow *window,
                                              GLFWmousebuttonfun cbfun);
GLFWscrollfun glfwSetScrollCallback(GLFWwindow *window, GLFWscrollfun cbfun);

double glfwGetTime(void);
void glfwSetTime(double time);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_CRAFT_GLFW3_H */
