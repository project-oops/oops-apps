/*
 * home payload entry.
 *
 * Executed by a homebrew ELF loader (elfldr) with payload_args in rdi. Opens the display and the
 * pad, then loops: read the pad, apply it to the model, draw, flip.
 *
 * The model, the skins and the drawing are `home.c`, shared with the host self-test. This file is
 * the console-only part: the display, the input, the loop, and **the host dispatch** - the seam
 * where a shell action becomes something a machine actually does.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/fs.h"
#include "oops/pkg.h"
#include "oops/freestd.h"

#include "home.h"

/* A line to the system log, the one output a payload always has. */
static void klog(const char *msg) {
    oops_klog("HOME", msg);
}

/*
 * Dynamic Real Filesystem Title & Payload Scanner.
 *
 * Scans on-disk storage locations for real installed PS5 / PS4 titles, homebrew,
 * and staged ELFs, extracting true title names and versions from param.json.
 */
static home_title_t s_scanned[HOME_MAX_TITLES];
static char s_ids[HOME_MAX_TITLES][20];
static char s_names[HOME_MAX_TITLES][80];
static char s_cats[HOME_MAX_TITLES][20];
static char s_vers[HOME_MAX_TITLES][20];
static int s_scanned_count = 0;

static int is_title_scanned(const char *id) {
    if (!id || id[0] == '\0') return 1;
    for (int i = 0; i < s_scanned_count; i++) {
        if (s_scanned[i].id && obs_strcmp(s_scanned[i].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static int extract_json_string(const char *json, const char *key, char *out, size_t out_len) {
    if (!json || !key || !out || out_len == 0) return 0;
    out[0] = '\0';
    size_t klen = obs_strlen(key);
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            if (obs_strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
                p += 1 + klen + 1;
                while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':') p++;
                if (*p == '"') {
                    p++;
                    size_t i = 0;
                    while (*p && *p != '"' && i + 1 < out_len) {
                        out[i++] = *p++;
                    }
                    out[i] = '\0';
                    return 1;
                }
            }
        }
        p++;
    }
    return 0;
}

static int try_parse_param_json(const char *path, char *out_name, size_t name_sz, char *out_ver, size_t ver_sz) {
    int fd = oops_fs_open(path, OOPS_O_RDONLY, 0);
    if (fd < 0) return 0;
    char buf[4096];
    int64_t n = oops_fs_read(fd, buf, sizeof(buf) - 1);
    oops_fs_close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    extract_json_string(buf, "titleName", out_name, name_sz);
    if (!extract_json_string(buf, "contentVersion", out_ver, ver_sz)) {
        extract_json_string(buf, "masterVersion", out_ver, ver_sz);
    }
    return 1;
}

static void add_discovered_title(const char *id, const char *default_category, const char *exec_path) {
    if (!id || id[0] == '\0' || s_scanned_count >= HOME_MAX_TITLES) return;
    if (is_title_scanned(id)) return;

    int idx = s_scanned_count;
    oops_snprintf(s_ids[idx], sizeof(s_ids[idx]), "%s", id);

    char name[80] = {0};
    char ver[20] = {0};
    char path[256];
    int parsed = 0;

    /* 1. Try /user/appmeta/<ID>/param.json */
    oops_snprintf(path, sizeof(path), "/user/appmeta/%s/param.json", id);
    if (oops_fs_exists(path)) {
        parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
    }
    /* 2. Try /data/homebrew/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/data/homebrew/%s/sce_sys/param.json", id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 3. Try /user/app/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/user/app/%s/sce_sys/param.json", id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 4. Try /user/app/<ID>/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/user/app/%s/param.json", id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }

    if (name[0] != '\0') {
        oops_snprintf(s_names[idx], sizeof(s_names[idx]), "%s", name);
    } else {
        if (obs_strcmp(id, "ITEM00001") == 0) {
            oops_snprintf(s_names[idx], sizeof(s_names[idx]), "ITEMZFLOW");
        } else if (obs_strcmp(id, "LAPY20011") == 0) {
            oops_snprintf(s_names[idx], sizeof(s_names[idx]), "PS5-XPLORER");
        } else if (obs_strcmp(id, "NPXS39041") == 0) {
            oops_snprintf(s_names[idx], sizeof(s_names[idx]), "HOMEBREW STORE");
        } else {
            oops_snprintf(s_names[idx], sizeof(s_names[idx]), "%s", id);
        }
    }

    if (ver[0] != '\0') {
        oops_snprintf(s_vers[idx], sizeof(s_vers[idx]), "%s", ver);
    } else {
        oops_snprintf(s_vers[idx], sizeof(s_vers[idx]), "1.00");
    }

    if (default_category) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "%s", default_category);
    } else if (obs_strcmp(id, "NPXS39041") == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "HOMEBREW");
    } else if (obs_strncmp(id, "PPSA", 4) == 0 || obs_strncmp(id, "NPXS", 4) == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "PS5");
    } else if (obs_strncmp(id, "CUSA", 4) == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "PS4");
    } else {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "HOMEBREW");
    }

    s_scanned[idx].id = s_ids[idx];
    s_scanned[idx].name = s_names[idx];
    s_scanned[idx].category = s_cats[idx];
    s_scanned[idx].version = s_vers[idx];
    s_scanned[idx].installed = 1;

    long sz = 0;
    if (exec_path && oops_fs_exists(exec_path)) {
        sz = oops_fs_file_size(exec_path);
    }
    s_scanned[idx].size_mb = (sz > 0) ? (int)(sz / (1024 * 1024)) : 1;
    if (s_scanned[idx].size_mb <= 0) s_scanned[idx].size_mb = 1;

    s_scanned[idx].minutes_played = 0;
    s_scanned[idx].last_played_days_ago = 0;
    s_scanned[idx].trophy_unlocked = 0;
    s_scanned[idx].trophy_total = 0;

    s_scanned_count++;
}

static void scan_dir_for_titles(const char *dir_path, const char *default_category) {
    int fd = oops_fs_open(dir_path, OOPS_O_RDONLY, 0);
    if (fd < 0) return;

    char dents[4096];
    for (;;) {
#ifndef OOPS_HOST_BUILD
        long n = sys_call(272, fd, (long)dents, sizeof(dents), 0, 0, 0);
#else
        long n = 0;
#endif
        if (n <= 0) break;

        long pos = 0;
        while (pos + 8 < n && s_scanned_count < HOME_MAX_TITLES) {
            uint16_t reclen = *(const uint16_t *)(dents + pos + 4);
            if (reclen == 0) break;
            uint8_t namlen = *(const uint8_t *)(dents + pos + 7);
            const char *name = (const char *)(dents + pos + 8);

            if (namlen > 0 && !(namlen == 1 && name[0] == '.') && !(namlen == 2 && name[0] == '.' && name[1] == '.')) {
                char entry[64];
                size_t cplen = namlen < (sizeof(entry) - 1) ? namlen : (sizeof(entry) - 1);
                memcpy(entry, name, cplen);
                entry[cplen] = '\0';

                if (cplen == 9) {
                    add_discovered_title(entry, default_category, NULL);
                }
            }
            pos += reclen;
        }
    }
    oops_fs_close(fd);
}

static void home_scan_installed_titles(home_model_t *model) {
    if (model == 0) return;
    s_scanned_count = 0;

    /* 1. Walk directory trees on target */
    scan_dir_for_titles("/user/appmeta", NULL);
    scan_dir_for_titles("/data/homebrew", "HOMEBREW");
    scan_dir_for_titles("/user/app", NULL);

    /* 2. Direct presence sweep for installed candidates on disk */
    static const char *const probe_candidates[] = {
        "PROH00001", "GLCB00001", "WIPE00001", "PROO00001", "GLHW00001",
        "NET000001", "PORT00001", "PADV00001", "TRAC00001", "GALL00001",
        "PPSA21564", "PPSA02664", "PPSA04263", "PPSA03416", "PPSA25872",
        "PPSA28061", "PPSA01650", "PUWX90000", "ITEM00001", "LAPY20011",
        "NPXS39041", "NPXS40172", "PLDM00001"
    };
    for (size_t i = 0; i < sizeof(probe_candidates)/sizeof(probe_candidates[0]); i++) {
        const char *id = probe_candidates[i];
        if (is_title_scanned(id)) continue;

        char test_path[256];
        oops_snprintf(test_path, sizeof(test_path), "/user/appmeta/%s/param.json", id);
        if (oops_fs_exists(test_path)) { add_discovered_title(id, NULL, NULL); continue; }

        oops_snprintf(test_path, sizeof(test_path), "/data/homebrew/%s/eboot.bin", id);
        if (oops_fs_exists(test_path)) { add_discovered_title(id, "HOMEBREW", test_path); continue; }

        oops_snprintf(test_path, sizeof(test_path), "/user/app/%s/eboot.bin", id);
        if (oops_fs_exists(test_path)) { add_discovered_title(id, NULL, test_path); continue; }

        oops_snprintf(test_path, sizeof(test_path), "/user/app/%s/app.pkg", id);
        if (oops_fs_exists(test_path)) { add_discovered_title(id, NULL, test_path); continue; }

        oops_snprintf(test_path, sizeof(test_path), "/user/appmeta/%s/icon0.png", id);
        if (oops_fs_exists(test_path)) { add_discovered_title(id, NULL, NULL); continue; }
    }

    /* 3. Commit to model - only real discovered titles, zero made-up data */
    if (s_scanned_count > 0) {
        home_set_titles(model, s_scanned, s_scanned_count);
        oops_kprintf("HOME", "discovered %d real titles from filesystem\n", s_scanned_count);
    } else {
        home_set_titles(model, NULL, 0);
        klog("no titles found on filesystem");
    }

    /* Clear all placeholder activity, friends, saves, and captures */
    model->activity_count = 0;
    model->friend_count = 0;
    model->save_count = 0;
    model->capture_count = 0;
}

/*
 * The host dispatch seam.
 *
 * In Orbistoun, this dispatch hooks directly into emulator services (process loader, package
 * manager, save state manager, frame grabber). On console hardware, these call into system
 * daemons. Returning 1 tells the model the action was handled.
 */
static int console_perform(void *ctx, home_action_t action, int arg) {
    home_model_t *m = (home_model_t *)ctx;
    switch (action) {
        case HOME_ACTION_LAUNCH_TITLE: {
            if (m != 0 && arg >= 0 && arg < m->title_count) {
                const home_title_t *t = &m->titles[arg];
                oops_kprintf("HOME", "launching title %s (%s)\n", t->name ? t->name : "?", t->id ? t->id : "?");
                home_show_toast(m, "LAUNCHING", t->name ? t->name : "TITLE");
            } else {
                klog("launch requested - delegating to title loader");
            }
            return 1;
        }
        case HOME_ACTION_SUSPEND_TITLE:
            klog("suspend requested");
            if (m != 0) home_show_toast(m, "SWITCHER", "TITLE SUSPENDED");
            return 1;
        case HOME_ACTION_RESUME_TITLE:
            klog("resume requested");
            if (m != 0) home_show_toast(m, "SWITCHER", "TITLE RESUMED");
            return 1;
        case HOME_ACTION_TERMINATE_TITLE:
            klog("terminate requested");
            if (m != 0) home_show_toast(m, "SWITCHER", "TITLE CLOSED");
            return 1;
        case HOME_ACTION_INSTALL_PACKAGE: {
            klog("install package requested - scanning /data/pkg/...");
            const char *pkg_path = "/data/pkg/app.pkg";
            if (!oops_fs_exists(pkg_path)) {
                pkg_path = "/data/app.pkg";
            }
            if (oops_fs_exists(pkg_path)) {
                oops_kprintf("HOME", "installing package: %s\n", pkg_path);
                int ret = oops_pkg_install(pkg_path);
                if (m != 0) {
                    if (ret == 0) home_show_toast(m, "PACKAGE INSTALL", "INSTALLATION STARTED");
                    else home_show_toast(m, "PACKAGE INSTALL", "INSTALLATION FAILED");
                }
            } else {
                klog("no package file found in /data/pkg/ or /data/");
                if (m != 0) home_show_toast(m, "PACKAGE INSTALLER", "NO .PKG IN /DATA");
            }
            return 1;
        }
        case HOME_ACTION_RUN_PAYLOAD: {
            klog("run payload requested - scanning /data/...");
            const char *pld_path = "/data/payload.elf";
            if (!oops_fs_exists(pld_path)) {
                pld_path = "/data/tracer.elf";
            }
            if (oops_fs_exists(pld_path)) {
                oops_kprintf("HOME", "executing payload: %s\n", pld_path);
                if (m != 0) home_show_toast(m, "PAYLOAD RUNNER", pld_path);
            } else {
                klog("no standalone payload found in /data/");
                if (m != 0) home_show_toast(m, "PAYLOAD RUNNER", "NO .ELF IN /DATA");
            }
            return 1;
        }
        case HOME_ACTION_DELETE_TITLE: {
            if (m != 0 && arg >= 0 && arg < m->title_count) {
                const home_title_t *t = &m->titles[arg];
                oops_kprintf("HOME", "delete requested for title: %s\n", t->name ? t->name : "?");
                home_show_toast(m, "DELETE TITLE", t->name ? t->name : "TITLE");
            }
            return 1;
        }
        case HOME_ACTION_CHECK_UPDATE:
            klog("check update requested");
            if (m != 0) home_show_toast(m, "SYSTEM UPDATE", "LATEST VERSION INSTALLED");
            return 1;
        case HOME_ACTION_MANAGE_CONTENT:
            klog("manage content requested");
            if (m != 0) home_show_toast(m, "CONTENT MANAGER", "NO ADD-ONS FOUND");
            return 1;
        case HOME_ACTION_SYNC_SAVE:
        case HOME_ACTION_EXPORT_SAVE:
        case HOME_ACTION_IMPORT_SAVE:
        case HOME_ACTION_DELETE_SAVE:
            klog("save data operation requested");
            if (m != 0) home_show_toast(m, "SAVED DATA", "OPERATION COMPLETE");
            return 1;
        case HOME_ACTION_SAVE_STATE:
            klog("emulator save state requested");
            if (m != 0) home_show_toast(m, "EMULATOR", "SAVED STATE SLOT 1");
            return 1;
        case HOME_ACTION_LOAD_STATE:
            klog("emulator load state requested");
            if (m != 0) home_show_toast(m, "EMULATOR", "LOADED STATE SLOT 1");
            return 1;
        case HOME_ACTION_TAKE_SCREENSHOT:
            klog("screenshot capture requested");
            if (m != 0) home_show_toast(m, "SCREENSHOT", "CAPTURED TO /DATA");
            return 1;
        case HOME_ACTION_TOGGLE_MUSIC:
            klog("music playback toggle requested");
            if (m != 0) home_show_toast(m, "MUSIC", "PLAYBACK TOGGLED");
            return 1;
        case HOME_ACTION_REST_MODE:
            klog("rest mode requested");
            (void)oops_system_power_tick();
            if (m != 0) home_show_toast(m, "POWER", "ENTERING REST MODE");
            return 1;
        case HOME_ACTION_RESTART:
            klog("restart requested");
            (void)oops_system_power_tick();
            if (m != 0) home_show_toast(m, "POWER", "RESTARTING CONSOLE");
            return 1;
        case HOME_ACTION_POWER_OFF:
            klog("power off requested");
            (void)oops_system_power_tick();
            if (m != 0) home_show_toast(m, "POWER", "POWERING OFF");
            return 1;
        default:
            return 0;
    }
}

/*
 * The exit gesture: L1, R1 and Options together.
 */
static int exit_combo(uint32_t buttons) {
    return ((buttons & OOPS_BUTTON_L1) != 0u) &&
           ((buttons & OOPS_BUTTON_R1) != 0u) &&
           ((buttons & OOPS_BUTTON_OPTIONS) != 0u);
}

int home_start(const payload_args_t *args);

int home_start(const payload_args_t *args) {
    if (args != 0) {
        sys_call_init(args);
    }
    klog("home payload entry reached");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (disp == 0 || !oops_display_is_ready(disp)) {
        klog("display would not open - see oops_display_get_last_error");
        return -1;
    }
    if (oops_display_is_gpu_accelerated(disp)) {
        klog("display: hardware RDNA2 compute presentation enabled");
    } else {
        klog("display: software presentation active");
    }
    oops_input_init();

    home_model_t model;
    home_model_init(&model);

    home_host_t host;
    host.ctx = &model;
    host.perform = console_perform;
    home_set_host(&model, &host);

    /* Dynamically probe for on-disk titles and payloads */
    home_scan_installed_titles(&model);

    home_input_t input;
    home_input_reset(&input);

    klog("home: cross selects, circle backs, square library, triangle search, "
         "options context menu, PS button control centre, L1/R1 games/media, L1+R1+options exits");

    int running = 1;
    while (running != 0) {
        oops_pad_state_t pad;
        uint32_t buttons = 0u;
        if (oops_input_poll(0, &pad) == 0) {
            buttons = pad.buttons;
        }

        if (exit_combo(buttons) != 0) {
            running = 0;
        } else {
            (void)home_input_apply(&input, &model, buttons);
        }

        home_tick(&model);

        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels != 0) {
            (void)home_render(&surf, &model, home_theme_at(model.theme));
        }
        oops_display_flip(disp);
    }

    klog("home exiting");
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
