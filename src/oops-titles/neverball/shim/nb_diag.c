/*
 * The bodies behind `nb_diag.h`, which says why they are not in the patch.
 *
 * Textures, draws and GL errors are counted by oops-gl's per-submit census, not here.
 * This reports only what the program alone sees: which font it asked for, and which
 * string it failed to draw.
 */
#include "oops/system.h"

#include "nb_diag.h"

/* Distinct render failures printed before only the count continues. Enough to tell
 * every string failing from one; bounded because it is a per-frame path. */
#define NB_DIAG_RENDER_LINES 4u

static unsigned nb_diag_missing;
static unsigned nb_diag_open_failed;
static unsigned nb_diag_convert_failed;
static unsigned nb_diag_render_failed;

void nb_diag_font_missing(const char *path) {
    nb_diag_missing++;
    oops_log_error("font", "no font file at %s - fs_load found nothing there",
                   path ? path : "(null)");
}

void nb_diag_font_open_failed(const char *path, int size, const char *why) {
    nb_diag_open_failed++;
    /* Not throttled: a load-time path, and each failure is distinct. */
    oops_log_error("font", "TTF_OpenFontRW refused size %d of %s: %s", size,
                   path ? path : "(null)", why ? why : "no reason given");
}

void nb_diag_font_convert_failed(const char *text, const char *why) {
    nb_diag_convert_failed++;
    if (nb_diag_convert_failed <= NB_DIAG_RENDER_LINES) {
        oops_log_warn("font",
                      "SDL_ConvertSurface refused '%s', keeping the unconverted "
                      "surface: %s",
                      text ? text : "(null)", why ? why : "no reason given");
    }
}

void nb_diag_font_render_failed(const char *text, const char *why) {
    nb_diag_render_failed++;
    if (nb_diag_render_failed <= NB_DIAG_RENDER_LINES) {
        oops_log_error("font", "TTF_RenderUTF8_Blended refused '%s': %s",
                       text ? text : "(null)", why ? why : "no reason given");
    } else if (nb_diag_render_failed == NB_DIAG_RENDER_LINES + 1u) {
        oops_log_error("font", "further render failures are counted, not printed");
    }
}

void nb_diag_report(void) {
    if (!nb_diag_missing && !nb_diag_open_failed && !nb_diag_convert_failed &&
        !nb_diag_render_failed) {
        return;
    }
    oops_log_error("font",
                   "totals: %u missing, %u refused to open, %u unconverted, %u not "
                   "rendered",
                   nb_diag_missing, nb_diag_open_failed, nb_diag_convert_failed,
                   nb_diag_render_failed);
}
