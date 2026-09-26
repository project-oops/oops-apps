#include "net-tool.h"

#include "app_ui.h"
#include "oops/draw.h"
#include "oops/freestd.h"

#define BG 0xFF0D1116u
#define ACCENT OOPS_COLOR_CYAN
#define LABEL OOPS_COLOR_GRAY

/* "<ms> ms", or "-" when there is no measurement. */
static void format_latency(int ms, char *out, size_t out_sz) {
    if (out_sz < 16)
        return;
    if (ms < 0)
        oops_snprintf(out, out_sz, "-");
    else
        oops_snprintf(out, out_sz, "%d ms", ms);
}

int net_tool_render(oops_surface_t *surf, const oops_net_info_t *info, int latency_ms) {
    if (!surf || !info) {
        return 0;
    }

    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS net-tool", ACCENT, 4);
    app_ui_version(surf, -1, 56);

    oops_draw_rect(surf, 48, 96, (int)surf->width - 96, 3, ACCENT);

    int rows = 0;
    int y = 140;

    const char *link = info->link_status == 1 ? "connected" : "down";
    y = app_ui_row(surf, y, "link", link);
    rows++;

    const char *kind = info->device_type == 1   ? "wired"
                       : info->device_type == 2 ? "wireless"
                                                : "unknown";
    y = app_ui_row(surf, y, "device", kind);
    rows++;

    y = app_ui_row(surf, y, "address", info->ip_address[0] ? info->ip_address : "-");
    rows++;
    y = app_ui_row(surf, y, "netmask", info->netmask[0] ? info->netmask : "-");
    rows++;
    y = app_ui_row(surf, y, "gateway",
                   info->default_gateway[0] ? info->default_gateway : "-");
    rows++;
    y = app_ui_row(surf, y, "dns-pri", info->primary_dns[0] ? info->primary_dns : "-");
    rows++;
    y = app_ui_row(surf, y, "dns-sec",
                   info->secondary_dns[0] ? info->secondary_dns : "-");
    rows++;
    y = app_ui_row(surf, y, "mac", info->mac_address[0] ? info->mac_address : "-");
    rows++;

    if (info->device_type == 2 && info->ssid[0]) {
        y = app_ui_row(surf, y, "ssid", info->ssid);
        rows++;
    }

    char lat_str[24];
    format_latency(latency_ms, lat_str, sizeof(lat_str));
    y = app_ui_row(surf, y, "latency", lat_str);
    rows++;

    oops_draw_text(surf, 48, y + 24, "an echo server would listen on 9007", LABEL, 2);

    return rows;
}
