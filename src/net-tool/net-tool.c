#include "net-tool.h"

#include "oops/draw.h"

#define BG 0xFF0D1116u
#define ACCENT OOPS_COLOR_CYAN
#define LABEL OOPS_COLOR_GRAY
#define VALUE OOPS_COLOR_WHITE

static int row(oops_surface_t *surf, int y, const char *label, const char *value) {
    oops_draw_text(surf, 48, y, label, LABEL, 3);
    oops_draw_text(surf, 380, y, value, VALUE, 3);
    return y + 44;
}

int net_tool_render(oops_surface_t *surf, const oops_net_info_t *info) {
    if (!surf || !info) {
        return 0;
    }

    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS net-tool", ACCENT, 4);
    oops_draw_rect(surf, 48, 96, (int)surf->width - 96, 3, ACCENT);

    int rows = 0;
    int y = 140;

    const char *link = info->link_status == 1 ? "connected" : "down";
    y = row(surf, y, "link", link);
    rows++;

    const char *kind = info->device_type == 1   ? "wired"
                       : info->device_type == 2 ? "wireless"
                                                : "unknown";
    y = row(surf, y, "device", kind);
    rows++;

    y = row(surf, y, "address", info->ip_address[0] ? info->ip_address : "-");
    rows++;
    y = row(surf, y, "netmask", info->netmask[0] ? info->netmask : "-");
    rows++;
    y = row(surf, y, "gateway", info->default_gateway[0] ? info->default_gateway : "-");
    rows++;
    y = row(surf, y, "mac", info->mac_address[0] ? info->mac_address : "-");
    rows++;

    if (info->device_type == 2 && info->ssid[0]) {
        y = row(surf, y, "ssid", info->ssid);
        rows++;
    }

    oops_draw_text(surf, 48, y + 24, "an echo server would listen on 9007", LABEL, 2);

    return rows;
}
