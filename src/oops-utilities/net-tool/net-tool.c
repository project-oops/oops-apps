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

static void format_latency(int ms, char *out, size_t out_sz) {
    if (out_sz < 16)
        return;
    if (ms < 0) {
        out[0] = '-';
        out[1] = '\0';
        return;
    }
    char tmp[12];
    int pos = 0;
    uint32_t val = (uint32_t)ms;
    if (val == 0) {
        tmp[pos++] = '0';
    } else {
        while (val > 0) {
            tmp[pos++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    size_t k = 0;
    while (pos > 0 && k < out_sz - 4) {
        out[k++] = tmp[--pos];
    }
    out[k++] = ' ';
    out[k++] = 'm';
    out[k++] = 's';
    out[k] = '\0';
}

int net_tool_render(oops_surface_t *surf, const oops_net_info_t *info, int latency_ms) {
    if (!surf || !info) {
        return 0;
    }

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS net-tool", ACCENT, 4);

    /* UI version placeholder (top right) */
    char ver_buf[64];
    int vi = 0;
    ver_buf[vi++] = 'v';
    ver_buf[vi++] = ' ';
    const char *vp = OOPS_APP_VERSION;
    while (*vp && vi < (int)sizeof(ver_buf) - 1) {
        ver_buf[vi++] = *vp++;
    }
    ver_buf[vi] = '\0';
    int ver_w = vi * 8 * 2;
    int ver_x = (int)surf->width - 48 - ver_w;
    oops_draw_text(surf, ver_x, 56, ver_buf, LABEL, 2);

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
    y = row(surf, y, "dns-pri", info->primary_dns[0] ? info->primary_dns : "-");
    rows++;
    y = row(surf, y, "dns-sec", info->secondary_dns[0] ? info->secondary_dns : "-");
    rows++;
    y = row(surf, y, "mac", info->mac_address[0] ? info->mac_address : "-");
    rows++;

    if (info->device_type == 2 && info->ssid[0]) {
        y = row(surf, y, "ssid", info->ssid);
        rows++;
    }

    char lat_str[24];
    format_latency(latency_ms, lat_str, sizeof(lat_str));
    y = row(surf, y, "latency", lat_str);
    rows++;

    oops_draw_text(surf, 48, y + 24, "an echo server would listen on 9007", LABEL, 2);

    return rows;
}
