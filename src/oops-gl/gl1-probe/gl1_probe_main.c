/*
 * gl1-probe: the target payload.
 *
 * Runs the same check suite the host self-test runs, and reports each result to the
 * console log. **Comparing the two outputs is the whole point**: a check that passes on
 * the host software rasteriser and fails here is a bug in the hardware path, which is
 * the one place nothing else in this repository can look.
 *
 * It draws nothing to the screen and exits. A probe that stayed resident would need a
 * stop file and an input loop, and would be one more thing to get wrong; the result is
 * in the log.
 */

#include "gl1_probe.h"

#include <oops/system.h>
#include <oops/syscall.h>
#ifndef OOPS_HOST_BUILD
/* The replay path reads a file, opens a display and calls into GL - see
 * `replay_capture`. */
#include <oops/fs.h>
#include <oops/display.h>
#include <GL/gl.h>
#endif

/* app.mk hands every app a build stamp; the probe reports it because the deploy is
 * otherwise unverifiable from this side. mkmodule normalises the ELF, so two builds can
 * land at the same byte count and an identical log then reads as "the fix did nothing"
 * when the fix was never in the running binary - which is exactly what happened chasing
 * the blend bug on 2026-09-17. The string is greppable in the staged ELF and printed in
 * the log, so both ends can be checked. */
#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

#ifndef OOPS_HOST_BUILD
#define PROBE_TAG "gl1-probe"

/* Builds "  name            pass" without a printf, which a freestanding payload does
 * not have. Padded so a column of results reads as a column. */
static void report(const char *name, int passed) {
    char line[64];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 20; i++)
        line[at++] = name[i];
    while (at < 22)
        line[at++] = ' ';
    const char *verdict = passed ? "pass" : "FAIL";
    for (int i = 0; verdict[i] && at < (int)sizeof(line) - 1; i++)
        line[at++] = verdict[i];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/*
 * The running commentary: each check named as it starts, and again with its verdict
 * when it ends. The name alone is what a hang leaves behind, and on 2026-09-20 a hang
 * was the first thing the first console run of this suite produced - nine frames and
 * then nothing, with every result still sitting in an array that was never printed.
 */
static void trace(const char *name, int verdict) {
    char line[64];
    int at = 0;
    const char *head = (verdict < 0) ? "-> " : "   ";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    for (int i = 0; name[i] && at < 24; i++)
        line[at++] = name[i];
    if (verdict >= 0) {
        while (at < 26)
            line[at++] = ' ';
        const char *v = verdict ? "pass" : "FAIL";
        for (int i = 0; v[i] && at < (int)sizeof(line) - 1; i++)
            line[at++] = v[i];
    }
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/* "   name                saw 0xff204060" - the colour a failing check left at the
 * centre of the probe region, in the byte order px() returns. Only failures reach here.
 */
static void saw(const char *name, uint32_t centre) {
    static const char hex[] = "0123456789abcdef";
    char line[64];
    int at = 0;
    line[at++] = ' ';
    line[at++] = ' ';
    line[at++] = ' ';
    for (int i = 0; name[i] && at < 24; i++)
        line[at++] = name[i];
    while (at < 26)
        line[at++] = ' ';
    const char *head = "saw 0x";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    for (int s = 28; s >= 0; s -= 4)
        line[at++] = hex[(centre >> s) & 0xfu];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}

/* **Three digits, because the suite is allowed a hundred and twenty-eight checks** -
 * the same printer gl2-probe has had since it was written, and for the reason that
 * file's comment names. Two digits was right while the suite was short and became wrong
 * the moment it was not: the 2026-09-23 13:08 run printed `98/01` for ninety-eight of a
 * hundred and one, because `ran % 100` is 1. The count the verdict line carries is the
 * one that gets read at a glance and quoted into a worklog, so it is worth the four
 * characters. `GL1_PROBE_MAX_CASES` is 128 and statically asserted against `g_cases`,
 * so three digits cannot themselves overflow. */
static void report_total(int passed, int ran) {
    char line[64];
    int at = 0;
    const char *head = "gl1-probe: ";
    for (int i = 0; head[i]; i++)
        line[at++] = head[i];
    line[at++] = (char)('0' + (passed / 100) % 10);
    line[at++] = (char)('0' + (passed / 10) % 10);
    line[at++] = (char)('0' + passed % 10);
    line[at++] = '/';
    line[at++] = (char)('0' + (ran / 100) % 10);
    line[at++] = (char)('0' + (ran / 10) % 10);
    line[at++] = (char)('0' + ran % 10);
    const char *tail = " passed on hardware";
    for (int i = 0; tail[i] && at < (int)sizeof(line) - 1; i++)
        line[at++] = tail[i];
    line[at] = '\0';
    oops_log_info(PROBE_TAG, "%s", line);
}
#endif

#ifndef OOPS_HOST_BUILD
/* Reads the capture whole, rewrites its call count if `capture-calls` asks for a
 * shorter replay, makes a context and replays it. The frame is left on the panel and
 * the caller parks.
 *
 * The format's header is six bytes of magic, a zero, a version and a little-endian
 * count of commands - so cutting the replay short is a four-byte edit rather than a
 * parse. */
/* The opaque pass is on unless a file beside the capture turns it off, so one build
 * answers both halves of the question: `capture-noalpha` present means present the
 * frame as the replay left it, absent means force alpha to 1 first. A file rather than
 * a rebuild, because the two runs have to be the same binary for the comparison to mean
 * anything. */
static int getenv_flag_opaque(void) {
    const int fd = oops_fs_open("/app0/capture-noalpha", 0, 0);
    if (fd < 0)
        return 1;
    oops_fs_close(fd);
    return 0;
}

static void replay_capture(int fd) {
    static uint8_t
        buf[40u * 1024u * 1024u]; /* a frame plus its loading is about 34MB */
    size_t got = 0;
    for (;;) {
        const int64_t n = oops_fs_read(fd, buf + got, sizeof(buf) - got);
        if (n <= 0)
            break;
        got += (size_t)n;
        if (got >= sizeof(buf))
            break;
    }
    saw("capture-bytes", (uint32_t)got);
    if (got < 12u) {
        oops_log_info(PROBE_TAG, "gl1-probe: the capture is too short to be one");
        return;
    }

    /* An optional shorter replay, for bisecting: a decimal count in a file beside the
     * capture. */
    const int cfd = oops_fs_open("/data/homebrew/GLPB00001/capture-calls", 0, 0);
    if (cfd >= 0) {
        char txt[24];
        const int64_t n = oops_fs_read(cfd, txt, sizeof(txt) - 1u);
        oops_fs_close(cfd);
        if (n > 0) {
            txt[n] = 0;
            uint32_t want = 0u;
            for (int i = 0; txt[i] >= '0' && txt[i] <= '9'; i++) {
                want = want * 10u + (uint32_t)(txt[i] - '0');
            }
            if (want > 0u) {
                buf[8] = (uint8_t)(want & 0xffu);
                buf[9] = (uint8_t)((want >> 8) & 0xffu);
                buf[10] = (uint8_t)((want >> 16) & 0xffu);
                buf[11] = (uint8_t)((want >> 24) & 0xffu);
                saw("capture-calls", want);
            }
        }
    }

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    if (!disp || !oops_display_is_ready(disp)) {
        oops_log_info(PROBE_TAG, "gl1-probe: no display for the replay");
        return;
    }
    void *ctx = glContextCreate(disp);
    if (!ctx) {
        oops_log_info(PROBE_TAG, "gl1-probe: no context for the replay");
        return;
    }

    const unsigned ran = oops_gl_capture_replay(buf, got);
    saw("capture-replayed", (uint32_t)ran);

    /* **Force the frame opaque before presenting it.**
     *
     * The readback of this frame matches a desktop's byte for byte, and the panel shows
     * it green - two views of the same memory disagreeing. What differs between the
     * surfaces that are right on the panel and the ones that are not is their alpha:
     * the planet, the glyphs and the title are opaque, and the starfield, the floor and
     * the menu panels are drawn with GL_SRC_ALPHA/GL_ONE, which accumulates into alpha
     * as well as colour and leaves it short of 255. `replay/px-c` came back with an
     * alpha of 191 and `px-a` with 255, and those are exactly the two cases.
     *
     * A compositor that honours the scanout buffer's alpha would composite every
     * blended surface against whatever is behind it and leave the opaque ones alone,
     * which is the picture. So this writes 1 into alpha everywhere - colour untouched,
     * through the colour mask - and presents that. If the panel comes good, the frame
     * was always right and only its alpha was being read by something downstream. */
    if (getenv_flag_opaque()) {
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
        glRectf(-1.0f, -1.0f, 1.0f, 1.0f);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        oops_log_info(
            PROBE_TAG,
            "gl1-probe: alpha forced to 1 across the frame before presenting");
    }

    /* **What the replayed frame actually holds**, because the panel shows it for a
     * moment and then the shell takes the display back - and because bisecting this
     * means running it twenty times, which is twenty numbers to read rather than twenty
     * screenshots to catch.
     *
     * Four points across the background, which is the surface that comes back green.
     * Replayed on a desktop the same stream draws them dark; if they are dark here the
     * replay is clean to this many calls, and if they are green the fault is already
     * in. That is the whole of what bisection needs to ask. */
    {
        /* **The display's own width is the row stride**, not the width the frame was
           asked for. Indexing by a hardcoded 1920 and reading without a bound took the
           title down on the first attempt - the same mistake the probe's own `px`
           guards against and this did not. */
        glFinish();
        const unsigned int fw = oops_display_get_width(disp);
        const unsigned int fh = oops_display_get_height(disp);
        const uint32_t *fb = (const uint32_t *)glGetFrameReadback();
        if (!fb)
            fb = oops_display_get_framebuffer(disp);
        if (fb && fw >= 1920u && fh >= 1080u) {
            /* **The whole frame's colour, not four pixels of it.**
             *
             * Four fixed points were chosen before there was a frame to choose them
             * from, and three of them landed in black where every render agrees. What
             * distinguishes the two renders is not a pixel, it is a cast: the correct
             * frame's background is a blue starfield and the broken one is green, over
             * most of the screen. So this averages the three channels across a grid and
             * reports them - a green frame reads with G far above B, and a correct one
             * does not, whatever any individual pixel does. */
            /* **An odd stride, because the artifact has a period of two.** Stepping by
               16 walks one parity of pixel columns for ever, and in a frame striped
               every other column that samples only the dark half - the mean came back
               matching a correct desktop render while the same frame's neighbouring
               pixels were black and full green. An average taken on the beat of the
               thing it is measuring measures nothing. */
            uint64_t sr = 0u, sg = 0u, sb = 0u, n = 0u;
            for (unsigned int y = 7u; y < 1080u; y += 13u) {
                for (unsigned int x = 7u; x < 1920u; x += 13u) {
                    const uint32_t c = fb[(size_t)y * (size_t)fw + (size_t)x];
                    sr += (c >> 16) & 0xffu;
                    sg += (c >> 8) & 0xffu;
                    sb += c & 0xffu;
                    n++;
                }
            }
            if (n == 0u)
                n = 1u;
            saw("replay/mean-rgb",
                (uint32_t)(((sr / n) << 16) | ((sg / n) << 8) | (sb / n)));
            /* The cast in one number: green minus blue, biased by 128 so it prints
               unsigned. Above 128 is green-dominant, which the correct frame is not. */
            {
                const int64_t d = (int64_t)(sg / n) - (int64_t)(sb / n);
                saw("replay/green-over-blue", (uint32_t)(128 + d));
            }
            /* **Eight neighbouring pixels, twice**, because the question is no longer
               whether the two views differ but how. The frame read back here matches a
               desktop's and the frame on the panel is green, so something between them
               is reinterpreting the same bytes - and a permutation, a channel rotation
               and a stride error all look different across a run of adjacent pixels.
               Printed here and read off the photograph at the same coordinates, the two
               runs say which it is. */
            static const struct {
                unsigned int x, y;
                const char *tag;
            } runs[] = {
                {1200u, 400u, "replay/r1-"},
                {300u, 900u, "replay/r2-"},
            };
            for (size_t r = 0; r < sizeof(runs) / sizeof(runs[0]); r++) {
                for (unsigned int k = 0; k < 8u; k++) {
                    char nm[24];
                    int at = 0;
                    for (int c = 0; runs[r].tag[c]; c++)
                        nm[at++] = runs[r].tag[c];
                    nm[at++] = (char)('0' + (int)k);
                    nm[at] = 0;
                    saw(nm, fb[(size_t)runs[r].y * (size_t)fw + (size_t)runs[r].x + k]);
                }
            }
        } else {
            oops_log_info(PROBE_TAG, "gl1-probe: no readable frame after the replay");
        }
    }
    /* **Once, not twice.** Swapping twice does not put the frame in both buffers - it
     * presents the frame and then presents the *other* buffer, which holds whatever was
     * there before. With a previous run's Neverball frame still in it that is what ends
     * up on the panel, and it was: a green title screen photographed off the television
     * and reported here as the replay reproducing the bug, when the replay had drawn
     * the correct frame and then hidden it. The readback said so at the time and was
     * not believed. */
    glSwapBuffers();
}
#endif

int gl1_probe_start(const payload_args_t *args);

__attribute__((visibility("default"))) int gl1_probe_start(const payload_args_t *args) {
#ifndef OOPS_HOST_BUILD
    if (args) {
        sys_call_init(args);
    }
    oops_log_info(
        PROBE_TAG,
        "gl1-probe: running the OpenGL 1.x check suite [build " OOPS_APP_VERSION "]");
    gl1_probe_trace = trace;
    gl1_probe_saw = saw;
#else
    (void)args;
#endif

    /* **The panel is an instrument this suite has never read.** Every check decides by
       reading pixels back, and on hardware they all pass while the port they were
       written for still looks wrong on a television. Keeping the context alive lets the
       card below be painted after the suite, so the log and the screen can be compared
       against the same frame. */
#ifndef OOPS_HOST_BUILD
    gl1_probe_keep_context = 1;
#endif

#ifndef OOPS_HOST_BUILD
    /* **A recorded frame, replayed on the hardware that draws it wrongly.**
     *
     * Nine reproductions of the port's broken surface have been built into the suite
     * below and every one of them passes here - the sampler, the pitch, the combine,
     * both rings, the submission path, state reset, magnified filtering, compositing
     * and lighting. The artifact needs the real call stream, and the capture engine
     * that records one has never been usable because this console has nowhere to write
     * a capture to.
     *
     * Recording on a desktop solves that from the other end. The stream is ordinary
     * bytes, so it arrives the way any other data does - pushed into the title's own
     * directory - and replays here against the same GL the port uses. When it does, the
     * frame on the panel is the port's frame, produced without the port,
     * deterministically, from a file that can be cut short.
     *
     * That last part is the point: `capture-calls` holds a decimal count, and the
     * header's own count is rewritten to it before replaying. A frame that is right at
     * N calls and wrong at M has its fault between them, and twenty runs of bisection
     * name the call exactly - which is what none of the nine checks could do.
     */
    {
        /* **`/app0` first, because that is the title's own directory as the title sees
           it** -
           `/data/homebrew/<id>` is where the file is pushed from outside, and the two
           are the same bytes reached by different names. The first attempt used only
           the outside name and found nothing, so both are tried and the one that opened
           is reported. */
        static const char *const cap_paths[] = {
            "/app0/frame.oglcap",
            "/data/homebrew/GLPB00001/frame.oglcap",
        };
        for (size_t i = 0; i < sizeof(cap_paths) / sizeof(cap_paths[0]); i++) {
            const int fd = oops_fs_open(cap_paths[i], 0 /* O_RDONLY */, 0);
            if (fd < 0) {
                oops_log_info(PROBE_TAG, "gl1-probe: no capture at %s", cap_paths[i]);
                continue;
            }
            oops_log_info(
                PROBE_TAG,
                "gl1-probe: a capture is present; replaying it instead of the suite");
            replay_capture(fd);
            oops_fs_close(fd);
            oops_system_park_until_closed();
        }
    }
#endif

    gl1_probe_result_t results[GL1_PROBE_MAX_CASES];
    const int ran = gl1_probe_run(results, (int)(sizeof(results) / sizeof(results[0])));
    if (ran < 0) {
#ifndef OOPS_HOST_BUILD
        oops_log_info(PROBE_TAG, "gl1-probe: no GL context; nothing measured");
        oops_system_park_until_closed();
#endif
        return -1;
    }

    int passed = 0;
    for (int i = 0; i < ran; i++) {
        if (results[i].passed)
            passed++;
#ifndef OOPS_HOST_BUILD
        report(results[i].name, results[i].passed);
#endif
    }
#ifndef OOPS_HOST_BUILD
    report_total(passed, ran);
    /* **After the sentinel, so a harness watching for it is unaffected**, and before
       the park, so the card is what stays on the panel for as long as the title is up.
       Its own readings follow as the `card` rows - the log says what the frame holds,
       the screen says what arrived. */
    gl1_probe_test_card();
#endif

    /*
     * **The answer arrived, and it is that nothing here may finish.**
     *
     * This used to return `(passed == ran) ? 0 : 1` and take a crash at the end of
     * every run, including the successful ones - `SIGSEGV`, `rip: 0x0`, the return
     * value still in `rax`. The note here said the right exit was a platform question
     * rather than a guess, that it was filed as `REQ-20260917T1450Z-2e71`, and that
     * returning was what every other app did until an answer came. It came, on
     * 2026-09-17 at 16:15Z, and it is architectural:
     *
     *   - `exit`, `_Exit`, `sceKernelExit` and every shell-level kill: **absent**.
     *   - `_exit` is present and raises `SIGSYS`, because a `big-app` container's
     * credentials do not permit FreeBSD syscall 1 - which is why `SYS_exit` "did not
     * help" above.
     *   - returning faults at zero because the dynamic linker gives the entry point no
     * caller frame, which is exactly what the old note observed.
     *
     * Process lifecycle belongs to `SceShellCore`, and the conforming ending is to
     * print the last line and idle while the host closes the app.
     * `oops_system_park_until_closed` carries that, and its comment carries the
     * measurement.
     *
     * # What happened to the exit code
     *
     * Nothing that mattered. The old note wanted it so that "a loader that reports the
     * exit code says something useful without the log" - but there is no loader left to
     * report one, since the process cannot exit to hand one over. And the verdict was
     * never only in the code: `report_total` above prints `gl1-probe: NN/NN passed on
     * hardware`, and the resolution names
     * **that exact line** as the completion sentinel a harness watches for. So the
     * machine readable result already lives in the log, and it is printed before this
     * call, which is the order this ending requires.
     *
     * The host build keeps returning, because a host process has a real lifecycle and a
     * host test that never returns is a hang rather than a result.
     */
#ifndef OOPS_HOST_BUILD
    oops_system_park_until_closed();
#else
    return (passed == ran) ? 0 : 1;
#endif
}
