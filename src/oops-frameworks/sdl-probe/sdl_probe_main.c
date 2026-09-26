/*
 * sdl-probe: the smallest app that runs upstream SDL2 on this target.
 *
 * As an app it goes through common/app.mk and its undefined-symbol check, which a
 * payload link needs because it passes --unresolved-symbols=ignore-all.
 *
 * It runs SDL_Init, SDL_CreateWindow with SDL_WINDOW_OPENGL, a GL context, the event
 * pump, SDL_GameControllerOpen with no mapping database, and glClear and swap each
 * frame. The clear colour follows the frame counter, so a stopped loop is visible, and
 * the probe ends by itself.
 */
#include "SDL.h"

/*
 * GL comes from oops-gl directly, not from SDL_opengl.h: the entry points are linked
 * in, which is also why SDL_GL_GetProcAddress returns NULL for everything.
 */
#include "GL/gl.h"

#include "oops/input.h"   /* to ask the SDK directly when SDL reports no pad */
#include "oops/syscall.h" /* payload_args_t, the entry point's argument */
#include "oops/system.h"

#include <stdbool.h>

/*
 * Long enough to work a controller through by hand. Options ends a run sooner; the
 * count is the backstop for a run with nobody at the console.
 */
#define PROBE_FRAMES 3000

#define PROBE_TAG "SDLPB"

int sdl_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int sdl_probe_start(const payload_args_t *args) {
    SDL_Window *window;
    SDL_GLContext context;
    SDL_GameController *pad = NULL;
    int frame;
    int pads;

    (void)args;

    /* Debug level, so the SDK's pad init lines (scePadSetProcessPrivilege, scePadInit
     * and the user id) reach the console log. */
    oops_log_set_level(OOPS_LOG_DEBUG);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_TIMER) != 0) {
        oops_log_info(PROBE_TAG, "SDL_Init failed");
        oops_log_info(PROBE_TAG, "%s", SDL_GetError());
        return 1;
    }
    oops_log_info(PROBE_TAG, "SDL_Init ok");
    oops_log_info(PROBE_TAG, "%s", SDL_GetCurrentVideoDriver());

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window =
        SDL_CreateWindow("sdl-probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        oops_log_info(PROBE_TAG, "SDL_CreateWindow failed");
        oops_log_info(PROBE_TAG, "%s", SDL_GetError());
        SDL_Quit();
        return 2;
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        oops_log_info(PROBE_TAG, "SDL_GL_CreateContext failed");
        oops_log_info(PROBE_TAG, "%s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }
    if (SDL_GL_MakeCurrent(window, context) != 0) {
        oops_log_info(PROBE_TAG, "SDL_GL_MakeCurrent failed");
        oops_log_info(PROBE_TAG, "%s", SDL_GetError());
    }

    /* The pad, through the controller API, which exercises the mapping the driver
     * declares for a title with no mapping database. */
    pads = SDL_NumJoysticks();
    if (pads > 0) {
        oops_log_info(PROBE_TAG, "a pad is present");
        if (SDL_IsGameController(0)) {
            pad = SDL_GameControllerOpen(0);
            oops_log_info(PROBE_TAG, "%s",
                          pad ? "opened as a game controller"
                              : "not opened as a game controller");
            if (pad) {
                int a;
                /* Every axis once, at rest: the event log below fires only when an
                 * axis crosses a deadzone, so a resting value is otherwise never
                 * printed. */
                for (a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
                    oops_kprintf(
                        "SDLPB", "axis %d at rest: %d\n", a,
                        (int)SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)a));
                }
            }
        } else {
            oops_log_info(PROBE_TAG,
                          "present, but not recognised as a game controller");
        }
    } else {
        /* Zero joysticks means no pad or no joystick subsystem. SDL_WasInit answers
         * the second and the SDK's own pad poll the first. */
        oops_pad_state_t st;
        int rc;

        oops_log_info(PROBE_TAG, "no pad");
        oops_log_info(PROBE_TAG, "%s",
                      SDL_WasInit(SDL_INIT_JOYSTICK) ? "joystick subsystem: up"
                                                     : "joystick subsystem: DOWN");
        rc = oops_input_poll(0, &st);
        oops_kprintf("SDLPB", "oops_input_poll rc=%d connected=%d buttons=0x%x\n", rc,
                     rc == 0 ? st.connected : -1, rc == 0 ? st.buttons : 0u);
    }

    for (frame = 0; frame < PROBE_FRAMES; frame++) {
        SDL_Event ev;
        float t = (float)frame / (float)PROBE_FRAMES;
        bool quit = false;

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                quit = true;
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN ||
                       ev.type == SDL_CONTROLLERBUTTONUP) {
                /* Logged, so a run shows events reaching the queue from
                 * PROSPERO_JoystickUpdate through SDL_PrivateJoystickButton. */
                oops_kprintf("SDLPB", "button %d %s\n", (int)ev.cbutton.button,
                             ev.type == SDL_CONTROLLERBUTTONDOWN ? "down" : "up");
                if (ev.type == SDL_CONTROLLERBUTTONDOWN &&
                    ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    /* Options ends the run early without closing the title. */
                    quit = true;
                }
            } else if (ev.type == SDL_CONTROLLERAXISMOTION) {
                /* One line per axis per deadzone crossing, so resting jitter does not
                 * bury the button lines. */
                static int reported[SDL_CONTROLLER_AXIS_MAX];
                int axis = ev.caxis.axis;
                int big = (ev.caxis.value > 16000 || ev.caxis.value < -16000);

                if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX &&
                    big != reported[axis]) {
                    reported[axis] = big;
                    if (big) {
                        oops_kprintf("SDLPB", "axis %d moved to %d\n", axis,
                                     (int)ev.caxis.value);
                    }
                }
            }
        }
        if (quit) {
            oops_log_info(PROBE_TAG, "asked to stop");
            break;
        }

        glClearColor(t, 1.0f - t, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    oops_log_info(PROBE_TAG, "park] work done");

    if (pad) {
        SDL_GameControllerClose(pad);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
