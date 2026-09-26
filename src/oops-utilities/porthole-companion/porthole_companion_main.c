/*
 * porthole_companion_main.c - Console hardware entry point for porthole-companion.
 *
 * Runs as a native category-0 Big App (PORT00002) on the console home screen.
 * Talks to oops-sdk display, input, networking, http, and filesystem.
 */

#include "porthole_companion.h"
#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/net.h"
#include "oops/netctl.h"
#include "oops/fs.h"
#include "oops/time.h"
#include "oops/system.h"
#include "oops/freestd.h"

#if defined(__has_include)
#  if __has_include(<oops/http.h>)
#    include <oops/http.h>
#    define PORTHOLE_COMPANION_HAVE_HTTP 1
#  endif
#endif

typedef struct dl_context {
    porthole_companion_state_t *st;
    oops_display_t *disp;
    oops_surface_t *surf;
    int last_pct;
} dl_context_t;

#ifdef PORTHOLE_COMPANION_HAVE_HTTP
static void on_dl_progress(uint64_t downloaded, uint64_t total, void *ud) {
    dl_context_t *ctx = (dl_context_t *)ud;
    ctx->st->dl_bytes_done = downloaded;
    ctx->st->dl_bytes_total = total;
    if (total > 0) {
        ctx->st->dl_pct = (int)((downloaded * 100) / total);
    } else {
        ctx->st->dl_pct = 50;
    }
    if (ctx->st->dl_pct != ctx->last_pct) {
        ctx->last_pct = ctx->st->dl_pct;
        porthole_companion_render(ctx->st, ctx->surf);
        oops_display_flip(ctx->disp);
    }
}
#endif

/* Test connection to a local port */
static int probe_port(uint16_t port) {
    int s = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s < 0) return 0;

    int rc = oops_connect(s, "127.0.0.1", port);
    if (rc == 0) {
        if (port == PORTHOLE_ELFLDR_PORT) {
            /* Send 4 zero bytes so elfldr's socksrv reads magic cleanly without logging EOF */
            uint32_t dummy = 0;
            (void)oops_send(s, &dummy, sizeof(dummy), 0);
        }
        oops_close(s);
        return 1;
    }

    oops_close(s);
    return 0;
}

static void probe_daemon(porthole_companion_state_t *st) {
    st->status = PORTHOLE_STATUS_PROBING;
    int vid_up = probe_port(st->video_port);
    int in_up = probe_port(st->input_port);

    if (vid_up && in_up) {
        st->elfldr_running = 1;
        porthole_companion_set_status(st, PORTHOLE_STATUS_RUNNING,
                                      "Porthole active and streaming on ports 9805 / 9806.");
        return;
    } else if (vid_up || in_up) {
        porthole_companion_set_status(st, PORTHOLE_STATUS_ERROR,
                                      "Port conflict: only one port is open.");
        return;
    }

    /* Daemon stopped; check loader status (probe shsrv only if elfldr is down) */
    st->elfldr_running = probe_port(PORTHOLE_ELFLDR_PORT);
    if (!st->elfldr_running) {
        st->shsrv_running = probe_port(PORTHOLE_SHSRV_PORT);
    } else {
        st->shsrv_running = 0;
    }

    if (st->elfldr_running) {
        porthole_companion_set_status(st, PORTHOLE_STATUS_STOPPED,
                                      "Daemon stopped. elfldr ready on :9021. Press [X] to launch.");
    } else if (st->shsrv_running) {
        porthole_companion_set_status(st, PORTHOLE_STATUS_STOPPED,
                                      "Daemon stopped. shsrv ready on :2323. Press [X] to launch.");
    } else {
        porthole_companion_set_status(st, PORTHOLE_STATUS_STOPPED,
                                      "Stopped. Loader required (elfldr :9021 or shsrv :2323).");
    }
}

static void update_network_info(porthole_companion_state_t *st) {
    oops_net_info_t info;
    for (size_t i = 0; i < sizeof(info); i++) ((uint8_t *)&info)[i] = 0;

    if (oops_net_ctl_get_info(&info) == 0 && info.ip_address[0] != '\0') {
        porthole_companion_set_ip(st, info.ip_address);
    }
}

static void check_local_payloads(porthole_companion_state_t *st) {
    st->has_bundled_payload = oops_fs_exists(PORTHOLE_PAYLOAD_APP0_PATH);
    st->has_data_payload = oops_fs_exists(PORTHOLE_PAYLOAD_DATA_PATH);
}

static void send_keyframe_request(porthole_companion_state_t *st) {
    int s = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s < 0) {
        porthole_companion_set_status(st, st->status, "Could not open socket for keyframe probe.");
        return;
    }

    if (oops_connect(s, "127.0.0.1", st->input_port) != 0) {
        oops_close(s);
        porthole_companion_set_status(st, st->status, "Failed to connect to port 9806.");
        return;
    }

    /* Fixed 24-byte PCTL record (opcode 1 = IDR request) */
    porthole_ctl ctl;
    for (size_t i = 0; i < sizeof(ctl); i++) ((uint8_t *)&ctl)[i] = 0;
    ctl.magic[0] = PORTHOLE_CTL_MAGIC0;
    ctl.magic[1] = PORTHOLE_CTL_MAGIC1;
    ctl.magic[2] = PORTHOLE_CTL_MAGIC2;
    ctl.magic[3] = PORTHOLE_CTL_MAGIC3;
    ctl.version = PORTHOLE_CTL_VERSION;
    ctl.op = (uint8_t)PORTHOLE_CTL_OP_KEYFRAME;

    long sent = oops_send(s, &ctl, sizeof(ctl), 0);
    oops_close(s);

    if (sent == (long)sizeof(ctl)) {
        porthole_companion_set_status(st, st->status, "PCTL Keyframe request dispatched successfully!");
    } else {
        porthole_companion_set_status(st, st->status, "Partial write sending PCTL keyframe.");
    }
}

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

#define PORTHOLE_PLDMGR_PORT 8084

/* Query pldmgr on :8084 to find and kill any running payload.elf before launching */
static void kill_existing_payload_via_pldmgr(void) {
    int s = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s < 0) return;

    if (oops_connect(s, "127.0.0.1", PORTHOLE_PLDMGR_PORT) != 0) {
        oops_close(s);
        return;
    }

    const char *req = "GET /processes_list HTTP/1.1\r\nHost: 127.0.0.1:8084\r\nConnection: close\r\n\r\n";
    (void)oops_send(s, req, obs_strlen(req), 0);

    char buf[4096];
    size_t total = 0;
    while (total < sizeof(buf) - 1) {
        long n = oops_recv(s, buf + total, sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    oops_close(s);

    /* Look for "payload.elf" */
    char *p = obs_strstr(buf, "\"name\":\"payload.elf\"");
    if (p == NULL) {
        p = obs_strstr(buf, "\"payload.elf\"");
    }
    if (p == NULL) return;

    /* Scan backwards to find "pid": */
    while (p > buf && obs_strncmp(p, "\"pid\":", 6) != 0) {
        p--;
    }
    if (obs_strncmp(p, "\"pid\":", 6) != 0) return;
    p += 6;
    while (*p == ' ' || *p == '\t') p++;

    int pid = 0;
    while (*p >= '0' && *p <= '9') {
        pid = pid * 10 + (*p - '0');
        p++;
    }
    if (pid <= 0) return;

    /* Connect again and issue kill to pldmgr */
    int s_kill = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s_kill >= 0) {
        if (oops_connect(s_kill, "127.0.0.1", PORTHOLE_PLDMGR_PORT) == 0) {
            char kill_req[128];
            char pid_str[16];
            pid_str[obs_format_i64(pid_str, (int64_t)pid)] = '\0';
            obs_strncpy(kill_req, "GET /process_kill?pid=", sizeof(kill_req) - 1);
            kill_req[sizeof(kill_req) - 1] = '\0';
            append_str(kill_req, pid_str, sizeof(kill_req));
            append_str(kill_req, " HTTP/1.1\r\nHost: 127.0.0.1:8084\r\nConnection: close\r\n\r\n", sizeof(kill_req));
            (void)oops_send(s_kill, kill_req, obs_strlen(kill_req), 0);
            oops_time_sleep_ms(300);
        }
        oops_close(s_kill);
    }
}

static void launch_payload(porthole_companion_state_t *st) {
    kill_existing_payload_via_pldmgr();
    const char *payload_path = NULL;
    if (st->has_bundled_payload) {
        payload_path = PORTHOLE_PAYLOAD_APP0_PATH;
    } else if (st->has_data_payload) {
        payload_path = PORTHOLE_PAYLOAD_DATA_PATH;
    }

    if (!payload_path) {
        porthole_companion_set_status(st, st->status, "No payload binary found! Press [Triangle] to download first.");
        return;
    }

    /* 1. Try streaming payload directly to elfldr on :9021 */
    int s = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s >= 0) {
        if (oops_connect(s, "127.0.0.1", PORTHOLE_ELFLDR_PORT) == 0) {
            void *payload_data = NULL;
            size_t payload_sz = 0;
            if (oops_fs_read_all(payload_path, &payload_data, &payload_sz) != 0 || !payload_data) {
                oops_close(s);
                porthole_companion_set_status(st, st->status, "Failed reading payload ELF file.");
                return;
            }

            st->elfldr_running = 1;
            porthole_companion_set_status(st, PORTHOLE_STATUS_PROBING,
                                          "Streaming payload to elfldr (:9021)...");
            size_t sent_total = 0;
            const uint8_t *p = (const uint8_t *)payload_data;
            while (sent_total < payload_sz) {
                long n = oops_send(s, p + sent_total, payload_sz - sent_total, 0);
                if (n <= 0) break;
                sent_total += (size_t)n;
            }
            oops_close(s);
            oops_fs_free_data(payload_data);

            if (sent_total == payload_sz) {
                porthole_companion_set_status(st, PORTHOLE_STATUS_PROBING,
                                              "Dispatched to elfldr! Waiting for startup...");
                oops_time_sleep_ms(1000);
                probe_daemon(st);
                return;
            } else {
                porthole_companion_set_status(st, PORTHOLE_STATUS_ERROR,
                                              "Partial write sending payload ELF to elfldr.");
                return;
            }
        }
        oops_close(s);
    }

    /* 2. Try bootstrapping via shsrv on :2323 if elfldr was not listening */
    int s_sh = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, 0);
    if (s_sh >= 0) {
        if (oops_connect(s_sh, "127.0.0.1", PORTHOLE_SHSRV_PORT) == 0) {
            st->shsrv_running = 1;
            porthole_companion_set_status(st, PORTHOLE_STATUS_PROBING,
                                          "Bootstrapping elfldr via root shell (:2323)...");
            const char *boot_cmd = "hbldr /data/pldmgr/payloads/elfldr/elfldr_v0.24.elf\n";
            (void)oops_send(s_sh, boot_cmd, obs_strlen(boot_cmd), 0);
            oops_time_sleep_ms(500);
            oops_close(s_sh);

            oops_time_sleep_ms(500);
            probe_daemon(st);
            return;
        }
        oops_close(s_sh);
    }

    /* 3. Neither loader is available */
    st->elfldr_running = 0;
    st->shsrv_running = 0;
    porthole_companion_set_status(st, PORTHOLE_STATUS_STOPPED,
        "Cannot start: elfldr (:9021) is not running. Please start elfldr.");
}

static void download_payload_from_release(porthole_companion_state_t *st,
                                         oops_display_t *disp,
                                         oops_surface_t *surf) {
#ifndef PORTHOLE_COMPANION_HAVE_HTTP
    (void)disp;
    (void)surf;
    porthole_companion_set_status(st, st->status, "HTTP download client not available in this build.");
#else
    (void)oops_fs_mkdir("/data/pldmgr", 0755);
    (void)oops_fs_mkdir("/data/pldmgr/payloads", 0755);
    (void)oops_fs_mkdir("/data/pldmgr/payloads/porthole", 0755);

    st->is_downloading = 1;
    st->dl_pct = 0;
    st->dl_bytes_done = 0;
    st->dl_bytes_total = 0;
    porthole_companion_set_status(st, st->status, "Fetching porthole.elf from GitHub releases...");

    dl_context_t ctx = { st, disp, surf, -1 };
    int rc = oops_http_get_to_file_cb(PORTHOLE_RELEASE_URL, PORTHOLE_PAYLOAD_DATA_PATH, on_dl_progress, &ctx);

    st->is_downloading = 0;
    if (rc == OOPS_HTTP_OK) {
        st->has_data_payload = 1;
        porthole_companion_set_status(st, st->status, "Download complete! Staged in /data/pldmgr/payloads/porthole/.");
    } else {
        porthole_companion_set_status(st, st->status, "Download failed. Check internet connection.");
    }
#endif
}

/* Main title entry point */
int porthole_companion_start(void);
int porthole_companion_start(void) {
    oops_klog_level(OOPS_LOG_INFO, "PCOMP", "porthole-companion starting up");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    if (!disp) {
        oops_klog_level(OOPS_LOG_ERROR, "PCOMP", "Failed to open display");
        return -1;
    }

    (void)oops_input_init();
    (void)oops_net_init();
    (void)oops_net_ctl_init();

    porthole_companion_state_t state;
    porthole_companion_init(&state);

    update_network_info(&state);
    check_local_payloads(&state);
    probe_daemon(&state);

    uint32_t prev_buttons = 0;
    int running = 1;

    while (running) {
        oops_surface_t surf = oops_display_get_surface(disp);
        state.frame_count++;

        /* Poll pad input */
        oops_pad_state_t pad;
        for (size_t i = 0; i < sizeof(pad); i++) ((uint8_t *)&pad)[i] = 0;

        if (oops_input_poll(0, &pad) == 0 && pad.connected) {
            uint32_t new_pressed = pad.buttons & ~prev_buttons;

            /* Navigation */
            if (new_pressed & OOPS_BUTTON_UP) {
                porthole_companion_nav_up(&state);
            }
            if (new_pressed & OOPS_BUTTON_DOWN) {
                porthole_companion_nav_down(&state);
            }

            /* Quick direct action buttons */
            if (new_pressed & OOPS_BUTTON_TRIANGLE) {
                download_payload_from_release(&state, disp, &surf);
            }
            if (new_pressed & (OOPS_BUTTON_L1 | OOPS_BUTTON_R1)) {
                update_network_info(&state);
                check_local_payloads(&state);
                probe_daemon(&state);
            }
            if (new_pressed & OOPS_BUTTON_CIRCLE) {
                running = 0;
            }

            /* Execute selected menu action on Cross */
            if (new_pressed & OOPS_BUTTON_CROSS) {
                switch (state.selected_item) {
                    case MENU_ACTION_LAUNCH_RESTART:
                        launch_payload(&state);
                        break;
                    case MENU_ACTION_DOWNLOAD_PAYLOAD:
                        download_payload_from_release(&state, disp, &surf);
                        break;
                    case MENU_ACTION_TEST_KEYFRAME:
                        send_keyframe_request(&state);
                        break;
                    case MENU_ACTION_REFRESH_STATUS:
                        update_network_info(&state);
                        check_local_payloads(&state);
                        probe_daemon(&state);
                        break;
                    default:
                        break;
                }
            }

            prev_buttons = pad.buttons;
        }

        /* Periodic background probe every 180 frames (~3s) */
        if ((state.frame_count % 180) == 0 && !state.is_downloading) {
            update_network_info(&state);
            check_local_payloads(&state);
            probe_daemon(&state);
        }

        /* Draw UI */
        porthole_companion_render(&state, &surf);
        oops_display_flip(disp);

        oops_time_sleep_ms(16);
    }

    oops_net_ctl_term();
    oops_net_term();
    oops_display_close(disp);
    oops_klog_level(OOPS_LOG_INFO, "PCOMP", "porthole-companion cleanly exited");
    return 0;
}
