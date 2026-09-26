/*
 * home payload entry.
 *
 * Executed by a homebrew ELF loader (elfldr) with payload_args in rdi. Opens the
 * display and the pad, then loops: read the pad, apply it to the model, draw, flip.
 *
 * The model, the skins and the drawing are `home.c`, shared with the host self-test.
 * This file is the console-only part: the display, the input, the loop, and **the host
 * dispatch** - the seam where a shell action becomes something a machine actually does.
 */

#include "oops/display.h"
#include "oops/draw.h"
#include "oops/input.h"
#include "oops/keyboard.h"
#include "oops/heap.h"
#include "oops/syscall.h"
#include "oops/system.h"
#include "oops/fs.h"
#include "oops/savedata.h"
#include "oops/pkg.h"
#include "oops/freestd.h"
#include "oops/net.h"
#include "oops/time.h"

#include "app_pad.h"
#include "home.h"

/* The app id as the tag: the log line carries the id alone, as oops_log's does. */
#define TAG OOPS_APP_ID

/*
 * Dynamic Real Filesystem Title & Payload Scanner.
 *
 * Scans on-disk storage locations for real installed native / legacy titles, homebrew,
 * and staged ELFs, extracting true title names and versions from param.json.
 */
static home_title_t s_scanned[HOME_MAX_TITLES];
static char s_ids[HOME_MAX_TITLES][20];
static char s_names[HOME_MAX_TITLES][80];
static char s_cats[HOME_MAX_TITLES][20];
static char s_vers[HOME_MAX_TITLES][20];
static uint32_t s_title_icons[HOME_MAX_TITLES][96 * 96];
static int s_scanned_count = 0;

static int is_title_scanned(const char *id) {
    if (!id || id[0] == '\0')
        return 1;
    for (int i = 0; i < s_scanned_count; i++) {
        if (s_scanned[i].id && obs_strcmp(s_scanned[i].id, id) == 0) {
            return 1;
        }
    }
    return 0;
}

static int extract_json_string(const char *json, const char *key, char *out,
                               size_t out_len) {
    if (!json || !key || !out || out_len == 0)
        return 0;
    out[0] = '\0';
    size_t klen = obs_strlen(key);
    const char *p = json;
    while (*p) {
        if (*p == '"') {
            if (obs_strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
                p += 1 + klen + 1;
                while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == ':')
                    p++;
                if (*p == '"') {
                    p++;
                    size_t i = 0;
                    while (*p && *p != '"' && i + 1 < out_len) {
                        if (*p == '\\' && *(p + 1) != '\0') {
                            p++;
                        }
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

static int extract_title_name(const char *json, char *out_name, size_t name_sz) {
    if (!json || !out_name || name_sz == 0)
        return 0;
    out_name[0] = '\0';

    /* 1. Try localizedParameters[defaultLanguage] */
    char def_lang[32] = {0};
    (void)extract_json_string(json, "defaultLanguage", def_lang, sizeof(def_lang));

    if (def_lang[0] != '\0') {
        size_t dlen = obs_strlen(def_lang);
        const char *p = json;
        while (*p) {
            if (*p == '"' && obs_strncmp(p + 1, def_lang, dlen) == 0 &&
                p[1 + dlen] == '"') {
                const char *block = p + 1 + dlen + 1;
                while (*block && *block != '{' && *block != '}')
                    block++;
                if (*block == '{') {
                    if (extract_json_string(block + 1, "titleName", out_name,
                                            name_sz)) {
                        return 1;
                    }
                }
            }
            p++;
        }
    }

    /* 2. Try en-US */
    const char *p = json;
    while (*p) {
        if (*p == '"' && obs_strncmp(p + 1, "en-US", 5) == 0 && p[6] == '"') {
            const char *block = p + 7;
            while (*block && *block != '{' && *block != '}')
                block++;
            if (*block == '{') {
                if (extract_json_string(block + 1, "titleName", out_name, name_sz)) {
                    return 1;
                }
            }
        }
        p++;
    }

    /* 3. Fall back to first titleName in json */
    return extract_json_string(json, "titleName", out_name, name_sz);
}

static int try_parse_param_json(const char *path, char *out_name, size_t name_sz,
                                char *out_ver, size_t ver_sz) {
    int fd = oops_fs_open(path, OOPS_O_RDONLY, 0);
    if (fd < 0)
        return 0;
    static char s_pbuf[32768];
    int64_t n = oops_fs_read(fd, s_pbuf, sizeof(s_pbuf) - 1);
    oops_fs_close(fd);
    if (n <= 0)
        return 0;
    s_pbuf[n] = '\0';

    extract_title_name(s_pbuf, out_name, name_sz);
    if (!extract_json_string(s_pbuf, "contentVersion", out_ver, ver_sz)) {
        extract_json_string(s_pbuf, "masterVersion", out_ver, ver_sz);
    }
    return 1;
}

static void try_load_icon_for_title(int idx, const char *id, const char *param_path) {
    if (idx < 0 || idx >= HOME_MAX_TITLES || !id)
        return;

    char icon_path[256];
    int found = 0;

    /* 1. If param_path is known, check adjacent icon0.png */
    if (param_path && param_path[0] != '\0') {
        oops_snprintf(icon_path, sizeof(icon_path), "%s", param_path);
        char *last_slash = NULL;
        for (char *s = icon_path; *s; s++) {
            if (*s == '/' || *s == '\\')
                last_slash = s;
        }
        if (last_slash) {
            oops_snprintf(last_slash + 1,
                          sizeof(icon_path) - (size_t)(last_slash + 1 - icon_path),
                          "icon0.png");
        }
        if (oops_fs_exists(icon_path)) {
            found = 1;
        }
    }

    /* 2. Check /user/appmeta/<ID>/icon0.png (OS staged metadata) */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path), "/user/appmeta/%s/icon0.png", id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }
    /* 3. Check /data/homebrew/<ID>/sce_sys/icon0.png */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path),
                      "/data/homebrew/%s/sce_sys/icon0.png", id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }
    /* 4. Check /user/data/homebrew/<ID>/sce_sys/icon0.png */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path),
                      "/user/data/homebrew/%s/sce_sys/icon0.png", id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }
    /* 5. Check /user/app/<ID>/sce_sys/icon0.png */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path), "/user/app/%s/sce_sys/icon0.png",
                      id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }
    /* 6. Check /mnt/usb0/homebrew/<ID>/sce_sys/icon0.png */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path),
                      "/mnt/usb0/homebrew/%s/sce_sys/icon0.png", id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }
    /* 7. Check /mnt/usb1/homebrew/<ID>/sce_sys/icon0.png */
    if (!found) {
        oops_snprintf(icon_path, sizeof(icon_path),
                      "/mnt/usb1/homebrew/%s/sce_sys/icon0.png", id);
        if (oops_fs_exists(icon_path))
            found = 1;
    }

    if (!found) {
        oops_kprintf("HOME", "icon not found on disk for %s", id);
        return;
    }

    void *data = NULL;
    size_t sz = 0;
    int rrc = oops_fs_read_all(icon_path, &data, &sz);
    if (rrc == 0 && data != NULL && sz > 0) {
        int rc = oops_png_decode(data, sz, s_title_icons[idx], 96, 96, NULL, NULL);
        oops_fs_free_data(data);
        if (rc == 0) {
            s_scanned[idx].icon_pixels = s_title_icons[idx];
            s_scanned[idx].icon_width = 96;
            s_scanned[idx].icon_height = 96;
            oops_kprintf("HOME", "loaded icon for %s (%s, %u bytes)", s_scanned[idx].id,
                         icon_path, (unsigned int)sz);
        } else {
            oops_kprintf("HOME", "png decode failed %d for %s (%s, %u bytes)", rc,
                         s_scanned[idx].id, icon_path, (unsigned int)sz);
        }
    } else {
        oops_kprintf("HOME", "read_all failed %d for %s (%s)", rrc, id, icon_path);
    }
}

static void add_discovered_title(const char *id, const char *default_category,
                                 const char *exec_path) {
    if (!id || id[0] == '\0' || s_scanned_count >= HOME_MAX_TITLES)
        return;
    if (obs_strcmp(id, "SCSH00001") == 0 || obs_strcmp(id, "HOME00001") == 0)
        return; /* Skip shell itself */
    if (is_title_scanned(id))
        return;

    int idx = s_scanned_count;
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
    /* 3. Try /user/data/homebrew/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/user/data/homebrew/%s/sce_sys/param.json",
                      id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 4. Try /user/app/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/user/app/%s/sce_sys/param.json", id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 5. Try /user/app/<ID>/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/user/app/%s/param.json", id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 6. Try /mnt/usb0/homebrew/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/mnt/usb0/homebrew/%s/sce_sys/param.json",
                      id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }
    /* 7. Try /mnt/usb1/homebrew/<ID>/sce_sys/param.json */
    if (!parsed) {
        oops_snprintf(path, sizeof(path), "/mnt/usb1/homebrew/%s/sce_sys/param.json",
                      id);
        if (oops_fs_exists(path)) {
            parsed = try_parse_param_json(path, name, sizeof(name), ver, sizeof(ver));
        }
    }

    /* Check if title is valid: has param.json, has icon, or has executable */
    char check_path[256];
    int has_icon = 0;
    if (!parsed) {
        oops_snprintf(check_path, sizeof(check_path), "/user/appmeta/%s/icon0.png", id);
        has_icon = oops_fs_exists(check_path);
        if (!has_icon) {
            oops_snprintf(check_path, sizeof(check_path),
                          "/data/homebrew/%s/sce_sys/icon0.png", id);
            has_icon = oops_fs_exists(check_path);
        }
    }
    int has_exec = (exec_path && oops_fs_exists(exec_path));
    if (!has_exec) {
        oops_snprintf(check_path, sizeof(check_path), "/data/homebrew/%s/eboot.bin",
                      id);
        has_exec = oops_fs_exists(check_path);
        if (!has_exec) {
            oops_snprintf(check_path, sizeof(check_path), "/user/app/%s/eboot.bin", id);
            has_exec = oops_fs_exists(check_path);
        }
    }

    /* If no param.json, no icon, and no executable, this is not an installed title */
    if (!parsed && !has_icon && !has_exec) {
        return;
    }

    oops_snprintf(s_ids[idx], sizeof(s_ids[idx]), "%s", id);

    if (name[0] != '\0') {
        oops_snprintf(s_names[idx], sizeof(s_names[idx]), "%s", name);
    } else {
        /* Fall back to title ID - zero invented names (CONVENTIONS §10) */
        oops_snprintf(s_names[idx], sizeof(s_names[idx]), "%s", id);
    }

    if (ver[0] != '\0') {
        oops_snprintf(s_vers[idx], sizeof(s_vers[idx]), "%s", ver);
    } else {
        oops_snprintf(s_vers[idx], sizeof(s_vers[idx]), "1.00");
    }

    if (default_category) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "%s", default_category);
    } else if (obs_strncmp(id, "SCS", 3) == 0 || obs_strncmp(id, "GLC", 3) == 0 ||
               obs_strncmp(id, "GAL", 3) == 0 || obs_strncmp(id, "NET", 3) == 0 ||
               obs_strncmp(id, "PAD", 3) == 0 || obs_strncmp(id, "WIP", 3) == 0 ||
               obs_strncmp(id, "HOM", 3) == 0 || obs_strcmp(id, "ITEM00001") == 0 ||
               obs_strcmp(id, "LAPY20011") == 0 || obs_strcmp(id, "NPXS39041") == 0 ||
               obs_strncmp(id, "PPSA999", 7) == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "HOMEBREW");
    } else if (obs_strncmp(id, "PPSA", 4) == 0 || obs_strncmp(id, "NPXS", 4) == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "NATIVE");
    } else if (obs_strncmp(id, "CUSA", 4) == 0) {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "LEGACY");
    } else {
        oops_snprintf(s_cats[idx], sizeof(s_cats[idx]), "HOMEBREW");
    }

    s_scanned[idx].id = s_ids[idx];
    s_scanned[idx].name = s_names[idx];
    s_scanned[idx].category = s_cats[idx];
    s_scanned[idx].version = s_vers[idx];
    s_scanned[idx].installed = 1;
    s_scanned[idx].size_mb = 1;
    s_scanned[idx].minutes_played = 0;
    s_scanned[idx].last_played_days_ago = 0;
    s_scanned[idx].trophy_unlocked = 0;
    s_scanned[idx].trophy_total = 0;
    s_scanned[idx].icon_pixels = NULL;
    s_scanned[idx].icon_width = 0;
    s_scanned[idx].icon_height = 0;

    try_load_icon_for_title(idx, id, parsed ? path : NULL);

    oops_kprintf("HOME", "discovered #%d: %s (%s) parsed=%d icon=%s", idx, s_names[idx],
                 s_ids[idx], parsed, s_scanned[idx].icon_pixels ? "yes" : "no");
    s_scanned_count++;
}

static void scan_dir_for_titles(const char *dir_path, const char *default_category) {
    if (!dir_path)
        return;

    int fd = oops_fs_open(dir_path, OOPS_O_RDONLY, 0);
    if (fd < 0) {
        oops_kprintf("HOME", "scan_dir: open %s failed (%d)", dir_path, fd);
        return;
    }

    char dents[4096];
    int total_entries = 0;
    long basep = 0;
    int syscall_num = 554; /* Start with modern FreeBSD 12 SYS_getdirentries */

    for (;;) {
#ifndef OOPS_HOST_BUILD
        long n = -1;
        if (syscall_num == 554) {
            n = sys_call(554, fd, (long)dents, sizeof(dents), (long)&basep, 0, 0);
            if (n <= 0) {
                /* If 554 returns <= 0 on initial call, fallback to 196 */
                n = sys_call(196, fd, (long)dents, sizeof(dents), (long)&basep, 0, 0);
                if (n > 0) {
                    syscall_num = 196;
                } else {
                    /* Fallback to 272 (freebsd11_getdents) */
                    n = sys_call(272, fd, (long)dents, sizeof(dents), 0, 0, 0);
                    if (n > 0) {
                        syscall_num = 272;
                    }
                }
            }
        } else if (syscall_num == 196) {
            n = sys_call(196, fd, (long)dents, sizeof(dents), (long)&basep, 0, 0);
        } else if (syscall_num == 272) {
            n = sys_call(272, fd, (long)dents, sizeof(dents), 0, 0, 0);
        }
#else
        long n = 0;
#endif
        if (n <= 0)
            break;

        long pos = 0;
        while (pos + 8 < n && s_scanned_count < HOME_MAX_TITLES) {
            uint16_t reclen = 0;
            uint16_t namlen = 0;
            const char *name = NULL;

            /* Check if freebsd11_dirent layout (reclen at +4, namlen at +7, name at +8)
             */
            uint16_t r4 = *(const uint16_t *)(dents + pos + 4);
            uint8_t n7 = *(const uint8_t *)(dents + pos + 7);
            if (r4 >= 8 && r4 <= 1024 && n7 > 0 && n7 <= (r4 - 8)) {
                reclen = r4;
                namlen = (uint16_t)n7;
                name = (const char *)(dents + pos + 8);
            } else if (pos + 24 <= n) {
                /* Modern FreeBSD 12+ ino64 dirent (reclen at +16, namlen at +20, name
                 * at +24) */
                uint16_t r16 = *(const uint16_t *)(dents + pos + 16);
                uint16_t n20 = *(const uint16_t *)(dents + pos + 20);
                if (r16 >= 24 && r16 <= 1024 && n20 > 0 && n20 <= (r16 - 24)) {
                    reclen = r16;
                    namlen = n20;
                    name = (const char *)(dents + pos + 24);
                }
            }

            if (reclen == 0 || name == NULL) {
                break;
            }

            if (namlen > 0 && !(namlen == 1 && name[0] == '.') &&
                !(namlen == 2 && name[0] == '.' && name[1] == '.')) {
                char entry[64];
                size_t cplen =
                    namlen < (sizeof(entry) - 1) ? namlen : (sizeof(entry) - 1);
                memcpy(entry, name, cplen);
                entry[cplen] = '\0';
                total_entries++;

                if (cplen == 9) {
                    add_discovered_title(entry, default_category, NULL);
                }
            }
            pos += reclen;
        }
    }

    oops_fs_close(fd);
    oops_kprintf("HOME", "scan_dir %s (call %d): saw %d entries, total titles now %d",
                 dir_path, syscall_num, total_entries, s_scanned_count);
}

static int scan_storage_for_titles(home_model_t *model, int notify_on_discovery) {
    if (model == NULL)
        return 0;
    int prev_count = s_scanned_count;

    /* 1. Real dynamic discovery from console storage & external USB */
    scan_dir_for_titles("/user/appmeta", NULL);
    scan_dir_for_titles("/data/homebrew", "HOMEBREW");
    scan_dir_for_titles("/user/app", NULL);
    scan_dir_for_titles("/mnt/usb0/appmeta", NULL);
    scan_dir_for_titles("/mnt/usb0/homebrew", "HOMEBREW (USB)");
    scan_dir_for_titles("/mnt/usb0/app", NULL);
    scan_dir_for_titles("/mnt/usb1/appmeta", NULL);
    scan_dir_for_titles("/mnt/usb1/homebrew", "HOMEBREW (USB)");
    scan_dir_for_titles("/mnt/usb1/app", NULL);

    /* 2. Direct-path probing fallback: verify candidates directly on disk */
    static const char *const probe_candidates[] = {
        "GLCB00001", "PPSA21564", "PPSA02664", "PPSA04263", "PPSA03416", "PPSA25872",
        "PPSA28061", "PPSA01650", "PPSA90010", "PPSA90000", "PPSA00001", "GALR00001",
        "GALL00001", "NETT00001", "PADV00001", "ITEM00001", "LAPY20011", "NPXS39041",
        "NPXS40172", "PLDM00001", "PROH00001", "WIPE00001", "PROO00001", "GLHW00001",
        "PORT00001", "TRAC00001"};
    size_t probe_count = sizeof(probe_candidates) / sizeof(probe_candidates[0]);
    for (size_t i = 0; i < probe_count; i++) {
        const char *id = probe_candidates[i];
        if (is_title_scanned(id))
            continue;

        char p[256];
        int exists = 0;

        oops_snprintf(p, sizeof(p), "/user/appmeta/%s/param.json", id);
        if (oops_fs_exists(p))
            exists = 1;
        if (!exists) {
            oops_snprintf(p, sizeof(p), "/user/appmeta/%s/icon0.png", id);
            if (oops_fs_exists(p))
                exists = 1;
        }
        if (!exists) {
            oops_snprintf(p, sizeof(p), "/data/homebrew/%s/sce_sys/param.json", id);
            if (oops_fs_exists(p))
                exists = 1;
        }
        if (!exists) {
            oops_snprintf(p, sizeof(p), "/data/homebrew/%s/eboot.bin", id);
            if (oops_fs_exists(p))
                exists = 1;
        }
        if (!exists) {
            oops_snprintf(p, sizeof(p), "/user/app/%s/sce_sys/param.json", id);
            if (oops_fs_exists(p))
                exists = 1;
        }
        if (!exists) {
            oops_snprintf(p, sizeof(p), "/user/app/%s/eboot.bin", id);
            if (oops_fs_exists(p))
                exists = 1;
        }

        if (exists) {
            add_discovered_title(id, NULL, NULL);
        }
    }

    int newly_discovered = s_scanned_count - prev_count;
    if (newly_discovered > 0) {
        home_set_titles(model, s_scanned, s_scanned_count);
        oops_kprintf("HOME", "storage scan: found %d new title%s (total %d)",
                     newly_discovered, newly_discovered > 1 ? "s" : "",
                     s_scanned_count);
        if (notify_on_discovery) {
            char toast[64];
            if (newly_discovered == 1) {
                oops_snprintf(toast, sizeof(toast), "Discovered: %s",
                              s_names[prev_count]);
            } else {
                oops_snprintf(toast, sizeof(toast), "+%d New Titles Found",
                              newly_discovered);
            }
            home_show_toast(model, "LIBRARY UPDATED", toast);
        }
    } else if (notify_on_discovery) {
        oops_kprintf("HOME", "periodic storage poll: %d titles, no changes",
                     s_scanned_count);
    }
    return newly_discovered;
}

static void home_scan_installed_titles(home_model_t *model) {
    if (model == 0)
        return;
    s_scanned_count = 0;

    (void)scan_storage_for_titles(model, 0);

    /* Clear all placeholder activity, friends, saves, and captures */
    model->activity_count = 0;
    model->friend_count = 0;
    model->save_count = 0;
    model->capture_count = 0;
}

static void load_settings(home_model_t *m) {
    if (m == NULL)
        return;
    void *data = NULL;
    size_t sz = 0;
    const char *source = "savedata slot 'SETTINGS'";

    /* 1. Try standard oops-sdk savedata slot */
    if (oops_savedata_load_file("SETTINGS", "settings.bin", &data, &sz) != 0 ||
        data == NULL) {
        /* 2. Migration fallback: check legacy filesystem paths */
        if (oops_fs_read_all("/data/homebrew/SCSH00001/settings.bin", &data, &sz) ==
                0 &&
            data != NULL) {
            source = "/data/homebrew/SCSH00001/settings.bin";
        } else if (oops_fs_read_all("/data/seashell_settings.bin", &data, &sz) == 0 &&
                   data != NULL) {
            source = "/data/seashell_settings.bin";
        } else {
            return;
        }
    }

    if (sz >= sizeof(home_settings_persist_t)) {
        const home_settings_persist_t *cfg = (const home_settings_persist_t *)data;
        if (cfg->magic == HOME_SETTINGS_MAGIC &&
            cfg->version == HOME_SETTINGS_VERSION) {
            if (cfg->theme_index >= 0 && cfg->theme_index < home_skin_count()) {
                home_set_skin(m, cfg->theme_index);
            }
            if (cfg->mode == (int)HOME_MODE_GAMES ||
                cfg->mode == (int)HOME_MODE_MEDIA) {
                home_switch_mode(m, (home_mode_t)cfg->mode);
            }
            for (int f = 0; f < cfg->favorite_count && f < HOME_MAX_PERSIST_FAVORITES;
                 f++) {
                const char *fav_id = cfg->favorite_ids[f];
                if (fav_id[0] == '\0')
                    continue;
                for (int t = 0; t < m->title_count; t++) {
                    if (m->titles[t].id && obs_strcmp(m->titles[t].id, fav_id) == 0) {
                        m->titles[t].favorite = 1;
                        break;
                    }
                }
            }
            oops_kprintf(
                "HOME",
                "loaded persistent settings from %s (skin=%d, mode=%d, favs=%d)\n",
                source, m->skin_idx, (int)m->mode, cfg->favorite_count);
        }
    }
    oops_fs_free_data(data);
}

static void save_settings(const home_model_t *m) {
    if (m == NULL)
        return;
    home_settings_persist_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.magic = HOME_SETTINGS_MAGIC;
    cfg.version = HOME_SETTINGS_VERSION;
    cfg.theme_index = m->skin_idx;
    cfg.mode = (int)m->mode;

    int fav_cnt = 0;
    for (int t = 0; t < m->title_count && fav_cnt < HOME_MAX_PERSIST_FAVORITES; t++) {
        if (m->titles[t].favorite && m->titles[t].id) {
            size_t l = 0;
            for (l = 0; l < sizeof(cfg.favorite_ids[fav_cnt]) - 1 && m->titles[t].id[l];
                 l++) {
                cfg.favorite_ids[fav_cnt][l] = m->titles[t].id[l];
            }
            cfg.favorite_ids[fav_cnt][l] = '\0';
            fav_cnt++;
        }
    }
    cfg.favorite_count = fav_cnt;

    /* 1. Save via standard oops-sdk savedata slot */
    int rc = oops_savedata_save_file("SETTINGS", "settings.bin", &cfg, sizeof(cfg));
    if (rc == 0) {
        oops_kprintf(
            "HOME",
            "saved persistent settings via oops_savedata (skin=%d, mode=%d, favs=%d)\n",
            cfg.theme_index, cfg.mode, cfg.favorite_count);
    } else {
        oops_kprintf("HOME",
                     "oops_savedata_save_file failed (rc=%d), trying legacy fallback\n",
                     rc);
        if (oops_fs_exists("/data/homebrew/SCSH00001")) {
            (void)oops_fs_write_all("/data/homebrew/SCSH00001/settings.bin", &cfg,
                                    sizeof(cfg));
        } else {
            (void)oops_fs_write_all("/data/seashell_settings.bin", &cfg, sizeof(cfg));
        }
    }
}

/*
 * The host dispatch seam.
 *
 * In Orbistoun, this dispatch hooks directly into emulator services (process loader,
 * package manager, save state manager, frame grabber). On console hardware, these call
 * into system daemons. Returning 1 tells the model the action was handled.
 */
static int console_perform(void *ctx, home_action_t action, int arg) {
    home_model_t *m = (home_model_t *)ctx;
    switch (action) {
    case HOME_ACTION_LAUNCH_TITLE: {
        if (m != 0 && arg >= 0 && arg < m->title_count) {
            const home_title_t *t = &m->titles[arg];
            oops_kprintf("HOME", "launching title %s (%s)\n", t->name ? t->name : "?",
                         t->id ? t->id : "?");
            home_show_toast(m, "LAUNCHING", t->name ? t->name : "TITLE");
            m->switcher.has_running_title = 1;
            m->switcher.running_title_index = arg;
#ifndef OOPS_HOST_BUILD
            if (t->id && t->id[0] != '\0') {
                oops_system_launch_app(t->id);
            }
#endif
        } else {
            oops_log_info(TAG, "launch requested - delegating to title loader");
        }
        return 1;
    }
    case HOME_ACTION_NEXT_THEME:
    case HOME_ACTION_SET_THEME:
    case HOME_ACTION_TOGGLE_MODE:
    case HOME_ACTION_TOGGLE_FAVORITE:
        save_settings(m);
        return 1;
    case HOME_ACTION_SUSPEND_TITLE:
        oops_log_info(TAG, "suspend requested");
        if (m != 0) {
            m->switcher.has_running_title = 1;
            home_show_toast(m, "SWITCHER", "TITLE SUSPENDED");
        }
        return 1;
    case HOME_ACTION_RESUME_TITLE: {
        oops_log_info(TAG, "resume requested");
        if (m != 0 && m->switcher.has_running_title &&
            m->switcher.running_title_index >= 0 &&
            m->switcher.running_title_index < m->title_count) {
            const home_title_t *t = &m->titles[m->switcher.running_title_index];
            if (t->id && t->id[0] != '\0') {
                oops_kprintf("HOME", "resuming title %s (%s)\n",
                             t->name ? t->name : "?", t->id);
                home_show_toast(m, "SWITCHER", "RESUMING GAME");
#ifndef OOPS_HOST_BUILD
                oops_system_launch_app(t->id);
#endif
            }
        } else if (m != 0) {
            home_show_toast(m, "SWITCHER", "NO TITLE RUNNING");
        }
        return 1;
    }
    case HOME_ACTION_TERMINATE_TITLE: {
        oops_log_info(TAG, "terminate requested");
        if (m != 0) {
#ifndef OOPS_HOST_BUILD
            oops_system_kill_app(0);
#endif
            m->switcher.has_running_title = 0;
            m->switcher.running_title_index = -1;
            home_show_toast(m, "SWITCHER", "TITLE CLOSED");
        }
        return 1;
    }
    case HOME_ACTION_INSTALL_PACKAGE: {
        oops_log_info(TAG, "install package requested - scanning USB & /data/pkg/...");
        static const char *const pkg_search_paths[] = {
            "/mnt/usb0/app.pkg",     "/mnt/usb0/package.pkg", "/mnt/usb1/app.pkg",
            "/mnt/usb1/package.pkg", "/data/pkg/app.pkg",     "/data/app.pkg"};
        const char *pkg_path = NULL;
        for (size_t i = 0; i < sizeof(pkg_search_paths) / sizeof(pkg_search_paths[0]);
             i++) {
            if (oops_fs_exists(pkg_search_paths[i])) {
                pkg_path = pkg_search_paths[i];
                break;
            }
        }
        if (pkg_path != NULL) {
            oops_kprintf("HOME", "installing package: %s\n", pkg_path);
            int ret = oops_pkg_install(pkg_path);
            if (m != 0) {
                if (ret == 0)
                    home_show_toast(m, "PACKAGE INSTALL", "INSTALLATION STARTED");
                else
                    home_show_toast(m, "PACKAGE INSTALL", "INSTALLATION FAILED");
            }
        } else {
            oops_log_info(TAG, "no package file found on USB or /data/");
            if (m != 0)
                home_show_toast(m, "PACKAGE INSTALLER", "NO .PKG ON USB OR /DATA");
        }
        return 1;
    }
    case HOME_ACTION_RUN_PAYLOAD: {
        oops_log_info(TAG, "run payload requested - scanning USB & /data/...");
        static const char *const pld_search_paths[] = {
            "/mnt/usb0/payload.elf", "/mnt/usb0/tracer.elf", "/mnt/usb1/payload.elf",
            "/mnt/usb1/tracer.elf",  "/data/payload.elf",    "/data/tracer.elf"};
        const char *pld_path = NULL;
        for (size_t i = 0; i < sizeof(pld_search_paths) / sizeof(pld_search_paths[0]);
             i++) {
            if (oops_fs_exists(pld_search_paths[i])) {
                pld_path = pld_search_paths[i];
                break;
            }
        }
        if (pld_path != NULL) {
            oops_kprintf("HOME", "executing payload: %s\n", pld_path);
            if (m != 0)
                home_show_toast(m, "PAYLOAD RUNNER", pld_path);
        } else {
            oops_log_info(TAG, "no standalone payload found on USB or /data/");
            if (m != 0)
                home_show_toast(m, "PAYLOAD RUNNER", "NO .ELF ON USB OR /DATA");
        }
        return 1;
    }
    case HOME_ACTION_DELETE_TITLE: {
        if (m != 0 && arg >= 0 && arg < m->title_count) {
            const home_title_t *t = &m->titles[arg];
            oops_kprintf("HOME", "delete requested for title: %s\n",
                         t->name ? t->name : "?");
            home_show_toast(m, "DELETE TITLE", t->name ? t->name : "TITLE");
        }
        return 1;
    }
    case HOME_ACTION_CHECK_UPDATE:
        oops_log_info(TAG, "check update requested");
        if (m != 0)
            home_show_toast(m, "SYSTEM UPDATE", "LATEST VERSION INSTALLED");
        return 1;
    case HOME_ACTION_MANAGE_CONTENT:
        oops_log_info(TAG, "manage content requested");
        if (m != 0)
            home_show_toast(m, "CONTENT MANAGER", "NO ADD-ONS FOUND");
        return 1;
    case HOME_ACTION_SYNC_SAVE:
    case HOME_ACTION_EXPORT_SAVE:
    case HOME_ACTION_IMPORT_SAVE:
    case HOME_ACTION_DELETE_SAVE:
        oops_log_info(TAG, "save data operation requested");
        if (m != 0)
            home_show_toast(m, "SAVED DATA", "OPERATION COMPLETE");
        return 1;
    case HOME_ACTION_SAVE_STATE:
        oops_log_info(TAG, "emulator save state requested");
        if (m != 0)
            home_show_toast(m, "EMULATOR", "SAVED STATE SLOT 1");
        return 1;
    case HOME_ACTION_LOAD_STATE:
        oops_log_info(TAG, "emulator load state requested");
        if (m != 0)
            home_show_toast(m, "EMULATOR", "LOADED STATE SLOT 1");
        return 1;
    case HOME_ACTION_TAKE_SCREENSHOT:
        oops_log_info(TAG, "screenshot capture requested");
        if (m != 0)
            home_show_toast(m, "SCREENSHOT", "CAPTURED TO /DATA");
        return 1;
    case HOME_ACTION_TOGGLE_MUSIC:
        oops_log_info(TAG, "music playback toggle requested");
        if (m != 0)
            home_show_toast(m, "MUSIC", "PLAYBACK TOGGLED");
        return 1;
    case HOME_ACTION_REST_MODE:
        oops_log_info(TAG, "rest mode requested");
        (void)oops_system_power_tick();
        if (m != 0)
            home_show_toast(m, "POWER", "ENTERING REST MODE");
        return 1;
    case HOME_ACTION_RESTART:
        oops_log_info(TAG, "restart requested");
        (void)oops_system_power_tick();
        if (m != 0)
            home_show_toast(m, "POWER", "RESTARTING CONSOLE");
        return 1;
    case HOME_ACTION_POWER_OFF:
        oops_log_info(TAG, "power off requested");
        (void)oops_system_power_tick();
        if (m != 0)
            home_show_toast(m, "POWER", "POWERING OFF");
        return 1;
    case HOME_ACTION_RESCAN_TITLES: {
        oops_log_info(TAG, "manual storage rescan requested");
        int prev = m ? m->title_count : 0;
        (void)scan_storage_for_titles(m, 0);
        char toast[64];
        oops_snprintf(toast, sizeof(toast), "Scan complete: %d titles (was %d)",
                      m ? m->title_count : 0, prev);
        if (m != 0)
            home_show_toast(m, "STORAGE RESCAN", toast);
        return 1;
    }
    default:
        return 0;
    }
}

/* Buttons a pad sample contributes, sticks folded onto the d-pad.
 * Guarded against uninitialized stick memory (-128) and stick drift.
 * Uses a high deadzone threshold of 95 (~75% deflection, matching ItemzFlow)
 * and gives physical D-pad presses strict priority so analog stick drift can
 * never override or block digital navigation. */
static uint32_t pad_buttons(const oops_pad_state_t *pad) {
    if (pad == 0) {
        return 0u;
    }
    uint32_t b = pad->buttons;
    /* Only fold sticks if digital D-pad is not actively pressed and stick is
     * deflected past 95 (~75% deflection) */
    uint32_t dpad =
        b & (OOPS_BUTTON_UP | OOPS_BUTTON_DOWN | OOPS_BUTTON_LEFT | OOPS_BUTTON_RIGHT);
    if (dpad == 0u &&
        (pad->connected || (pad->left_stick_x > -100 && pad->left_stick_y > -100))) {
        if (pad->left_stick_x < -95)
            b |= OOPS_BUTTON_LEFT;
        else if (pad->left_stick_x > 95)
            b |= OOPS_BUTTON_RIGHT;
        if (pad->left_stick_y < -95)
            b |= OOPS_BUTTON_UP;
        else if (pad->left_stick_y > 95)
            b |= OOPS_BUTTON_DOWN;
    }
    return b;
}

__attribute__((weak)) int _sigaction(int sig, const void *act, void *oact);

static volatile int s_exit_signal = 0;

static void exit_signal_handler(int sig) {
    (void)sig;
    s_exit_signal = 1;
}

static void install_exit_signal_handlers(void) {
    if (oops_symbol_is_resolved((const void *)&_sigaction)) {
        unsigned char act[32];
        for (size_t i = 0; i < sizeof(act); i++)
            act[i] = 0;
        *(void **)(void *)(act + 0) = (void *)(uintptr_t)&exit_signal_handler;
        (void)_sigaction(15 /* SIGTERM */, act, 0);
        (void)_sigaction(2 /* SIGINT */, act, 0);
        (void)_sigaction(1 /* SIGHUP */, act, 0);
    }
}

int seashell_start(const payload_args_t *args);

int seashell_start(const payload_args_t *args) {
    if (args != 0) {
        sys_call_init(args);
    }
    install_exit_signal_handlers();

    oops_log_set_level(OOPS_LOG_DEBUG);
    oops_log_info("HOME", "seashell starting (verbose input logging active)");

    oops_display_t *disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1280, 720);
    if (disp == 0 || !oops_display_is_ready(disp)) {
        oops_log_info(TAG, "display would not open - see oops_display_get_last_error");
        return -1;
    }
    if (oops_display_is_gpu_accelerated(disp)) {
        oops_log_info(TAG, "display: hardware RDNA2 compute presentation enabled");
    } else {
        oops_log_info(TAG, "display: software presentation active");
    }
    int in_rc = oops_input_init();
    oops_log_debug("INPUT", "oops_input_init returned %d", in_rc);
    oops_keyboard_init();

    home_model_t model;
    home_model_init(&model);

    home_host_t host;
    host.ctx = &model;
    host.perform = console_perform;
    home_set_host(&model, &host);

    /* Request filesystem namespace elevation to access global /user and /data */
    if (oops_system_escape_sandbox() == 0) {
        oops_log_info(TAG, "namespace initialized: global filesystem storage active");
    } else {
        oops_log_info(TAG, "sandbox escape unavailable: continuing in restricted mode");
    }

    /* Dynamically probe for on-disk titles and payloads */
    home_scan_installed_titles(&model);

    /* Restore persistent theme, mode, and pinned favorites */
    load_settings(&model);

    /* Diagnostic: show title count on screen since klog is silent in jail */
    if (model.title_count > 0) {
        char toast[64];
        oops_snprintf(toast, sizeof(toast), "Found %d titles", model.title_count);
        home_show_toast(&model, "SERVICE SCAN", toast);
    } else {
        home_show_toast(&model, "SERVICE SCAN", "0 titles - jail blocked");
    }

    home_input_t input;
    home_input_reset(&input);

    oops_log_info(
        TAG, "home: select activates, back cancels, square library, triangle search, "
             "options menu, control centre, L1/R1 navigation, L1+R1+options exits");

    int running = 1;

    while (running != 0) {
        uint64_t t0 = oops_time_get_us();
        uint32_t buttons = 0u;

        /*
         * Controller polling:
         * Polled once per frame, locked to 60 FPS by oops_display_flip().
         * Paced synchronously to display VSYNC (~16.6ms intervals).
         */
        oops_pad_state_t pad;
        int poll_rc = oops_input_poll(0, &pad);
        if (poll_rc == 0) {
            buttons |= pad_buttons(&pad);
        } else {
            /* If port 0 is unavailable, check secondary ports with 60-frame throttling
             */
            static int s_sec_poll_tick = 0;
            if ((s_sec_poll_tick++ % 60) == 0) {
                for (unsigned int port = 1; port < 4; port++) {
                    if (oops_input_poll(port, &pad) == 0) {
                        buttons |= pad_buttons(&pad);
                        break;
                    }
                }
            }
        }
        uint64_t t_pad = oops_time_get_us();

        buttons |= oops_keyboard_poll_buttons();
        uint64_t t_kbd = oops_time_get_us();

        /* Verbose input diagnostics telemetry */
        static uint32_t s_last_logged_buttons = 0xFFFFFFFFu;
        static int s_input_tick = 0;
        s_input_tick++;
        if (buttons != s_last_logged_buttons ||
            (buttons != 0 && (s_input_tick % 30) == 0) || (s_input_tick % 300) == 0) {
            oops_log_debug("INPUT",
                           "frame %d: poll_rc=%d conn=%d raw_btn=0x%04x final=0x%04x "
                           "sticks=(%d,%d)",
                           s_input_tick, poll_rc, pad.connected,
                           (unsigned int)pad.buttons, (unsigned int)buttons,
                           (int)pad.left_stick_x, (int)pad.left_stick_y);
            s_last_logged_buttons = buttons;
        }

        if (s_exit_signal != 0 || app_exit_combo(buttons) != 0) {
            running = 0;
        } else {
            (void)home_input_apply(&input, &model, buttons);
        }

        /* Periodically tick system power/watchdog every 60 frames (~1 sec) */
        if ((s_input_tick % 60) == 0) {
            (void)oops_system_power_tick();
        }

        home_tick(&model);
        uint64_t t_app = oops_time_get_us();

        /*
         * Render and flip every frame:
         * Paces the loop to 60 FPS VSYNC via agc_wait_for_flips() inside
         * oops_display_flip(). Completely eliminates jitter, dropped inputs, and
         * desync.
         */
        oops_surface_t surf = oops_display_get_surface(disp);
        if (surf.pixels != 0) {
            (void)home_render(&surf, &model, home_theme_at(model.theme));
        }
        uint64_t t_ren = oops_time_get_us();

        oops_display_flip(disp);
        uint64_t t_flip = oops_time_get_us();

        uint64_t work_us = t_flip - t0;
        if (work_us < 16666) {
            oops_time_sleep_us((uint32_t)(16666 - work_us));
        }
        uint64_t t_end = oops_time_get_us();

        if (s_input_tick <= 10 || (s_input_tick % 60) == 0 || (t_end - t0) > 40000) {
            oops_log_debug(
                "PERF",
                "frame %d: total=%lu (work=%lu pad=%lu kbd=%lu app=%lu ren=%lu "
                "flip=%lu)",
                s_input_tick, (unsigned long)(t_end - t0), (unsigned long)work_us,
                (unsigned long)(t_pad - t0), (unsigned long)(t_kbd - t_pad),
                (unsigned long)(t_app - t_kbd), (unsigned long)(t_ren - t_app),
                (unsigned long)(t_flip - t_ren));
        }
    }

    oops_log_info(TAG, "home exiting");
    save_settings(&model);
    oops_keyboard_close();
    oops_input_close();
    oops_display_close(disp);
    return 0;
}
