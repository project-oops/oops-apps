#ifndef OOPS_APPS_SYSTEM_PANEL_H
#define OOPS_APPS_SYSTEM_PANEL_H

#include "oops/draw.h"
#include "oops/system.h"

/*
 * The system panel: what the machine is, drawn to a surface.
 *
 * Render is separated from where the pixels live so the same drawing runs against a display's
 * framebuffer on hardware and against a plain buffer in the host self-test - the seam the SDK
 * itself is built on, and the reason this is testable without a console.
 */

/* Draw the panel for `info` into `surf`. Returns the number of rows written. */
int system_panel_render(oops_surface_t *surf, const oops_system_info_t *info);

#endif /* OOPS_APPS_SYSTEM_PANEL_H */
