/*
 * net-tool payload entry.
 *
 * Executed by a homebrew ELF loader with payload_args in rdi. Brings up the network stack,
 * asks it what the connection is, opens the display and shows the panel, and holds until
 * circle. The drawing is net-tool.c, shared with the host test; this is the console-only half.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/krw.h"
#include "oops/net.h"
#include "oops/netctl.h"
#include "oops/syscall.h"

#include "net-tool.h"

static void klog(const char *msg) {
    char buf[160];
    const char *prefix = "[NET-TOOL] ";
    int n = 0;
    while (prefix[n] && n < 16) { buf[n] = prefix[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

int net_tool_start(const payload_args_t *args);

int net_tool_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    klog("net-tool payload entry reached");

    oops_net_init();
    oops_net_ctl_init();

    oops_net_info_t info;
    for (size_t i = 0; i < sizeof(info); i++) {
        ((unsigned char *)&info)[i] = 0;
    }
    if (oops_net_ctl_get_info(&info) == 0 && info.ip_address[0]) {
        klog(info.ip_address);
    } else {
        klog("network info unavailable");
    }

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (!disp || !oops_display_is_ready(disp)) {
        klog("display would not open");
        oops_net_ctl_term();
        oops_net_term();
        return -1;
    }
    oops_input_init();

    int udp_sock = oops_socket(OOPS_AF_INET, OOPS_SOCK_DGRAM, OOPS_IPPROTO_UDP);
    if (udp_sock >= 0) {
        oops_set_nonblocking(udp_sock, 1);
        (void)oops_bind(udp_sock, "0.0.0.0", 9090);
    }

    uint32_t last = 0;
    unsigned int net_ticks = 0;
    int running = 1;
    while (running) {
        oops_pad_state_t pad;
        if (oops_input_poll(0, &pad) == 0) {
            if ((pad.buttons & OOPS_BUTTON_CIRCLE) && !(last & OOPS_BUTTON_CIRCLE)) {
                running = 0;
            }
            last = pad.buttons;
        }

        /* Throttle netctl queries to once every 60 frames (~1 Hz) */
        if (++net_ticks >= 60) {
            (void)oops_net_ctl_get_info(&info);
            net_ticks = 0;
        }

        /* Check UDP status responder on port 9090 */
        if (udp_sock >= 0) {
            char rx_buf[64];
            char client_ip[32];
            uint16_t client_port = 0;
            long recvd = oops_recvfrom(udp_sock, rx_buf, sizeof(rx_buf) - 1, OOPS_MSG_DONTWAIT,
                                       client_ip, sizeof(client_ip), &client_port);
            if (recvd > 0) {
                const char reply[] = "OOPS net-tool OK\n";
                (void)oops_sendto(udp_sock, reply, sizeof(reply) - 1, 0, client_ip, client_port);
            }
        }

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels) {
            (void)net_tool_render(&surf, &info);
        }
        oops_display_flip(disp);
    }

    klog("net-tool exiting");
    if (udp_sock >= 0) {
        oops_close(udp_sock);
    }
    oops_input_close();
    oops_display_close(disp);
    oops_net_ctl_term();
    oops_net_term();
    return 0;
}
