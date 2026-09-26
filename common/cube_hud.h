/*
 * cube_hud.h - the one HUD both cube demos draw.
 *
 * gl1-cube and mesa-cube are the same demo through two renderers, so they show the same
 * dashboard: this is that dashboard, drawn once here and called by both. It is built on
 * the SDK's GPU overlay
 * (`oops/hud.h`), so it works on either renderer - including Mesa, whose scanout buffer
 * the CPU cannot write into.
 *
 * A caller fills the fields it has and calls `oops_cube_hud_draw` once a frame, after
 * its scene and before it presents. Fields a renderer does not have a value for are
 * shown honestly (a status line says what that stack actually measures); nothing here
 * is faked to look uniform.
 */
#ifndef OOPS_CUBE_HUD_H
#define OOPS_CUBE_HUD_H

#include "oops/hud.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct oops_cube_hud {
    /* Identity */
    const char *title;   /* "GL1 CUBE" / "MESA CUBE" */
    const char *backend; /* oops_gfx_backend_name() - "oops-gl" / "oops-mesa" */
    const char *api;     /* "OpenGL 1.x" / "OpenGL 3.3" */
    const char *build;   /* OOPS_APP_VERSION */

    /* Frame */
    unsigned width, height;
    unsigned frame;
    unsigned us_per_frame; /* 0 -> "measuring" rather than a divide-by-zero rate */
    bool paused;

    /* Geometry */
    const char *mesh; /* "cube" / "torus" / "sphere" */
    unsigned tris, verts;

    /* Live pipeline toggles - the interactive state both cubes now carry */
    bool cull, depth, texture, lighting;

    /* The per-stack truth: gl1-cube's GPU-verified/hash line, mesa-cube's
     * scanout/pacing line. */
    const char *status;
    uint32_t status_color; /* ARGB */

    /* The button legend */
    const char *controls;
} oops_cube_hud_t;

/*
 * Draw the dashboard. Saves and restores GL state itself (it calls oops_hud_begin/end),
 * so a caller hands it a live `oops_hud_t` and nothing else changes around it. Safe on
 * a NULL hud (draws nothing) so a title whose overlay failed to create still runs.
 */
void oops_cube_hud_draw(oops_hud_t *hud, const oops_cube_hud_t *info);

#endif /* OOPS_CUBE_HUD_H */
