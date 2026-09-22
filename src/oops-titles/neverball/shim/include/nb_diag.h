/*
 * What upstream's text path does not say, said.
 *
 * Three places in `share/font.c` and `share/image.c` fail and return quietly, which is a fair
 * trade on a desktop - the font is installed, so a message nobody will read is noise - and the
 * wrong one in a port, where whether the font arrived at all is the open question.
 *
 * **The reporting lives here and only the calls are a patch.** Two reasons, and the second is
 * the one that decided it:
 *
 *   - A patch is rebased onto every upstream revision and shim code is not, so the smaller the
 *     patch the cheaper the next bump. Four calls and two includes rebase without thinking;
 *     thirty lines of format strings and counters do not.
 *
 *   - **Upstream's archive compiles with `-w`** (`OOPS_NB_CFLAGS` in the Makefile), which is
 *     there so a 2010 C codebase survives this repository's `-Werror -Wconversion`. A format
 *     string written inside that archive is therefore checked by nobody, and on a freestanding
 *     target a `%s` against a non-pointer is not a warning, it is a fault in `vsnprintf`. These
 *     bodies are in `PAYLOAD_SRCS`, under the repository's own flags, so the compiler checks
 *     them. That is also why every entry point below takes strings and an int rather than a
 *     format: the patch cannot get an argument list wrong if it never writes one.
 *
 * The `why` argument is the SDL or SDL_ttf error string, read at the call site while it is still
 * current. Passing it keeps this file free of SDL headers entirely.
 */
#ifndef OOPS_NEVERBALL_DIAG_H
#define OOPS_NEVERBALL_DIAG_H

/* `fs_load` found no file at `path`. `font_load` returns 0 for this and for a font that opened
   badly alike, so without this the caller cannot tell a missing file from a broken one. */
void nb_diag_font_missing(const char *path);

/* `TTF_OpenFontRW` refused one of the three sizes. Upstream never checks: the failure leaves a
   null `ttf[i]`, `font_load` still returns 1, and every later string at that size renders as
   nothing. */
void nb_diag_font_open_failed(const char *path, int size, const char *why);

/* `SDL_ConvertSurface` refused, and upstream carries on with the unconverted surface under a
   comment reading "Pretend everything's just fine". It is a real fallback, so this is a warning
   rather than an error - but a port should know it was taken. */
void nb_diag_font_convert_failed(const char *text, const char *why);

/* `TTF_RenderUTF8_Blended` refused, so `make_image_from_font` returns 0 and the widget draws
   nothing. **This one is throttled**, because it is not a load-time path: `hud_update` rebuilds
   the clock and the frame counter through it every frame (`ball/hud.c:55,189`), so a font that
   cannot render reaches it several times a second for as long as the game runs. */
void nb_diag_font_render_failed(const char *text, const char *why);

/* The totals, once, from `nb_start` after `main` returns - so it costs no patch at all. Silent
   when nothing failed, which is the usual case and should not print a line. */
void nb_diag_report(void);

#endif
