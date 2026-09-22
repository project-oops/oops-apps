/*
 * sdl-probe: the smallest thing that proves upstream SDL2 works on this target.
 *
 * It is here rather than beside `src/oops-deps/sdl2/` because that directory holds a
 * dependency, not an app, and this is an app: it goes through `common/app.mk` like every other
 * payload and gets the undefined-symbol check that a dependency on its own never runs.
 *
 * **That check is the point of this probe.** `oops-sdl.mk` can be right about every source file
 * and still be wrong, because a payload link passes `--unresolved-symbols=ignore-all` - so a
 * call into a function nobody defines links cleanly and faults on the console. Building SDL
 * inside a real app is the only place that gets caught before hardware.
 *
 * What it exercises, in the order SDL does it:
 *
 *   SDL_Init          video, joystick, audio and timers, all four backends at once
 *   SDL_CreateWindow  with SDL_WINDOW_OPENGL, so the display opens
 *   SDL_GL_CreateContext, MakeCurrent
 *   SDL_PollEvent     the pump, once a frame
 *   SDL_NumJoysticks, SDL_GameControllerOpen - the declared mapping, with no mapping database
 *   glClear, SDL_GL_SwapWindow
 *
 * It draws a colour that changes with the frame counter, so a still frame and a stopped loop
 * look different on a screen, and it ends by itself rather than waiting to be closed - a probe
 * that has to be killed is one that costs a console run to find out about.
 */
#include "SDL.h"

/*
 * GL comes from oops-gl directly, not from `SDL_opengl.h`. SDL's header is a copy of the Khronos
 * headers for platforms whose GL arrives through a loader; here `<GL/gl.h>` is the real one and
 * the entry points are linked in, which is also why `SDL_GL_GetProcAddress` honestly returns
 * NULL for everything.
 */
#include "GL/gl.h"

#include "oops/input.h"   /* to ask the SDK directly when SDL reports no pad */
#include "oops/syscall.h" /* payload_args_t, the entry point's argument */
#include "oops/system.h"

#include <stdbool.h>

/*
 * Long enough to work a controller through by hand. At the measured 9 to 12 ms a flip this is
 * around half a minute, and Options ends it sooner - a fixed count is the backstop for a run
 * with nobody at the console, not the way a run is meant to end.
 */
#define PROBE_FRAMES 3000

static void probe_log(const char *msg)
{
    oops_klog("SDLPB", msg);
}

int sdl_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int sdl_probe_start(const payload_args_t *args)
{
    SDL_Window *window;
    SDL_GLContext context;
    SDL_GameController *pad = NULL;
    int frame;
    int pads;

    (void)args;

    /*
     * Debug level, so the SDK's own `oops_log_debug` lines reach the console log. The pad's init
     * reports `scePadSetProcessPrivilege`, `scePadInit` and the user id that way, and the first
     * hardware run of this probe printed "no pad" with no way to tell whether that meant no pad
     * was connected or the backend never asked. A probe that cannot distinguish its two failure
     * modes costs a console run to find out; this one line is cheaper.
     */
    oops_log_set_level(OOPS_LOG_DEBUG);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER |
                 SDL_INIT_TIMER) != 0) {
        probe_log("SDL_Init failed");
        probe_log(SDL_GetError());
        return 1;
    }
    probe_log("SDL_Init ok");
    probe_log(SDL_GetCurrentVideoDriver());

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow("sdl-probe", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (!window) {
        probe_log("SDL_CreateWindow failed");
        probe_log(SDL_GetError());
        SDL_Quit();
        return 2;
    }

    context = SDL_GL_CreateContext(window);
    if (!context) {
        probe_log("SDL_GL_CreateContext failed");
        probe_log(SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 3;
    }
    if (SDL_GL_MakeCurrent(window, context) != 0) {
        probe_log("SDL_GL_MakeCurrent failed");
        probe_log(SDL_GetError());
    }

    /*
     * The pad, through the controller API rather than the raw joystick one - that is what
     * exercises the mapping the driver declares, and a title with no mapping database is
     * exactly the case `GetGamepadMapping` exists for.
     */
    pads = SDL_NumJoysticks();
    if (pads > 0) {
        probe_log("a pad is present");
        if (SDL_IsGameController(0)) {
            pad = SDL_GameControllerOpen(0);
            probe_log(pad ? "opened as a game controller" : "not opened as a game controller");
            if (pad) {
                int a;
                /*
                 * Every axis read once, at rest, before anything is touched. The event log below
                 * only fires when an axis *crosses* a deadzone, so a trigger sitting correctly at
                 * zero produces no line at all - and absence of evidence read as evidence is how
                 * the half-scale trigger bug survived three clean hardware runs. This prints the
                 * resting value whether it is right or wrong.
                 */
                for (a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
                    oops_kprintf("SDLPB", "axis %d at rest: %d\n", a,
                                 (int)SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)a));
                }
            }
        } else {
            probe_log("present, but not recognised as a game controller");
        }
    } else {
        /*
         * Zero joysticks has two causes and they need telling apart: no pad is connected, or the
         * joystick subsystem never started. `SDL_WasInit` answers the second directly, and
         * asking the SDK's own pad poll answers the first - so the log says which rather than
         * leaving the next reader to guess, as the first hardware run did.
         */
        oops_pad_state_t st;
        int rc;

        probe_log("no pad");
        probe_log(SDL_WasInit(SDL_INIT_JOYSTICK) ? "joystick subsystem: up"
                                                 : "joystick subsystem: DOWN");
        rc = oops_input_poll(0, &st);
        oops_kprintf("SDLPB", "oops_input_poll rc=%d connected=%d buttons=0x%x\n",
                     rc, rc == 0 ? st.connected : -1, rc == 0 ? st.buttons : 0u);
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
                /*
                 * Logged, because the point of a probe is to say what arrived. Detection and
                 * `SDL_GameControllerOpen` were confirmed on hardware before this line existed,
                 * and events were not - the probe consumed them silently, so a run with the pad
                 * in hand proved nothing about the path from `PROSPERO_JoystickUpdate` through
                 * `SDL_PrivateJoystickButton` to the queue.
                 */
                oops_kprintf("SDLPB", "button %d %s\n", (int)ev.cbutton.button,
                             ev.type == SDL_CONTROLLERBUTTONDOWN ? "down" : "up");
                if (ev.type == SDL_CONTROLLERBUTTONDOWN &&
                    ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                    /* Options ends it early, so a run can be cut short without closing the
                       title. Logged above first, so the press that stopped it is on the record. */
                    quit = true;
                }
            } else if (ev.type == SDL_CONTROLLERAXISMOTION) {
                /*
                 * Rate-limited to one line per axis per crossing of a deadzone. A stick at rest
                 * still jitters, and 600 frames of that would bury the button lines it is meant
                 * to sit beside.
                 */
                static int reported[SDL_CONTROLLER_AXIS_MAX];
                int axis = ev.caxis.axis;
                int big = (ev.caxis.value > 16000 || ev.caxis.value < -16000);

                if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX && big != reported[axis]) {
                    reported[axis] = big;
                    if (big) {
                        oops_kprintf("SDLPB", "axis %d moved to %d\n", axis, (int)ev.caxis.value);
                    }
                }
            }
        }
        if (quit) {
            probe_log("asked to stop");
            break;
        }

        glClearColor(t, 1.0f - t, 0.25f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    probe_log("park] work done");

    if (pad) {
        SDL_GameControllerClose(pad);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
