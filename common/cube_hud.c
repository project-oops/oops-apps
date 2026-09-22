/*
 * cube_hud.c - the shared cube dashboard (see cube_hud.h).
 *
 * Pure presentation: it reads an oops_cube_hud_t and draws it through oops/hud.h. It formats with
 * oops_snprintf (oops/freestd.h), which both a freestanding oops-gl title and a hosted Mesa title
 * link, so one renderer serves both.
 */

#include "cube_hud.h"

#include "oops/freestd.h"

#include <stddef.h>

/* The dashboard palette, ARGB. Literals rather than <oops/draw.h>'s OOPS_COLOR_* so this file
 * depends on nothing but the overlay and the formatter. */
#define HUD_WHITE  0xFFFFFFFFu
#define HUD_BLUE   0xFF9FE0FFu
#define HUD_AMBER  0xFFFFAA33u
#define HUD_CYAN   0xFF33DDFFu
#define HUD_GREY   0xFFAAAAAAu
#define HUD_PANEL  0xC00A0E18u

static const char *onoff(bool b)
{
    return b ? "ON" : "OFF";
}

void oops_cube_hud_draw(oops_hud_t *hud, const oops_cube_hud_t *info)
{
    if (hud == NULL || info == NULL) {
        return;
    }

    char line[128];

    oops_hud_begin(hud);

    /* A dimmed panel behind the text, with a bright top edge. */
    oops_hud_rect(hud, 24, 24, 900, 212, HUD_PANEL);
    oops_hud_rect(hud, 24, 24, 900, 3, HUD_CYAN);

    /* Title, large. */
    oops_hud_text(hud, 40, 38, 3, HUD_WHITE, info->title ? info->title : "CUBE");

    /* Backend, API and build. */
    (void)oops_snprintf(line, sizeof line, "%s  |  %s  |  build %s",
                        info->backend ? info->backend : "?",
                        info->api ? info->api : "?",
                        info->build ? info->build : "dev");
    oops_hud_text(hud, 40, 76, 1, HUD_BLUE, line);

    /* Resolution, frame counter and rate. */
    if (info->us_per_frame != 0u) {
        const unsigned fps = (1000000u + info->us_per_frame / 2u) / info->us_per_frame;
        (void)oops_snprintf(line, sizeof line, "%ux%u   frame %u   %u fps%s",
                            info->width, info->height, info->frame, fps,
                            info->paused ? "   PAUSED" : "");
    } else {
        (void)oops_snprintf(line, sizeof line, "%ux%u   frame %u   measuring%s",
                            info->width, info->height, info->frame,
                            info->paused ? "   PAUSED" : "");
    }
    oops_hud_text(hud, 40, 100, 1, HUD_WHITE, line);

    /* Geometry. */
    (void)oops_snprintf(line, sizeof line, "Mesh: %s   Tris: %u   Verts: %u",
                        info->mesh ? info->mesh : "cube", info->tris, info->verts);
    oops_hud_text(hud, 40, 124, 1, HUD_AMBER, line);

    /* The per-stack status line, in its own colour. */
    if (info->status != NULL) {
        oops_hud_text(hud, 40, 148, 1, info->status_color ? info->status_color : HUD_GREY,
                      info->status);
    }

    /* Live pipeline toggles. */
    (void)oops_snprintf(line, sizeof line, "Cull %s   Light %s   Tex %s   Depth %s",
                        onoff(info->cull), onoff(info->lighting),
                        onoff(info->texture), onoff(info->depth));
    oops_hud_text(hud, 40, 172, 1, HUD_CYAN, line);

    /* Controls. */
    if (info->controls != NULL) {
        oops_hud_text(hud, 40, 196, 1, HUD_GREY, info->controls);
    }

    oops_hud_end(hud);
}
