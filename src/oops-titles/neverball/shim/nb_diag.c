/*
 * The bodies behind `nb_diag.h`. See that file for why they are here and not in the patch.
 *
 * Nothing in here counts textures, draws or GL errors. `oops-gl` sees every one of those without
 * a call site and reports them itself - `tex-created`, `tex-failed`, `gl-errors`,
 * `draws-textured`, `draws-untextured`, in its per-submit census - so a title that counted them
 * again would be instrumenting its own source to learn something the layer underneath already
 * knows. The only things worth a line here are the ones only this program can see: which font it
 * asked for, and which string it failed to draw.
 */
#include "oops/system.h"

#include "nb_diag.h"

/* **How many distinct render failures are worth printing before the count carries it.** Not one:
 * the first failure is usually the HUD clock and tells you almost nothing, while the fourth is
 * often a menu label that names the screen you are on. Not unbounded either, for the reason in
 * the header - this is a per-frame path. Four lines is enough to see whether it is every string
 * or one of them, which is the question. */
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
    /* Not throttled: three sizes per font and a handful of fonts in a run, and every one of them
       is a distinct thing that failed. */
    oops_log_error("font", "TTF_OpenFontRW refused size %d of %s: %s", size,
                   path ? path : "(null)", why ? why : "no reason given");
}

void nb_diag_font_convert_failed(const char *text, const char *why) {
    nb_diag_convert_failed++;
    if (nb_diag_convert_failed <= NB_DIAG_RENDER_LINES) {
        oops_log_warn("font", "SDL_ConvertSurface refused '%s', keeping the unconverted "
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
        /* Said once, so the log stays readable while the count keeps running. `nb_diag_report`
           has the total. */
        oops_log_error("font", "further render failures are counted, not printed");
    }
}

void nb_diag_report(void) {
    /* **Silent when there is nothing to say.** A summary that prints zeroes on every clean run
       trains a reader to skip the line, and then it is not there when it matters. */
    if (!nb_diag_missing && !nb_diag_open_failed && !nb_diag_convert_failed &&
        !nb_diag_render_failed) {
        return;
    }
    oops_log_error("font", "totals: %u missing, %u refused to open, %u unconverted, %u not "
                           "rendered",
                   nb_diag_missing, nb_diag_open_failed, nb_diag_convert_failed,
                   nb_diag_render_failed);
}
