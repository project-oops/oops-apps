/*
 * gl1-probe: the target payload.
 *
 * Runs the check suite the host self-test runs and reports each result to the console
 * log. A check that passes on the host software rasteriser and fails here is a fault in
 * the hardware path. The result is in the log; the payload has no input loop.
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

/* app.mk hands every app a build stamp. mkmodule normalises the ELF, so two builds can
 * have the same byte count; the stamp is greppable in the staged ELF and printed in the
 * log, which proves which build ran. */
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

/* Names each check as it starts and again with its verdict when it ends, so a hang
 * leaves the name of the check that hung in the log. */
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

/* The verdict line, which a harness watches for as the completion sentinel. Three
 * digits each side: `GL1_PROBE_MAX_CASES` is statically asserted against `g_cases` and
 * fits in three. */
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
/* Returns 1 unless `capture-noalpha` exists beside the capture: the replayed frame has
 * its alpha forced to 1 before presenting. A file rather than a build flag, so both
 * variants run from the same binary. */
static int getenv_flag_opaque(void) {
    const int fd = oops_fs_open("/app0/capture-noalpha", 0, 0);
    if (fd < 0)
        return 1;
    oops_fs_close(fd);
    return 0;
}

/* Reads the capture whole, rewrites its call count if `capture-calls` asks for a
 * shorter replay, makes a context and replays it. The frame is left on the panel and
 * the caller parks. The header is six bytes of magic, a zero, a version and a
 * little-endian command count, so a shorter replay is a four-byte edit. */
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

    /* Writes 1 into alpha everywhere, colour untouched through the colour mask.
     * GL_SRC_ALPHA/GL_ONE blending leaves destination alpha short of 255, and a
     * compositor that honours scanout alpha shows those surfaces against what is
     * behind them. */
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

    /* Logs what the replayed frame holds, so a bisection run reads numbers from the
     * log rather than the panel. */
    {
        /* The display's own width is the row stride, not the requested width. */
        glFinish();
        const unsigned int fw = oops_display_get_width(disp);
        const unsigned int fh = oops_display_get_height(disp);
        const uint32_t *fb = (const uint32_t *)glGetFrameReadback();
        if (!fb)
            fb = oops_display_get_framebuffer(disp);
        if (fb && fw >= 1920u && fh >= 1080u) {
            /* The mean colour across a grid: a colour cast shows as a channel far
               above the others whatever any single pixel does. The stride is odd so
               the grid is not in step with an artifact of period two. */
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
            /* Green minus blue, biased by 128 so it prints unsigned. Above 128 is
               green-dominant. */
            {
                const int64_t d = (int64_t)(sg / n) - (int64_t)(sb / n);
                saw("replay/green-over-blue", (uint32_t)(128 + d));
            }
            /* Two runs of eight adjacent pixels: a channel permutation, a rotation and
               a stride error each look different across a run, so comparing these with
               the panel at the same coordinates tells them apart. */
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
    /* One swap: a second would present the other buffer, which holds whatever an
     * earlier run left in it. */
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

    /* Every check decides by reading pixels back. Keeping the context alive lets the
       test card be painted after the suite, so the log and the panel can be compared
       against the same frame. */
#ifndef OOPS_HOST_BUILD
    gl1_probe_keep_context = 1;
#endif

#ifndef OOPS_HOST_BUILD
    /* A capture recorded on a desktop and pushed beside the title replays here
     * instead of the suite, against the same GL a port uses. `capture-calls` cuts the
     * replay to a decimal count of calls, so a fault can be bisected to one call. */
    {
        /* `/app0` is the title's own directory as the title sees it;
           `/data/homebrew/<id>` is the same directory by its outside name. Both are
           tried and the one that opens is reported. */
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
    /* After the sentinel, so a harness watching for it is unaffected, and before the
       park, so the card stays on the panel while the title is up. Its readings follow
       as the `card` rows. */
    gl1_probe_test_card();
#endif

    /* A big-app payload cannot end itself: returning faults at zero because the entry
     * point has no caller frame, and `_exit` raises SIGSYS. Process lifecycle belongs
     * to SceShellCore, so the payload parks after the verdict line and the host closes
     * it. The host build returns, since a host test that never returns is a hang. */
#ifndef OOPS_HOST_BUILD
    oops_system_park_until_closed();
#else
    return (passed == ran) ? 0 : 1;
#endif
}
