/*
 * Host self-test for net-tool.
 *
 * Renders the network panel from a made-up info block and checks it drew, and round-trips an
 * address through the SDK's inet helpers - which are pure arithmetic, so they are the same on
 * a host as on the console and worth checking here.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/net.h"
#include "oops/netctl.h"

#include "net-tool.h"

/* draw.c's oops_display_get_surface reaches the display backend this test never opens. */
uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return 0; }

#define W 1280
#define H 720

int main(void) {
    uint32_t *pixels = calloc((size_t)W * H, sizeof(uint32_t));
    if (!pixels) {
        return 1;
    }
    oops_surface_t surf = { .pixels = pixels, .width = W, .height = H, .pitch = W };

    oops_net_info_t info;
    memset(&info, 0, sizeof(info));
    info.link_status = 1;
    info.device_type = 2;
    strcpy(info.ip_address, "192.168.1.211");
    strcpy(info.netmask, "255.255.255.0");
    strcpy(info.default_gateway, "192.168.1.1");
    strcpy(info.mac_address, "01:23:45:67:89:ab");
    strcpy(info.ssid, "home");

    int ok = 1;
    int rows = net_tool_render(&surf, &info);
    if (rows < 6) {
        fprintf(stderr, "net-tool selftest: expected at least 6 rows, got %d\n", rows);
        ok = 0;
    }
    int any = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        if (pixels[i] == 0xFF00FFFFu) { any = 1; break; }
    }
    if (!any) {
        fprintf(stderr, "net-tool selftest: nothing drew\n");
        ok = 0;
    }

    /* Address round-trip through the SDK's inet helpers: text -> u32 -> text. */
    uint32_t packed = 0;
    char back[16];
    if (oops_net_inet_pton("192.168.1.211", &packed) != 0) {
        fprintf(stderr, "net-tool selftest: inet_pton refused a good address\n");
        ok = 0;
    } else if (oops_net_inet_ntop(packed, back, sizeof(back)) != 0 ||
               strcmp(back, "192.168.1.211") != 0) {
        fprintf(stderr, "net-tool selftest: address did not round-trip (got %s)\n", back);
        ok = 0;
    }
    /* And a bad address is refused, not quietly accepted. */
    if (oops_net_inet_pton("999.1.1.1", &packed) == 0) {
        fprintf(stderr, "net-tool selftest: inet_pton accepted a bad address\n");
        ok = 0;
    }

    free(pixels);
    if (ok) {
        printf("net-tool selftest: ok (panel drew; address round-trips through the SDK)\n");
        return 0;
    }
    return 1;
}
