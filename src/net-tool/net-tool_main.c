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
#include "oops/system.h"
#include "oops/time.h"

#include "net-tool.h"

static void klog(const char *msg) {
    oops_klog("NET-TOOL", msg);
}

static int measure_latency(const oops_net_info_t *info) {
    if (!info) return -1;
    const char *target = NULL;
    if (info->primary_dns[0]) {
        target = info->primary_dns;
    } else if (info->default_gateway[0]) {
        target = info->default_gateway;
    } else {
        return -1;
    }

    uint8_t qpkt[256];
    uint16_t tx_id = 0x4E54; /* 'NT' */
    int qlen = oops_dns_build_query("gateway", tx_id, qpkt, sizeof(qpkt));
    if (qlen <= 0) return -1;

    int sock = oops_socket(OOPS_AF_INET, OOPS_SOCK_DGRAM, OOPS_IPPROTO_UDP);
    if (sock < 0) return -1;
    oops_set_nonblocking(sock, 1);

    uint64_t t0 = oops_time_get_ms();
    long sent = oops_sendto(sock, qpkt, (size_t)qlen, 0, target, 53);
    if (sent != qlen) {
        oops_close(sock);
        return -1;
    }

    uint8_t resp[256];
    int latency = -1;
    while (oops_time_get_ms() - t0 < 200) {
        long r = oops_recv(sock, resp, sizeof(resp), 0);
        if (r > 0) {
            uint64_t t1 = oops_time_get_ms();
            latency = (int)(t1 - t0);
            if (latency < 0) latency = 0;
            break;
        }
        oops_time_sleep_ms(2);
    }
    oops_close(sock);
    return latency;
}

int net_tool_start(const payload_args_t *args);

int net_tool_start(const payload_args_t *args) {
    if (args) {
        sys_call_init(args);
    }
    klog("net-tool payload entry reached");

    oops_time_init();
    oops_net_init();
    oops_net_ctl_init();

    oops_net_info_t info;
    for (size_t i = 0; i < sizeof(info); i++) {
        ((unsigned char *)&info)[i] = 0;
    }
    if (oops_net_ctl_get_info(&info) == 0 && info.ip_address[0]) {
        oops_kprintf("NET-TOOL", "IP: %s | Gateway: %s | DNS: %s\n",
                     info.ip_address, info.default_gateway, info.primary_dns);
        char resolved[32];
        if (oops_net_resolve("localhost", resolved, sizeof(resolved)) == 0) {
            oops_kprintf("NET-TOOL", "DNS test (localhost): %s\n", resolved);
        }
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

    int latency_ms = measure_latency(&info);
    unsigned int latency_ticks = 0;

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

        /* Periodically re-probe latency every 120 frames (~2s) */
        if (++latency_ticks >= 120) {
            latency_ms = measure_latency(&info);
            latency_ticks = 0;
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
            (void)net_tool_render(&surf, &info, latency_ms);
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
