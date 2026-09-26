/*
 * porthole-companion - on-console utility to monitor, control and stage Porthole.
 *
 * Pure model & rendering logic (shared between host tests and console title).
 */

#include "porthole_companion.h"
#include "oops/freestd.h"

static void append_str(char *dst, const char *src, size_t max_sz) {
    size_t dlen = obs_strlen(dst);
    size_t slen = obs_strlen(src);
    if (dlen + slen >= max_sz) {
        slen = (max_sz > dlen + 1) ? (max_sz - dlen - 1) : 0;
    }
    for (size_t i = 0; i < slen; i++) {
        dst[dlen + i] = src[i];
    }
    dst[dlen + slen] = '\0';
}

/* Pure state operations */
void porthole_companion_init(porthole_companion_state_t *st) {
    if (!st) return;
    for (size_t i = 0; i < sizeof(*st); i++) {
        ((uint8_t *)st)[i] = 0;
    }
    st->status = PORTHOLE_STATUS_UNKNOWN;
    obs_strncpy(st->ip_address, "127.0.0.1", sizeof(st->ip_address) - 1);
    st->video_port = PORTHOLE_DEFAULT_VIDEO_PORT;
    st->input_port = PORTHOLE_DEFAULT_INPUT_PORT;
    st->selected_item = MENU_ACTION_LAUNCH_RESTART;
    st->elfldr_running = 0;
    st->shsrv_running = 0;
    st->status_color = 0xFF94A3B8u;
    obs_strncpy(st->status_message, "Ready. Press [X] to launch or [Triangle] to download.",
                sizeof(st->status_message) - 1);
}

void porthole_companion_set_ip(porthole_companion_state_t *st, const char *ip) {
    if (!st || !ip) return;
    obs_strncpy(st->ip_address, ip, sizeof(st->ip_address) - 1);
    st->ip_address[sizeof(st->ip_address) - 1] = '\0';
}

void porthole_companion_set_status(porthole_companion_state_t *st,
                                  porthole_daemon_status_t status,
                                  const char *msg) {
    if (!st) return;
    st->status = status;
    switch (status) {
        case PORTHOLE_STATUS_RUNNING:
            st->status_color = 0xFF22C55Eu; /* Green */
            break;
        case PORTHOLE_STATUS_STOPPED:
            st->status_color = 0xFFEF4444u; /* Red */
            break;
        case PORTHOLE_STATUS_PROBING:
            st->status_color = 0xFFF59E0Bu; /* Amber */
            break;
        case PORTHOLE_STATUS_ERROR:
            st->status_color = 0xFFDC2626u; /* Dark Red */
            break;
        case PORTHOLE_STATUS_UNKNOWN:
        default:
            st->status_color = 0xFF94A3B8u; /* Slate Gray */
            break;
    }
    if (msg) {
        obs_strncpy(st->status_message, msg, sizeof(st->status_message) - 1);
        st->status_message[sizeof(st->status_message) - 1] = '\0';
    }
}

void porthole_companion_nav_up(porthole_companion_state_t *st) {
    if (!st) return;
    if (st->selected_item == 0) {
        st->selected_item = (porthole_menu_item_t)(MENU_ACTION_COUNT - 1);
    } else {
        st->selected_item = (porthole_menu_item_t)(st->selected_item - 1);
    }
}

void porthole_companion_nav_down(porthole_companion_state_t *st) {
    if (!st) return;
    if (st->selected_item >= (porthole_menu_item_t)(MENU_ACTION_COUNT - 1)) {
        st->selected_item = (porthole_menu_item_t)0;
    } else {
        st->selected_item = (porthole_menu_item_t)(st->selected_item + 1);
    }
}

/*
 * Pure UI Rendering
 */
void porthole_companion_render(const porthole_companion_state_t *st, oops_surface_t *surf) {
    if (!st || !surf || !surf->pixels) return;

    /* Background: Deep Slate Navy */
    oops_draw_clear(surf, 0xFF0A0F1Du);

    int w = (int)surf->width;
    int h = (int)surf->height;

    /* Header Bar */
    oops_draw_rect(surf, 0, 0, w, 100, 0xFF0F172Au);
    oops_draw_rect(surf, 0, 98, w, 2, 0xFF38BDF8u);
    oops_draw_text(surf, 60, 25, "PORTHOLE REMOTE PLAY", 0xFFFFFFFFu, 3);
    oops_draw_text(surf, 60, 68, "Low-Latency Direct Console Streaming & Remote Pad Hub", 0xFF94A3B8u, 1);
    oops_draw_text(surf, w - 340, 40, "TARGET: PS5", 0xFF38BDF8u, 2);

    /* Left Panel: Status & Connection Details */
    int panel_w = 880;
    int panel_h = 750;
    int left_x = 60;
    int panel_y = 130;

    oops_draw_rect_blend(surf, left_x, panel_y, panel_w, panel_h, 0xCC1E293Bu);
    oops_draw_rect(surf, left_x, panel_y, panel_w, 1, 0xFF334155u);
    oops_draw_rect(surf, left_x, panel_y + panel_h - 1, panel_w, 1, 0xFF334155u);
    oops_draw_rect(surf, left_x, panel_y, 1, panel_h, 0xFF334155u);
    oops_draw_rect(surf, left_x + panel_w - 1, panel_y, 1, panel_h, 0xFF334155u);

    oops_draw_text(surf, left_x + 30, panel_y + 30, "DAEMON & NETWORK STATUS", 0xFF38BDF8u, 2);

    /* Status Badge */
    int badge_x = left_x + 30;
    int badge_y = panel_y + 80;
    int badge_w = 260;
    int badge_h = 44;

    uint32_t badge_bg = 0xFF475569u;
    uint32_t badge_border = 0xFF64748Bu;
    const char *badge_text = "UNKNOWN";

    if (st->status == PORTHOLE_STATUS_RUNNING) {
        badge_bg = 0xFF15803Du;
        badge_border = 0xFF22C55Eu;
        badge_text = "RUNNING";
    } else if (st->status == PORTHOLE_STATUS_STOPPED) {
        badge_bg = 0xFFB91C1Cu;
        badge_border = 0xFFEF4444u;
        badge_text = "STOPPED";
    } else if (st->status == PORTHOLE_STATUS_PROBING) {
        badge_bg = 0xFFB45309u;
        badge_border = 0xFFF59E0Bu;
        badge_text = "PROBING...";
    } else if (st->status == PORTHOLE_STATUS_ERROR) {
        badge_bg = 0xFF7F1D1Du;
        badge_border = 0xFFDC2626u;
        badge_text = "PORT ERROR";
    }

    oops_draw_rect(surf, badge_x, badge_y, badge_w, badge_h, badge_bg);
    oops_draw_rect(surf, badge_x, badge_y, badge_w, 1, badge_border);
    oops_draw_rect(surf, badge_x, badge_y + badge_h - 1, badge_w, 1, badge_border);
    oops_draw_rect(surf, badge_x, badge_y, 1, badge_h, badge_border);
    oops_draw_rect(surf, badge_x + badge_w - 1, badge_y, 1, badge_h, badge_border);
    oops_draw_text(surf, badge_x + 30, badge_y + 12, badge_text, 0xFFFFFFFFu, 2);

    /* Status Rows */
    int row_y = panel_y + 150;
    int line_spacing = 54;

    oops_draw_text(surf, left_x + 30, row_y, "Console IPv4:", 0xFF94A3B8u, 2);
    oops_draw_text(surf, left_x + 300, row_y, st->ip_address, 0xFFFFFFFFu, 2);

    row_y += line_spacing;
    oops_draw_text(surf, left_x + 30, row_y, "Video Stream:", 0xFF94A3B8u, 2);
    oops_draw_text(surf, left_x + 300, row_y, "Port 9805 (TCP Annex-B H.264)", 0xFFFFFFFFu, 2);

    row_y += line_spacing;
    oops_draw_text(surf, left_x + 30, row_y, "Input / Control:", 0xFF94A3B8u, 2);
    oops_draw_text(surf, left_x + 300, row_y, "Port 9806 (PPAD & PCTL)", 0xFFFFFFFFu, 2);

    row_y += line_spacing;
    oops_draw_text(surf, left_x + 30, row_y, "Daemon Loader:", 0xFF94A3B8u, 2);
    if (st->elfldr_running) {
        oops_draw_text(surf, left_x + 300, row_y, "Port 9021 (elfldr - READY)", 0xFF22C55Eu, 2);
    } else if (st->shsrv_running) {
        oops_draw_text(surf, left_x + 300, row_y, "Port 2323 (shsrv - READY)", 0xFF22C55Eu, 2);
    } else {
        oops_draw_text(surf, left_x + 300, row_y, "STOPPED (needs :9021 or :2323)", 0xFFEF4444u, 2);
    }

    row_y += line_spacing;
    oops_draw_text(surf, left_x + 30, row_y, "Local Payload:", 0xFF94A3B8u, 2);
    if (st->has_data_payload) {
        oops_draw_text(surf, left_x + 300, row_y, "STAGED (/data/pldmgr/payloads)", 0xFF22C55Eu, 2);
    } else if (st->has_bundled_payload) {
        oops_draw_text(surf, left_x + 300, row_y, "BUNDLED IN APPLICATION", 0xFF38BDF8u, 2);
    } else {
        oops_draw_text(surf, left_x + 300, row_y, "NOT FOUND (Download needed)", 0xFFF59E0Bu, 2);
    }

    /* Notification message box */
    int notif_y = panel_y + 450;
    oops_draw_rect(surf, left_x + 30, notif_y, panel_w - 60, 80, 0xFF0F172Au);
    oops_draw_rect(surf, left_x + 30, notif_y, panel_w - 60, 1, 0xFF334155u);
    oops_draw_rect(surf, left_x + 30, notif_y + 79, panel_w - 60, 1, 0xFF334155u);
    oops_draw_text(surf, left_x + 45, notif_y + 15, "SYSTEM STATUS:", 0xFF38BDF8u, 1);
    oops_draw_text(surf, left_x + 45, notif_y + 40, st->status_message, st->status_color, 1);

    /* Download progress bar if active */
    if (st->is_downloading) {
        int dl_bar_y = panel_y + 560;
        int dl_bar_w = panel_w - 60;
        oops_draw_text(surf, left_x + 30, dl_bar_y, "DOWNLOADING FROM GITHUB RELEASES...", 0xFF38BDF8u, 1);
        oops_draw_rect(surf, left_x + 30, dl_bar_y + 25, dl_bar_w, 28, 0xFF0F172Au);
        oops_draw_rect(surf, left_x + 30, dl_bar_y + 25, dl_bar_w, 1, 0xFF334155u);
        int fill_w = (dl_bar_w * st->dl_pct) / 100;
        if (fill_w > dl_bar_w) fill_w = dl_bar_w;
        if (fill_w > 0) {
            oops_draw_rect(surf, left_x + 31, dl_bar_y + 26, fill_w - 2, 26, 0xFF38BDF8u);
        }
    }

    /* Right Panel: Action Menu & Client Connection Guide */
    int right_x = 980;
    oops_draw_rect_blend(surf, right_x, panel_y, panel_w, panel_h, 0xCC1E293Bu);
    oops_draw_rect(surf, right_x, panel_y, panel_w, 1, 0xFF334155u);
    oops_draw_rect(surf, right_x, panel_y + panel_h - 1, panel_w, 1, 0xFF334155u);
    oops_draw_rect(surf, right_x, panel_y, 1, panel_h, 0xFF334155u);
    oops_draw_rect(surf, right_x + panel_w - 1, panel_y, 1, panel_h, 0xFF334155u);

    oops_draw_text(surf, right_x + 30, panel_y + 30, "ACTIONS & QUICK CONTROL", 0xFF38BDF8u, 2);

    const char *menu_titles[MENU_ACTION_COUNT] = {
        "[X] Start / Restart Daemon",
        "[Triangle] Download / Update from GitHub",
        "[L1] Send Test Keyframe (Port 9806)",
        "[R1] Refresh Network & Status"
    };

    const char *launch_sub;
    if (st->elfldr_running) {
        launch_sub = "Send porthole.elf payload to elfldr (:9021)";
    } else if (st->shsrv_running) {
        launch_sub = "Launch porthole.elf via root shell (:2323)";
    } else {
        launch_sub = "Needs loader (:9021 or :2323 - both stopped)";
    }

    const char *menu_subs[MENU_ACTION_COUNT] = {
        launch_sub,
        "Fetch latest porthole-payload-prospero.elf via SDK HTTP",
        "Emit PCTL opcode 1 IDR keyframe request packet",
        "Re-probe ports (9021/2323/9805/9806) and network"
    };

    int menu_y = panel_y + 90;
    int item_h = 74;

    for (int i = 0; i < MENU_ACTION_COUNT; i++) {
        int is_sel = (st->selected_item == (porthole_menu_item_t)i);
        if (is_sel) {
            oops_draw_rect_blend(surf, right_x + 20, menu_y, panel_w - 40, item_h, 0xFF334155u);
            oops_draw_rect(surf, right_x + 20, menu_y, 4, item_h, 0xFF38BDF8u);
        }

        uint32_t title_color = is_sel ? 0xFFFFFFFFu : 0xFFCBD5E1u;
        uint32_t sub_color = is_sel ? 0xFF38BDF8u : 0xFF64748Bu;

        oops_draw_text(surf, right_x + 35, menu_y + 14, menu_titles[i], title_color, 2);
        oops_draw_text(surf, right_x + 35, menu_y + 44, menu_subs[i], sub_color, 1);

        menu_y += item_h + 12;
    }

    /* Client Connection Guide box */
    int guide_y = panel_y + 490;
    oops_draw_rect(surf, right_x + 20, guide_y, panel_w - 40, 220, 0xFF0F172Au);
    oops_draw_rect(surf, right_x + 20, guide_y, panel_w - 40, 1, 0xFF334155u);
    oops_draw_rect(surf, right_x + 20, guide_y + 219, panel_w - 40, 1, 0xFF334155u);

    oops_draw_text(surf, right_x + 40, guide_y + 20, "HOW TO CONNECT ON PC / CLIENT:", 0xFF38BDF8u, 1);
    oops_draw_text(surf, right_x + 40, guide_y + 55, "1. PROSPEROUS WATCH: run 'pros watch' for direct video", 0xFFE2E8F0u, 1);
    oops_draw_text(surf, right_x + 40, guide_y + 90, "2. MOONLIGHT: run 'pros moonlight' & pair client with console IP", 0xFFE2E8F0u, 1);
    oops_draw_text(surf, right_x + 40, guide_y + 125, "3. DIRECT LOW-LATENCY MPV:", 0xFFE2E8F0u, 1);

    char mpv_cmd[96];
    obs_strncpy(mpv_cmd, "   mpv --demuxer=h264 --profile=low-latency tcp://", sizeof(mpv_cmd) - 1);
    mpv_cmd[sizeof(mpv_cmd) - 1] = '\0';
    append_str(mpv_cmd, st->ip_address, sizeof(mpv_cmd));
    append_str(mpv_cmd, ":9805", sizeof(mpv_cmd));
    oops_draw_text(surf, right_x + 40, guide_y + 155, mpv_cmd, 0xFFFCD34Du, 1);

    /* Bottom Controller Legend */
    int footer_y = h - 70;
    oops_draw_rect(surf, 0, footer_y, w, 70, 0xFF0F172Au);
    oops_draw_rect(surf, 0, footer_y, w, 1, 0xFF334155u);
    oops_draw_text(surf, 60, footer_y + 24,
                   "[D-PAD Up/Down] Select   [Cross] Execute   [Triangle] Download   [Circle] Exit",
                   0xFF94A3B8u, 2);
}
