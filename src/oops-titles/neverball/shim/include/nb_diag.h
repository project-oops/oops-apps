/*
 * Reports for the failures upstream's text path returns from quietly.
 *
 * `share/font.c` and `share/image.c` fail silently in three places. The calls are a
 * patch; the bodies live here in `PAYLOAD_SRCS`, under the repository's warning flags,
 * because upstream's archive compiles with `-w` and an unchecked format string can
 * fault in `vsnprintf`. Each entry point takes strings and an int, never a format, so
 * the patch writes no argument list.
 *
 * `why` is the SDL or SDL_ttf error string, read at the call site.
 */
#ifndef OOPS_NEVERBALL_DIAG_H
#define OOPS_NEVERBALL_DIAG_H

/* `fs_load` found no file at `path`. `font_load` returns 0 for this and for a font that
   opened badly alike. */
void nb_diag_font_missing(const char *path);

/* `TTF_OpenFontRW` refused one of the three sizes. Upstream does not check: `ttf[i]`
   stays null, `font_load` returns 1, and every string at that size renders nothing. */
void nb_diag_font_open_failed(const char *path, int size, const char *why);

/* `SDL_ConvertSurface` refused and upstream keeps the unconverted surface. A real
   fallback, so a warning. */
void nb_diag_font_convert_failed(const char *text, const char *why);

/* `TTF_RenderUTF8_Blended` refused, so the widget draws nothing. Throttled, because
   `hud_update` renders through it every frame (`ball/hud.c:55,189`). */
void nb_diag_font_render_failed(const char *text, const char *why);

/* The totals, from `nb_start` after `main` returns. Silent when nothing failed. */
void nb_diag_report(void);

#endif
