#ifndef OOPS_APPS_NET_TOOL_H
#define OOPS_APPS_NET_TOOL_H

#include "oops/draw.h"
#include "oops/netctl.h"

/*
 * net-tool: what the network is, and a socket you can talk to.
 *
 * The drawing is a pure function of the net info, so the panel is testable on a host
 * with a made-up info block. The socket server is the console-only half.
 */

/* Draw the network panel for `info` into `surf`. If `latency_ms >= 0`, displays
 * measured round-trip / connect latency, or '-' if negative. Returns the number of rows
 * written. */
int net_tool_render(oops_surface_t *surf, const oops_net_info_t *info, int latency_ms);

#endif /* OOPS_APPS_NET_TOOL_H */
