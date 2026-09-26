/*
 * home - clean-room reimplementation of the Prospero shell (Prospero UX / SceShellCore)
 * for homebrew and the Orbistoun emulator.
 *
 * Shared between the host self-test and the payload, so nothing here may call libc,
 * allocate, or touch a machine. Everything is a pure function of the model, a theme and
 * a surface; the things that are not pure go out through the host dispatch in home.h.
 */

#include "app_pad.h"
#include "home.h"
#include "oops/system.h"
#include "oops/freestd.h"

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

/* ---- skin management
 * -------------------------------------------------------------------- */

const home_skin_t *home_current_skin(const home_model_t *m) {
    if (m == 0)
        return home_skin_at(0);
    return home_skin_at(m->skin_idx);
}

void home_set_skin(home_model_t *m, int index) {
    if (m == 0)
        return;
    int count = home_skin_count();
    if (count <= 0)
        return;
    m->skin_idx = (index % count + count) % count;
    m->theme = m->skin_idx;
    const home_skin_t *skin = home_skin_at(m->skin_idx);
    if (skin) {
        m->category_idx = skin->default_category;
        if (skin->init) {
            skin->init(m, skin);
        }
    }
}

void home_next_skin(home_model_t *m) {
    if (m == 0)
        return;
    home_set_skin(m, m->skin_idx + 1);
}

void home_next_theme(home_model_t *m) {
    home_next_skin(m);
}

/* ---- menu building
 * ---------------------------------------------------------------------- */

static void menu_reset(home_menu_t *menu, const char *heading) {
    menu->heading = heading;
    menu->count = 0;
    menu->cursor = 0;
    for (int i = 0; i < HOME_MAX_ITEMS; i++) {
        menu->items[i].label = 0;
        menu->items[i].detail = 0;
        menu->items[i].action = HOME_ACTION_NONE;
        menu->items[i].arg = 0;
        menu->items[i].enabled = 0;
    }
}

static void menu_add(home_menu_t *menu, const char *label, const char *detail,
                     home_action_t action, int arg, int enabled) {
    if (menu->count >= HOME_MAX_ITEMS) {
        return;
    }
    home_item_t *item = &menu->items[menu->count];
    item->label = label;
    item->detail = detail;
    item->action = action;
    item->arg = arg;
    item->enabled = enabled;
    menu->count++;
}

static int screen_is_carousel(home_screen_t screen) {
    return (screen == HOME_SCREEN_GAMES) || (screen == HOME_SCREEN_MEDIA);
}

static char to_upper_ascii(char c) {
    if (c >= 'a' && c <= 'z')
        return (char)(c - 'a' + 'A');
    return c;
}

static int str_contains_ci(const char *haystack, const char *needle) {
    if (!needle || needle[0] == '\0')
        return 1;
    if (!haystack)
        return 0;
    for (int i = 0; haystack[i] != '\0'; i++) {
        int match = 1;
        for (int j = 0; needle[j] != '\0'; j++) {
            if (haystack[i + j] == '\0' ||
                to_upper_ascii(haystack[i + j]) != to_upper_ascii(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match)
            return 1;
    }
    return 0;
}

static void build_title_options_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_TITLE_OPTIONS];
    int index = m->selected_title;
    if (index < 0 || index >= m->title_count) {
        menu_reset(menu, "TITLE OPTIONS");
        menu_add(menu, "NOTHING SELECTED", 0, HOME_ACTION_BACK, 0, 1);
        return;
    }
    const home_title_t *title = &m->titles[index];
    menu_reset(menu, title->name);
    menu_add(menu, "PLAY", title->id, HOME_ACTION_LAUNCH_TITLE, index,
             title->installed);
    menu_add(menu, title->favorite ? "REMOVE FROM FAVORITES" : "ADD TO FAVORITES",
             title->favorite ? "UNPIN FROM FAVORITES" : "PIN TO FAVORITES VIEW",
             HOME_ACTION_TOGGLE_FAVORITE, index, 1);
    menu_add(menu, "CHECK FOR UPDATE", "VERSION 1.002.000", HOME_ACTION_CHECK_UPDATE,
             index, 1);
    menu_add(menu, "MANAGE GAME CONTENT", "1 ADD-ON INSTALLED",
             HOME_ACTION_MANAGE_CONTENT, index, 1);
    menu_add(menu, "SAVED DATA", "SYNC WITH CLOUD / USB", HOME_ACTION_SYNC_SAVE, index,
             1);
    menu_add(menu, "INFORMATION", "VIEW METADATA & SPECS", HOME_ACTION_OPEN,
             (int)HOME_SCREEN_TITLE_INFO, 1);
    menu_add(menu, "DELETE", "REMOVE FROM STORAGE", HOME_ACTION_DELETE_TITLE, index,
             title->installed);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_title_info_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_TITLE_INFO];
    int index = m->selected_title;
    if (index < 0 || index >= m->title_count) {
        menu_reset(menu, "INFORMATION");
        menu_add(menu, "NOTHING SELECTED", 0, HOME_ACTION_BACK, 0, 1);
        return;
    }
    const home_title_t *title = &m->titles[index];
    menu_reset(menu, "TITLE INFORMATION");
    menu_add(menu, title->name, 0, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "TITLE ID", title->id, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "CATEGORY", title->category, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "VERSION", title->version, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "EXECUTABLE FORMAT", "PROSPERO NATIVE ELF", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "LOCATION", "CONSOLE SSD /USER/APP/", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "TARGET SDK", "12.40 (0x12400000)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "AUDIO FORMAT", "LINEAR PCM 7.1 / TEMPEST 3D", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "PARENTAL LEVEL", "LEVEL 1 (ALL AGES)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_switcher_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SWITCHER];
    menu_reset(menu, "SWITCHER");
    if (m->switcher.has_running_title != 0 && m->switcher.running_title_index >= 0 &&
        m->switcher.running_title_index < m->title_count) {
        const home_title_t *running = &m->titles[m->switcher.running_title_index];
        const char *state_str =
            m->switcher.is_suspended ? "SUSPENDED (IN BACKGROUND)" : "NOW PLAYING";
        menu_add(menu, state_str, running->name, HOME_ACTION_NONE, 0, 1);
        if (m->switcher.is_suspended) {
            menu_add(menu, "RESUME GAME", "RETURN TO ACTIVE PLAY",
                     HOME_ACTION_RESUME_TITLE, m->switcher.running_title_index, 1);
        } else {
            menu_add(menu, "SUSPEND GAME", "FREEZE IN BACKGROUND",
                     HOME_ACTION_SUSPEND_TITLE, m->switcher.running_title_index, 1);
        }
        menu_add(menu, "CLOSE GAME", "TERMINATE PROCESS", HOME_ACTION_TERMINATE_TITLE,
                 m->switcher.running_title_index, 1);
    } else {
        menu_add(menu, "NO GAME RUNNING", "SELECT A TITLE TO LAUNCH", HOME_ACTION_NONE,
                 0, 0);
    }
    for (int i = 0; i < m->switcher.recent_count; i++) {
        int idx = m->switcher.recent_indices[i];
        if (idx >= 0 && idx < m->title_count) {
            menu_add(menu, "RECENT", m->titles[idx].name, HOME_ACTION_LAUNCH_TITLE, idx,
                     1);
        }
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_search_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SEARCH];
    menu_reset(menu, "UNIVERSAL SEARCH");
    const char *query =
        (m->last_search[0] != '\0') ? m->last_search : "SEARCH TITLES & PAYLOADS";
    menu_add(menu, "OPEN KEYBOARD", query, HOME_ACTION_TRIGGER_DIALOG,
             (int)HOME_DIALOG_IME, 1);

    int count = 0;
    if (m->last_search[0] != '\0') {
        for (int i = 0; i < m->title_count; i++) {
            const home_title_t *t = &m->titles[i];
            if (str_contains_ci(t->name, m->last_search) ||
                str_contains_ci(t->id, m->last_search)) {
                menu_add(menu, t->name, t->id, HOME_ACTION_LAUNCH_TITLE, i, 1);
                count++;
            }
        }
        if (count == 0) {
            menu_add(menu, "NO MATCHING TITLES", "PRESS KEYBOARD TO SEARCH AGAIN",
                     HOME_ACTION_NONE, 0, 0);
        }
    } else {
        for (int i = 0; i < m->title_count; i++) {
            menu_add(menu, m->titles[i].name, m->titles[i].id, HOME_ACTION_LAUNCH_TITLE,
                     i, 1);
        }
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_game_base_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_GAME_BASE];
    menu_reset(menu, "GAME BASE");
    for (int i = 0; i < m->friend_count; i++) {
        menu_add(menu, m->friends[i].name, m->friends[i].activity, HOME_ACTION_NONE, 0,
                 m->friends[i].online);
    }
    if (m->friend_count == 0) {
        menu_add(menu, "NO FRIENDS ONLINE", "NO ACTIVE PARTY", HOME_ACTION_NONE, 0, 0);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_music_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_MUSIC];
    menu_reset(menu, "MUSIC");
    menu_add(menu, "NO AUDIO PLAYING", 0, HOME_ACTION_NONE, 0, 0);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_captures_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_CAPTURES];
    menu_reset(menu, "MEDIA GALLERY");
    for (int i = 0; i < m->capture_count; i++) {
        menu_add(menu, m->captures[i].title, m->captures[i].type_and_res,
                 HOME_ACTION_NONE, 0, 1);
    }
    menu_add(menu, "TAKE SCREENSHOT", "CAPTURE CURRENT FRAME",
             HOME_ACTION_TAKE_SCREENSHOT, 0, 1);
    menu_add(menu, "EXPORT ALL TO USB / HOST", 0, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_saves_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_SAVES];
    menu_reset(menu, "SAVED DATA (PROSPERO)");
    for (int i = 0; i < m->save_count; i++) {
        menu_add(menu, m->saves[i].name, m->saves[i].timestamp, HOME_ACTION_NONE, 0, 1);
    }
    menu_add(menu, "BACKUP ALL TO USB / HOST", 0, HOME_ACTION_EXPORT_SAVE, 0, 1);
    menu_add(menu, "RESTORE FROM USB / HOST", 0, HOME_ACTION_IMPORT_SAVE, 0, 1);
    menu_add(menu, "DELETE ALL SAVES", 0, HOME_ACTION_DELETE_SAVE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_storage_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_STORAGE];
    menu_reset(menu, "STORAGE");
    menu_add(menu, "CONSOLE STORAGE (SSD)", "412 GB / 825 GB USED", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "M.2 SSD STORAGE", "550 GB / 2000 GB USED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "USB EXTENDED STORAGE", "190 GB / 500 GB USED", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "EMULATOR HOST SHARE", "DIRECT FILESYSTEM MOUNT", HOME_ACTION_NONE,
             0, 1);
    menu_add(menu, "AUTO CLEANUP UNUSED CACHES", 0, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static const char *const s_filter_names[HOME_FILTER_COUNT] = {
    "ALL TITLES", "NATIVE APPS", "HOMEBREW & ELFS", "FAVORITES (PINNED)"};

static void build_library_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_LIBRARY];
    menu_reset(menu, "GAME LIBRARY");

    int filter_idx = (int)m->library_filter;
    if (filter_idx < 0 || filter_idx >= HOME_FILTER_COUNT) {
        filter_idx = 0;
        m->library_filter = HOME_FILTER_ALL;
    }
    menu_add(menu, "FILTER CATEGORY", s_filter_names[filter_idx],
             HOME_ACTION_SET_LIBRARY_FILTER, 0, 1);

    int count = 0;
    for (int i = 0; i < m->title_count; i++) {
        const home_title_t *t = &m->titles[i];
        int include = 0;
        switch (m->library_filter) {
        case HOME_FILTER_ALL:
            include = 1;
            break;
        case HOME_FILTER_NATIVE:
            if (str_contains_ci(t->category, "BIG APP") ||
                str_contains_ci(t->category, "MINI APP") ||
                str_contains_ci(t->category, "NATIVE") ||
                str_contains_ci(t->category, "APP")) {
                include = 1;
            }
            break;
        case HOME_FILTER_HOMEBREW:
            if (str_contains_ci(t->category, "HOMEBREW") ||
                str_contains_ci(t->category, "ELF") ||
                str_contains_ci(t->category, "PAYLOAD")) {
                include = 1;
            }
            break;
        case HOME_FILTER_FAVORITES:
            if (t->favorite != 0) {
                include = 1;
            }
            break;
        default:
            include = 1;
            break;
        }

        if (include) {
            menu_add(menu, t->name, t->favorite ? "[*] FAVORITE" : t->category,
                     HOME_ACTION_LAUNCH_TITLE, i, 1);
            count++;
        }
    }

    if (count == 0) {
        menu_add(menu, "NO MATCHING TITLES", "PRESS CROSS ON FILTER TO CYCLE",
                 HOME_ACTION_SET_LIBRARY_FILTER, 0, 1);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_system_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_SYSTEM];
    menu_reset(menu, "SYSTEM");
    menu_add(menu, "CONSOLE INFORMATION",
             m->dev.console_info_str[0] ? m->dev.console_info_str
                                        : "PROSPERO (FW 12.40)",
             HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "SEASHELL VERSION", OOPS_APP_VERSION, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "HARDWARE TELEMETRY",
             m->dev.hw_telemetry_str[0] ? m->dev.hw_telemetry_str
                                        : "CPU -- C | FAN --%",
             HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "SYSTEM SOFTWARE UPDATE", "CHECK AUTOMATICALLY", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "POWER SAVING", "REST MODE IN 1 HOUR", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "HDMI", "HDMI DEVICE LINK ENABLED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "REMOTE PLAY", "ENABLED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "RESET OPTIONS", "REBUILD DATABASE / CLEAR CACHE", HOME_ACTION_NONE,
             0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_developer_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_DEVELOPER];
    menu_reset(menu, "DEVELOPER & DEBUG");
    menu_add(menu, "INSTALL PACKAGE (PKG)", "SCAN USB & /DATA/PKG",
             HOME_ACTION_INSTALL_PACKAGE, 0, 1);
    menu_add(menu, "RUN PAYLOAD (ELF)", "SCAN USB & /DATA/PAYLOADS",
             HOME_ACTION_RUN_PAYLOAD, 0, 1);
    menu_add(menu, "APP CATEGORY OVERRIDE", "BIG APP 0 (PROSPERO)", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "PLTAUTH STATUS",
             m->dev.pltauth_str[0] ? m->dev.pltauth_str : "ACTIVE", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "RESCAN INSTALLED TITLES", "SCAN STORAGE & USB",
             HOME_ACTION_RESCAN_TITLES, 0, 1);
    menu_add(menu, "LIVE KERNEL LOG (KLOG)", "VIEW SYSTEM LOG STREAM", HOME_ACTION_NONE,
             0, 1);
    menu_add(menu, "FILESYSTEM BROWSER", "EXPLORE /APP0 /DATA /USER", HOME_ACTION_NONE,
             0, 1);
    menu_add(menu, "DEV TEST: CONFIRM DIALOG", 0, HOME_ACTION_TRIGGER_DIALOG,
             (int)HOME_DIALOG_CONFIRM, 1);
    menu_add(menu, "DEV TEST: PROGRESS DIALOG", 0, HOME_ACTION_TRIGGER_DIALOG,
             (int)HOME_DIALOG_PROGRESS, 1);
    menu_add(menu, "DEV TEST: ERROR CE-108255-1", 0, HOME_ACTION_TRIGGER_DIALOG,
             (int)HOME_DIALOG_ERROR, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_theme_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_THEME];
    menu_reset(menu, "THEMES & SKINS");
    int count = home_skin_count();
    for (int i = 0; i < count && i < HOME_MAX_ITEMS - 1; i++) {
        const home_skin_t *s = home_skin_at(i);
        const char *detail = (i == m->skin_idx) ? "[ACTIVE]" : (s ? s->description : 0);
        menu_add(menu, s ? s->name : "SKIN", detail, HOME_ACTION_SET_THEME, i, 1);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

void home_refresh_telemetry(home_model_t *m) {
    if (m == 0)
        return;

    oops_system_info_t sys_info;
    if (oops_system_get_info(&sys_info) == 0) {
        m->dev.total_ram_mb = (int)sys_info.total_ram_mb;
        m->dev.direct_mem_mb = (int)sys_info.direct_mem_mb;
        size_t i = 0;
        for (i = 0; i < sizeof(m->dev.fw_version) - 1 && sys_info.firmware_str[i];
             i++) {
            m->dev.fw_version[i] = sys_info.firmware_str[i];
        }
        m->dev.fw_version[i] = '\0';

        for (i = 0; i < sizeof(m->dev.model_name) - 1 && sys_info.model_str[i]; i++) {
            m->dev.model_name[i] = sys_info.model_str[i];
        }
        m->dev.model_name[i] = '\0';

        for (i = 0; i < sizeof(m->dev.username) - 1 && sys_info.user_name[i]; i++) {
            m->dev.username[i] = sys_info.user_name[i];
        }
        m->dev.username[i] = '\0';

        if (m->dev.username[0] != '\0') {
            m->status.user = m->dev.username;
        }
    } else {
        m->dev.fw_version[0] = '1';
        m->dev.fw_version[1] = '2';
        m->dev.fw_version[2] = '.';
        m->dev.fw_version[3] = '4';
        m->dev.fw_version[4] = '0';
        m->dev.fw_version[5] = '\0';
        m->dev.model_name[0] = 'C';
        m->dev.model_name[1] = 'F';
        m->dev.model_name[2] = 'I';
        m->dev.model_name[3] = '-';
        m->dev.model_name[4] = '1';
        m->dev.model_name[5] = '1';
        m->dev.model_name[6] = '1';
        m->dev.model_name[7] = '6';
        m->dev.model_name[8] = 'A';
        m->dev.model_name[9] = '\0';
    }

    oops_hw_info_t hw;
    if (oops_system_get_hw_info(&hw) == 0) {
        m->dev.cpu_temp_c = hw.cpu_temp_c;
        m->dev.soc_temp_c = hw.soc_temp_c;
        m->dev.fan_duty_pct = hw.fan_duty_pct;
    } else {
        m->dev.cpu_temp_c = -1;
        m->dev.soc_temp_c = -1;
        m->dev.fan_duty_pct = -1;
    }

    m->dev.pltauth_active = oops_system_check_pltauth();

    oops_snprintf(m->dev.console_info_str, sizeof(m->dev.console_info_str),
                  "%s (FW %s)", m->dev.model_name, m->dev.fw_version);

    if (m->dev.cpu_temp_c >= 0) {
        oops_snprintf(m->dev.hw_telemetry_str, sizeof(m->dev.hw_telemetry_str),
                      "CPU %d C | FAN %d%%", m->dev.cpu_temp_c, m->dev.fan_duty_pct);
    } else {
        oops_snprintf(m->dev.hw_telemetry_str, sizeof(m->dev.hw_telemetry_str),
                      "TELEMETRY ACTIVE");
    }

    if (m->dev.pltauth_active) {
        oops_snprintf(m->dev.pltauth_str, sizeof(m->dev.pltauth_str),
                      "ACTIVE (PLTAUTH BYPASS)");
    } else {
        oops_snprintf(m->dev.pltauth_str, sizeof(m->dev.pltauth_str),
                      "UNPATCHED (SYSTEM APP 65536)");
    }

    build_system_menu(m);
    build_developer_menu(m);
}

/* ---- model initialization
 * --------------------------------------------------------------- */

/* Zeroes the whole model, padding and unused array tails included, because
 * home_model_digest() hashes every byte; then sets the navigation state. */
static void init_state(home_model_t *m) {
    {
        unsigned char *raw = (unsigned char *)m;
        for (size_t i = 0; i < sizeof(*m); i++) {
            raw[i] = 0;
        }
    }
    for (int s = 0; s < HOME_SCREEN_COUNT; s++) {
        menu_reset(&m->menus[s], 0);
    }
    m->depth = 1;
    m->stack[0] = HOME_SCREEN_GAMES;
    m->mode = HOME_MODE_GAMES;
    m->top_nav = HOME_TOP_NAV_NONE;

    m->title_count = 0;
    m->title_cursor = 0;
    m->selected_title = 0;
    m->media_count = 0;
    m->media_cursor = 0;
    m->activity_count = 0;
    m->card_count = 0;
    m->card_cursor = 0;
    m->notice_count = 0;
    m->friend_count = 0;
    m->save_count = 0;
    m->capture_count = 0;
    m->skin_idx = 0;
    m->theme = 0;
    const home_skin_t *init_skin = home_skin_at(0);
    m->category_idx = init_skin ? init_skin->default_category : 0;
    for (int i = 0; i < HOME_MAX_SKIN_CATEGORIES; i++) {
        m->category_cursor[i] = 0;
    }

    m->host.ctx = 0;
    m->host.perform = 0;
    m->activate_count = 0;
    m->last_action = HOME_ACTION_NONE;
    m->last_arg = 0;
    m->last_refused = 0;
}

/* The library (host builds only; the payload scans storage), the media apps and the
 * control centre's dock. */
static void init_catalogue(home_model_t *m) {
#ifdef OOPS_HOST_BUILD
    /* Baseline test titles for host selftest */
    static const home_title_t default_titles[] = {
        {"PPSA01325", "ASTRO'S PLAYROOM", "NATIVE", "1.004.000", 11400, 1, 0, 0, 0, 0,
         NULL, 0, 0, 0},
        {"PPSA01342", "DEMON'S SOULS", "NATIVE", "1.002.000", 66200, 1, 0, 0, 0, 0,
         NULL, 0, 0, 0},
        {"PPSA01284", "RETURNAL", "NATIVE", "1.003.000", 56100, 1, 0, 0, 0, 0, NULL, 0,
         0, 0},
        {"PPSA01521", "HORIZON", "NATIVE", "1.018.000", 98400, 1, 0, 0, 0, 0, NULL, 0,
         0, 0},
        {"OOPS00001", "OBSCENE PROBE", "ELF", "1.000.000", 4, 1, 0, 0, 0, 0, NULL, 0, 0,
         0},
        {"OOPS00002", "PORTHOLE", "ELF", "1.000.000", 2, 1, 0, 0, 0, 0, NULL, 0, 0, 0},
        {"CUSA00123", "BLOODBORNE", "LEGACY", "1.009.000", 32000, 1, 0, 0, 0, 0, NULL,
         0, 0, 0}};
    home_set_titles(m, default_titles,
                    (int)(sizeof(default_titles) / sizeof(default_titles[0])));
#else
    m->title_count = 0;
#endif

    /* Realistic Media apps */
    static const home_title_t default_media[] = {
        {"MEDIA001", "MEDIA PLAYER (USB & LOCAL)", "SYSTEM", "1.00.00", 120, 1, 60, 0,
         0, 0, NULL, 0, 0, 0},
        {"MEDIA002", "MEDIA GALLERY (CAPTURES)", "SYSTEM", "1.00.00", 85, 1, 30, 0, 0,
         0, NULL, 0, 0, 0},
        {"MEDIA003", "WEB BROWSER (WEBKIT)", "SYSTEM", "1.00.00", 42, 1, 90, 0, 0, 0,
         NULL, 0, 0, 0}};
    m->media_count = (int)(sizeof(default_media) / sizeof(default_media[0]));
    for (int i = 0; i < m->media_count && i < HOME_MAX_MEDIA; i++) {
        m->media[i] = default_media[i];
    }

    /* Activities: only real data, none by default */
    m->activity_count = 0;

    /* Control Centre 13-dock icons */
    static const home_card_t default_cards[] = {
        {"HOME", "RETURN TO SHELL", HOME_ACTION_OPEN, (int)HOME_SCREEN_GAMES},
        {"SWITCHER", "NOW PLAYING", HOME_ACTION_OPEN, (int)HOME_SCREEN_SWITCHER},
        {"NOTIFICATIONS", "0 UNREAD", HOME_ACTION_OPEN, (int)HOME_SCREEN_NOTIFICATIONS},
        {"GAME BASE", "NO FRIENDS ONLINE", HOME_ACTION_OPEN,
         (int)HOME_SCREEN_GAME_BASE},
        {"MUSIC", "NO AUDIO", HOME_ACTION_OPEN, (int)HOME_SCREEN_MUSIC},
        {"CAPTURES", "MEDIA GALLERY", HOME_ACTION_OPEN, (int)HOME_SCREEN_CAPTURES},
        {"ACCESSIBILITY", "QUICK TOGGLES", HOME_ACTION_NONE, 0},
        {"NETWORK", "CONNECTED (WI-FI)", HOME_ACTION_NONE, 0},
        {"SOUND", "HEADPHONES (80%)", HOME_ACTION_TOGGLE_SOUND, 0},
        {"MIC", "MUTED (ORANGE LED)", HOME_ACTION_TOGGLE_MIC, 0},
        {"ACCESSORIES", "CONTROLLER 1 (85%)", HOME_ACTION_OPEN,
         (int)HOME_SCREEN_SETTINGS_ACCESSORIES},
        {"PROFILE", "PLAYER (ONLINE)", HOME_ACTION_OPEN, (int)HOME_SCREEN_PROFILE},
        {"POWER", "REST / RESTART / OFF", HOME_ACTION_OPEN, (int)HOME_SCREEN_POWER}};
    m->card_count = (int)(sizeof(default_cards) / sizeof(default_cards[0]));
    for (int i = 0; i < m->card_count && i < HOME_MAX_CARDS; i++) {
        m->cards[i] = default_cards[i];
    }
}

/* Switcher, storage, developer, status bar, notifications, dialog and toast. */
static void init_status(home_model_t *m) {
    /* Friends, Saves, Captures: no fake data */
    m->friend_count = 0;
    m->save_count = 0;
    m->capture_count = 0;

    /* Switcher state: no fake running game */
    m->switcher.has_running_title = 0;
    m->switcher.running_title_index = 0;
    m->switcher.is_suspended = 0;
    m->switcher.recent_count = 0;

    /* Storage breakdown */
    m->storage.total_gb = 825;
    m->storage.games_gb = 412;
    m->storage.media_gb = 24;
    m->storage.saves_gb = 8;
    m->storage.system_gb = 56;
    m->storage.free_gb = 325;

    /* Developer state */
    m->dev.big_app_override = 0;
    m->dev.pltauth_active = 1;
    m->dev.save_state_slot = 1;
    m->dev.fps_overlay = 1;
    m->dev.frame_limit_60 = 1;
    m->dev.shaders_compiled = 1420;

    /* Status bar */
    m->status.hour = 20;
    m->status.minute = 15;
    m->status.user = "PLAYER";
    m->status.notifications = 0;
    m->status.storage_used_gb = 412;
    m->status.storage_total_gb = 825;
    m->status.network_up = 1;
    m->status.pad_battery = 85;

    /* Notifications */
    m->notices[0].text = "DOWNLOAD READY: RETURNAL PATCH 1.003";
    m->notices[0].unread = 1;
    m->notices[1].text = "TROPHY EARNED: FIRST STEP (ASTRO)";
    m->notices[1].unread = 1;
    m->notices[2].text = "SYSTEM SOFTWARE 12.40 UP TO DATE";
    m->notices[2].unread = 0;
    m->notice_count = 3;
    m->status.notifications = home_unread_count(m);

    /* Dialog and toast */
    m->dialog.type = HOME_DIALOG_NONE;
    m->dialog.title = 0;
    m->dialog.message = 0;
    m->dialog.progress_percent = 0;
    m->dialog.confirm_choice = 0;
    m->dialog.on_confirm = HOME_ACTION_NONE;
    m->dialog.on_confirm_arg = 0;
    m->dialog.ime_len = 0;
    m->dialog.ime_row = 1;
    m->dialog.ime_col = 0;
    m->dialog.error_code = 0;
    m->dialog.error_desc = 0;

    m->toast.active = 0;
    m->toast.frames_left = 0;
    m->toast.title = 0;
    m->toast.message = 0;
}

/* Every screen's menu. */
static void build_menus(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS];
    menu_reset(menu, "SETTINGS");
    menu_add(menu, "USERS AND ACCOUNTS", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_PROFILE,
             1);
    menu_add(menu, "SYSTEM", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_SYSTEM, 1);
    menu_add(menu, "STORAGE", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_STORAGE,
             1);
    menu_add(menu, "SOUND", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_SOUND, 1);
    menu_add(menu, "SCREEN AND VIDEO", 0, HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_VIDEO, 1);
    menu_add(menu, "ACCESSORIES", 0, HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_ACCESSORIES, 1);
    menu_add(menu, "SAVED DATA AND GAME/APP SETTINGS", 0, HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_SAVES, 1);
    menu_add(menu, "DEVELOPER & DEBUG SETTINGS", 0, HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_DEVELOPER, 1);
    menu_add(menu, "EMULATOR SETTINGS (ORBISTOUN)", 0, HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_EMULATOR, 1);
    menu_add(menu, "THEMES & SKINS", "SELECT ACTIVE SKIN", HOME_ACTION_OPEN,
             (int)HOME_SCREEN_SETTINGS_THEME, 1);
    menu_add(menu, "POWER", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_POWER, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    build_system_menu(m);

    menu = &m->menus[HOME_SCREEN_SETTINGS_SOUND];
    menu_reset(menu, "SOUND");
    menu_add(menu, "AUDIO OUTPUT", "HEADPHONES / STEREO", HOME_ACTION_TOGGLE_SOUND, 0,
             1);
    menu_add(menu, "3D AUDIO FOR HEADPHONES", "ENABLED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "VOLUME", "80%", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "MICROPHONE", "MUTED BY DEFAULT", HOME_ACTION_TOGGLE_MIC, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_SETTINGS_VIDEO];
    menu_reset(menu, "SCREEN AND VIDEO");
    menu_add(menu, "RESOLUTION", "3840X2160 (4K UHD)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "REFRESH RATE", "60HZ / 120HZ AUTO", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "VRR", "AUTOMATIC", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "HDR", "ON WHEN SUPPORTED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "ADJUST DISPLAY AREA", "FULL DISPLAY AREA", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_SETTINGS_ACCESSORIES];
    menu_reset(menu, "ACCESSORIES");
    menu_add(menu, "CONTROLLER 1", "WIRELESS CONTROLLER (85%)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "COMMUNICATION METHOD", "USB / BLUETOOTH DUAL", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "VIBRATION INTENSITY", "STRONG (STANDARD)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "TRIGGER EFFECT INTENSITY", "STRONG (STANDARD)", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "INDICATOR BRIGHTNESS", "MEDIUM", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    build_developer_menu(m);
    build_theme_menu(m);

    menu = &m->menus[HOME_SCREEN_SETTINGS_EMULATOR];
    menu_reset(menu, "EMULATOR SETTINGS");
    menu_add(menu, "SAVE STATE SLOT", "SLOT 1", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "QUICK SAVE STATE", "SLOT 1", HOME_ACTION_SAVE_STATE, 0, 1);
    menu_add(menu, "QUICK LOAD STATE", "SLOT 1", HOME_ACTION_LOAD_STATE, 0, 1);
    menu_add(menu, "FRAME LIMITER", "60 FPS (VSYNC ON)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "RESOLUTION SCALING", "100% NATIVE (1280X720 / 4K)",
             HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "SHADER CACHE", "1,420 SHADERS COMPILED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "PERFORMANCE HUD", "FPS & FRAME TIME OVERLAY", HOME_ACTION_NONE, 0,
             1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_POWER];
    menu_reset(menu, "POWER");
    menu_add(menu, "ENTER REST MODE", 0, HOME_ACTION_REST_MODE, 0, 1);
    menu_add(menu, "RESTART", 0, HOME_ACTION_RESTART, 0, 1);
    menu_add(menu, "TURN OFF", 0, HOME_ACTION_POWER_OFF, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_PROFILE];
    menu_reset(menu, "PROFILE");
    menu_add(menu, "ONLINE STATUS", "OFFLINE (LAN ONLY)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "TROPHIES", "NO TROPHY DATA", HOME_ACTION_NONE, 0, 0);
    menu_add(menu, "SWITCH USER", 0, HOME_ACTION_SWITCH_USER, 0, 1);
    menu_add(menu, "LOG OUT", 0, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_NOTIFICATIONS];
    menu_reset(menu, "NOTIFICATIONS");
    for (int i = 0; i < m->notice_count; i++) {
        menu_add(menu, m->notices[i].text, m->notices[i].unread ? "NEW" : 0,
                 HOME_ACTION_DISMISS_NOTICE, i, m->notices[i].unread);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    menu = &m->menus[HOME_SCREEN_CONTROL];
    menu_reset(menu, "CONTROL CENTRE");
    for (int i = 0; i < m->card_count; i++) {
        menu_add(menu, m->cards[i].label, m->cards[i].detail, m->cards[i].action,
                 m->cards[i].arg, 1);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    build_title_options_menu(m);
    build_title_info_menu(m);
    build_switcher_menu(m);
    build_game_base_menu(m);
    build_music_menu(m);
    build_captures_menu(m);
    build_saves_menu(m);
    build_storage_menu(m);
    build_library_menu(m);
    build_search_menu(m);
}

void home_model_init(home_model_t *m) {
    if (m == 0) {
        return;
    }
    init_state(m);
    init_catalogue(m);
    init_status(m);
    home_refresh_telemetry(m);
    build_menus(m);
}

void home_set_host(home_model_t *m, const home_host_t *host) {
    if (m == 0) {
        return;
    }
    if (host == 0) {
        m->host.ctx = 0;
        m->host.perform = 0;
        return;
    }
    m->host = *host;
}

void home_set_titles(home_model_t *m, const home_title_t *titles, int count) {
    if (m == 0) {
        return;
    }
    if (titles == 0 || count < 0) {
        count = 0;
    }
    if (count > HOME_MAX_TITLES) {
        count = HOME_MAX_TITLES;
    }
    for (int i = 0; i < count; i++) {
        m->titles[i] = titles[i];
    }
    m->title_count = count;
    if (m->title_cursor >= count) {
        m->title_cursor = (count > 0) ? (count - 1) : 0;
    }
    if (m->selected_title >= count) {
        m->selected_title = (count > 0) ? (count - 1) : 0;
    }
    build_library_menu(m);
    build_search_menu(m);
}

int home_unread_count(const home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    int unread = 0;
    for (int i = 0; i < m->notice_count; i++) {
        if (m->notices[i].unread != 0) {
            unread++;
        }
    }
    return unread;
}

/* ---- screen navigation
 * ------------------------------------------------------------------ */

home_screen_t home_screen(const home_model_t *m) {
    if (m == 0 || m->depth <= 0) {
        return HOME_SCREEN_GAMES;
    }
    return m->stack[m->depth - 1];
}

void home_open(home_model_t *m, home_screen_t screen) {
    if (m == 0) {
        return;
    }
    if ((int)screen < 0 || (int)screen >= HOME_SCREEN_COUNT) {
        return;
    }
    if (screen == HOME_SCREEN_TITLE_OPTIONS) {
        build_title_options_menu(m);
    } else if (screen == HOME_SCREEN_TITLE_INFO) {
        build_title_info_menu(m);
    } else if (screen == HOME_SCREEN_SWITCHER) {
        build_switcher_menu(m);
    } else if (screen == HOME_SCREEN_LIBRARY) {
        build_library_menu(m);
    } else if (screen == HOME_SCREEN_SEARCH) {
        build_search_menu(m);
    } else if (screen == HOME_SCREEN_SETTINGS_THEME) {
        build_theme_menu(m);
    }

    if (m->depth >= HOME_NAV_DEPTH) {
        for (int i = 1; i < HOME_NAV_DEPTH; i++) {
            m->stack[i - 1] = m->stack[i];
        }
        m->depth = HOME_NAV_DEPTH - 1;
    }
    m->stack[m->depth] = screen;
    m->depth++;
}

int home_back(home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    if (m->dialog.type != HOME_DIALOG_NONE) {
        home_close_dialog(m);
        return 1;
    }
    const home_skin_t *skin = home_current_skin(m);
    if (skin && skin->back) {
        if (skin->back(m, skin)) {
            return 1;
        }
    }
    if (m->depth <= 1) {
        return 0;
    }
    m->depth--;
    return 1;
}

void home_toggle_control_centre(home_model_t *m) {
    if (m == 0) {
        return;
    }
    if (home_screen(m) == HOME_SCREEN_CONTROL) {
        (void)home_back(m);
    } else {
        home_open(m, HOME_SCREEN_CONTROL);
    }
}

void home_switch_mode(home_model_t *m, home_mode_t mode) {
    if (m == 0) {
        return;
    }
    m->mode = mode;
    if (m->depth == 1) {
        m->stack[0] = (mode == HOME_MODE_GAMES) ? HOME_SCREEN_GAMES : HOME_SCREEN_MEDIA;
    }
}

home_menu_t *home_current_menu(home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    home_screen_t screen = home_screen(m);
    if (screen_is_carousel(screen)) {
        return 0;
    }
    return &m->menus[screen];
}

const home_menu_t *home_current_menu_const(const home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    home_screen_t screen = home_screen(m);
    if (screen_is_carousel(screen)) {
        return 0;
    }
    return &m->menus[screen];
}

const home_title_t *home_selected_title(const home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    if (m->mode == HOME_MODE_MEDIA) {
        if (m->media_count <= 0 || m->media_cursor < 0 ||
            m->media_cursor >= m->media_count) {
            return 0;
        }
        return &m->media[m->media_cursor];
    }
    if (m->title_count <= 0 || m->title_cursor < 0 ||
        m->title_cursor >= m->title_count) {
        return 0;
    }
    return &m->titles[m->title_cursor];
}

const home_item_t *home_selected_item(const home_model_t *m) {
    const home_menu_t *menu = home_current_menu_const(m);
    if (menu == 0 || menu->count <= 0) {
        return 0;
    }
    if (menu->cursor < 0 || menu->cursor >= menu->count) {
        return 0;
    }
    return &menu->items[menu->cursor];
}

/* ---- dialogs and notifications
 * ---------------------------------------------------------- */

void home_show_dialog(home_model_t *m, home_dialog_type_t type, const char *title,
                      const char *msg) {
    if (m == 0) {
        return;
    }
    m->dialog.type = type;
    m->dialog.title = title;
    m->dialog.message = msg;
    m->dialog.progress_percent = (type == HOME_DIALOG_PROGRESS) ? 45 : 0;
    m->dialog.confirm_choice = 0;
    m->dialog.on_confirm = HOME_ACTION_NONE;
    m->dialog.on_confirm_arg = 0;
    m->dialog.ime_len = 0;
    m->dialog.ime_buffer[0] = '\0';
    m->dialog.ime_row = 1;
    m->dialog.ime_col = 0;
    m->dialog.error_code = 0;
    m->dialog.error_desc = 0;
}

void home_show_error(home_model_t *m, const char *code, const char *desc) {
    if (m == 0) {
        return;
    }
    home_show_dialog(m, HOME_DIALOG_ERROR, "AN ERROR HAS OCCURRED", desc);
    m->dialog.error_code = code;
    m->dialog.error_desc = desc;
}

void home_close_dialog(home_model_t *m) {
    if (m == 0) {
        return;
    }
    m->dialog.type = HOME_DIALOG_NONE;
}

void home_show_toast(home_model_t *m, const char *title, const char *msg) {
    if (m == 0) {
        return;
    }
    m->toast.active = 1;
    m->toast.frames_left = 180; /* 3 seconds at 60 FPS */
    m->toast.title = title;
    m->toast.message = msg;
}

void home_tick(home_model_t *m) {
    if (m == 0) {
        return;
    }
    if (m->toast.active != 0 && m->toast.frames_left > 0) {
        m->toast.frames_left--;
        if (m->toast.frames_left <= 0) {
            m->toast.active = 0;
        }
    }
    const home_skin_t *skin = home_current_skin(m);
    if (skin && skin->tick) {
        skin->tick(m, skin);
    }
}

/*
 * FNV-1a over the whole model.
 *
 * Hashing every byte rather than a chosen list of fields is the point: the
 * renderer reads from most of this structure, a caller that skips a frame on
 * this digest is trusting it completely, and a list is a thing someone forgets
 * to extend when they add a field. The whole struct cannot miss a change. It
 * can only be conservative - two different models colliding is a 2^-64 event,
 * and anything that perturbs a byte the renderer ignores costs one redraw.
 *
 * Everything the renderer reads is either in here or constant for the run: the
 * theme is a static table indexed by m->theme, the surface extent does not
 * change, and title icon pixels are filled in before the first frame and never
 * again. Add anything that breaks that and this stops being sufficient.
 *
 * The trap runs the other way too. home_refresh_telemetry() writes CPU
 * temperature and fan duty into the model, and it is called once, from
 * home_model_init(). Call it from the frame loop instead and the digest changes
 * whenever a fan speed does - the shell would redraw continuously and nothing
 * would look wrong, because the output is correct and merely wasteful. If those
 * readings should update live, give them their own cadence and let the redraw
 * follow from that, rather than refreshing every frame.
 *
 * ~18 KB a frame, against 1920x1080 of clear and swizzle - under half a percent
 * of the work it decides whether to skip.
 */
uint64_t home_model_digest(const home_model_t *m) {
    if (m == 0) {
        return 0u;
    }
    const unsigned char *raw = (const unsigned char *)m;
    uint64_t h = 1469598103934665603ull; /* FNV-1a 64-bit offset basis */
    for (size_t i = 0; i < sizeof(*m); i++) {
        h ^= (uint64_t)raw[i];
        h *= 1099511628211ull;
    }
    return h;
}

/* ---- navigation movement
 * ---------------------------------------------------------------- */

static void move_ime(home_dialog_t *dlg, home_direction_t dir) {
    if (dir == HOME_UP && dlg->ime_row > 0) {
        dlg->ime_row--;
    } else if (dir == HOME_DOWN && dlg->ime_row < 4) {
        dlg->ime_row++;
    } else if (dir == HOME_LEFT && dlg->ime_col > 0) {
        dlg->ime_col--;
    } else if (dir == HOME_RIGHT && dlg->ime_col < 10) {
        dlg->ime_col++;
    }
}

/* ---- generic category data providers
 * --------------------------------------------------- */

int home_get_category_item_count(const home_model_t *m, const home_skin_t *skin,
                                 int cat_idx) {
    if (m == 0 || skin == 0)
        return 0;
    if (skin->item_count) {
        int custom = skin->item_count(m, skin, cat_idx);
        if (custom >= 0)
            return custom;
    }
    if (cat_idx < 0 || cat_idx >= skin->category_count)
        return 0;

    home_category_type_t type = skin->categories[cat_idx].type;
    switch (type) {
    case HOME_CAT_GAMES:
        return (m->title_count > 0) ? m->title_count : 1;
    case HOME_CAT_MEDIA:
        return (m->media_count > 0) ? m->media_count : 1;
    case HOME_CAT_SETTINGS:
        return 11;
    case HOME_CAT_USERS:
        return 3;
    case HOME_CAT_PHOTO:
        return (m->capture_count > 0) ? m->capture_count : 1;
    case HOME_CAT_MUSIC:
        return 3;
    case HOME_CAT_NETWORK:
        return 3;
    case HOME_CAT_CUSTOM:
    default:
        return 4;
    }
}

void home_get_category_item_info(const home_model_t *m, const home_skin_t *skin,
                                 int cat_idx, int item_idx, const char **out_name,
                                 const char **out_sub, const home_title_t **out_title,
                                 char *out_badge) {
    if (out_name)
        *out_name = "";
    if (out_sub)
        *out_sub = "";
    if (out_title)
        *out_title = 0;
    if (out_badge)
        *out_badge = 'G';

    if (m == 0 || skin == 0 || cat_idx < 0 || cat_idx >= skin->category_count)
        return;

    home_category_type_t type = skin->categories[cat_idx].type;
    switch (type) {
    case HOME_CAT_GAMES:
        if (m->title_count > 0 && item_idx >= 0 && item_idx < m->title_count) {
            if (out_title)
                *out_title = &m->titles[item_idx];
            if (out_name)
                *out_name = m->titles[item_idx].name ? m->titles[item_idx].name
                                                     : m->titles[item_idx].id;
            if (out_sub)
                *out_sub = (m->titles[item_idx].category &&
                            m->titles[item_idx].category[0] == 'P')
                               ? "First-Party"
                               : "Community Homebrew";
            if (out_badge) {
                if (m->titles[item_idx].category &&
                    m->titles[item_idx].category[0] == 'O') {
                    *out_badge = 'M';
                } else if (m->titles[item_idx].category &&
                           m->titles[item_idx].category[0] == 'E') {
                    *out_badge = 'H';
                } else {
                    *out_badge = 'G';
                }
            }
        } else {
            if (out_name)
                *out_name = "NO TITLES INSTALLED";
            if (out_sub)
                *out_sub = "Install games via Developer Settings";
            if (out_badge)
                *out_badge = 'E';
        }
        break;
    case HOME_CAT_MEDIA:
        if (m->media_count > 0 && item_idx >= 0 && item_idx < m->media_count) {
            if (out_title)
                *out_title = &m->media[item_idx];
            if (out_name)
                *out_name = m->media[item_idx].name ? m->media[item_idx].name
                                                    : m->media[item_idx].id;
            if (out_sub)
                *out_sub = "Media Application";
            if (out_badge)
                *out_badge = 'G';
        } else {
            if (out_name)
                *out_name = "MEDIA PLAYER";
            if (out_sub)
                *out_sub = "USB & Local Media Playback";
            if (out_badge)
                *out_badge = 'G';
        }
        break;
    case HOME_CAT_SETTINGS: {
        static const char *s_st_names[] = {
            "System Settings",   "Storage Manager",     "Audio & Sound",
            "Video & Display",   "Controllers & Input", "Save Data Management",
            "Developer & Debug", "Emulator Options",    "Themes & Skins",
            "Power Options",     "User Profiles"};
        static const char *s_st_subs[] = {
            "Console information, firmware & HDMI",
            "Visual storage breakdown & content manager",
            "Audio output device, 3D audio & volume",
            "Resolution, refresh rate & HDR",
            "Controllers, input devices & haptics",
            "Save data management, backup & delete",
            "Package installer, payload runner & klog",
            "Save states, FPS overlay & host sync",
            "Select active dashboard skin and visual theme",
            "Enter rest mode, restart, or power off",
            "Trophies, profile status & avatar"};
        if (item_idx >= 0 && item_idx < 11) {
            if (out_name)
                *out_name = s_st_names[item_idx];
            if (out_sub)
                *out_sub = s_st_subs[item_idx];
        }
        if (out_badge)
            *out_badge = 'E';
        break;
    }
    case HOME_CAT_USERS: {
        static const char *s_u_names[] = {"User Profile", "Switch User",
                                          "Power Options"};
        static const char *s_u_subs[] = {"Trophies, profile status & avatar",
                                         "Log into another user profile",
                                         "Enter rest mode, restart, or power off"};
        if (item_idx >= 0 && item_idx < 3) {
            if (out_name)
                *out_name = s_u_names[item_idx];
            if (out_sub)
                *out_sub = s_u_subs[item_idx];
        }
        if (out_badge)
            *out_badge = 'G';
        break;
    }
    case HOME_CAT_PHOTO:
        if (m->capture_count > 0 && item_idx >= 0 && item_idx < m->capture_count) {
            if (out_name)
                *out_name = m->captures[item_idx].title ? m->captures[item_idx].title
                                                        : "Screenshot";
            if (out_sub)
                *out_sub = m->captures[item_idx].timestamp
                               ? m->captures[item_idx].timestamp
                               : "Photo Gallery";
        } else {
            if (out_name)
                *out_name = "Photo Gallery";
            if (out_sub)
                *out_sub = "Screenshots and captured images";
        }
        if (out_badge)
            *out_badge = 'G';
        break;
    case HOME_CAT_MUSIC: {
        static const char *s_m_names[] = {"Background Audio", "USB Music Player",
                                          "Audio Mixer"};
        static const char *s_m_subs[] = {"Now playing & playback controls",
                                         "Play audio files from USB storage",
                                         "3D audio profile & volume mixer"};
        if (item_idx >= 0 && item_idx < 3) {
            if (out_name)
                *out_name = s_m_names[item_idx];
            if (out_sub)
                *out_sub = s_m_subs[item_idx];
        }
        if (out_badge)
            *out_badge = 'G';
        break;
    }
    case HOME_CAT_NETWORK: {
        static const char *s_n_names[] = {"Universal Search", "Game Library",
                                          "Notifications"};
        static const char *s_n_subs[] = {"Search installed games and homebrew",
                                         "Browse entire collection & storage",
                                         "System alerts, downloads & notices"};
        if (item_idx >= 0 && item_idx < 3) {
            if (out_name)
                *out_name = s_n_names[item_idx];
            if (out_sub)
                *out_sub = s_n_subs[item_idx];
        }
        if (out_badge)
            *out_badge = 'G';
        break;
    }
    case HOME_CAT_CUSTOM:
    default: {
        static const char *s_l_names[] = {"Game Library", "Friends & Parties",
                                          "Universal Search", "Active Downloads"};
        static const char *s_l_subs[] = {
            "Browse full installed collection", "Online players & voice parties",
            "Search marketplace & apps", "Background package installs"};
        if (item_idx >= 0 && item_idx < 4) {
            if (out_name)
                *out_name = s_l_names[item_idx];
            if (out_sub)
                *out_sub = s_l_subs[item_idx];
        }
        if (out_badge)
            *out_badge = 'G';
        break;
    }
    }
}

int home_generic_move(home_model_t *m, const home_skin_t *skin, int dir) {
    if (m == 0 || skin == 0)
        return 0;
    if (skin->move) {
        if (skin->move(m, skin, dir)) {
            return 1;
        }
    }

    int cat = m->category_idx;
    int cur = m->category_cursor[cat];
    int count = home_get_category_item_count(m, skin, cat);

    /* Primary Axis Navigation (category switching) */
    if ((skin->primary_axis == HOME_AXIS_HORIZONTAL &&
         (dir == HOME_LEFT || dir == HOME_RIGHT)) ||
        (skin->primary_axis == HOME_AXIS_VERTICAL &&
         (dir == HOME_UP || dir == HOME_DOWN))) {
        int delta = (dir == HOME_RIGHT || dir == HOME_DOWN) ? 1 : -1;
        if (skin->wrap_categories) {
            m->category_idx =
                (m->category_idx + skin->category_count + delta) % skin->category_count;
        } else {
            int n = m->category_idx + delta;
            if (n >= 0 && n < skin->category_count)
                m->category_idx = n;
        }
    }
    /* Item Axis Navigation (item scrolling within category) */
    else if ((skin->item_axis == HOME_AXIS_VERTICAL &&
              (dir == HOME_UP || dir == HOME_DOWN)) ||
             (skin->item_axis == HOME_AXIS_HORIZONTAL &&
              (dir == HOME_LEFT || dir == HOME_RIGHT))) {
        int delta = (dir == HOME_DOWN || dir == HOME_RIGHT) ? 1 : -1;
        if (count > 0) {
            if (skin->wrap_items) {
                m->category_cursor[cat] = (cur + count + delta) % count;
            } else {
                int n = cur + delta;
                if (n >= 0 && n < count)
                    m->category_cursor[cat] = n;
            }
        }
    } else if (dir == HOME_UP) {
        m->top_nav = HOME_TOP_NAV_TABS;
        return 1;
    }

    /* Sync title_cursor & media_cursor */
    home_category_type_t cur_type = skin->categories[m->category_idx].type;
    if (cur_type == HOME_CAT_GAMES) {
        m->title_cursor = m->category_cursor[m->category_idx];
    } else if (cur_type == HOME_CAT_MEDIA) {
        m->media_cursor = m->category_cursor[m->category_idx];
    }
    return 1;
}

void home_move(home_model_t *m, home_direction_t dir) {
    if (m == 0) {
        return;
    }
    /* Dialog navigation intercepts d-pad */
    if (m->dialog.type != HOME_DIALOG_NONE) {
        if (m->dialog.type == HOME_DIALOG_CONFIRM) {
            if (dir == HOME_LEFT) {
                m->dialog.confirm_choice = 0; /* Cancel */
            } else if (dir == HOME_RIGHT) {
                m->dialog.confirm_choice = 1; /* Confirm */
            }
        } else if (m->dialog.type == HOME_DIALOG_IME) {
            move_ime(&m->dialog, dir);
        }
        return;
    }

    home_screen_t screen = home_screen(m);

    /* Control centre dock navigation */
    if (screen == HOME_SCREEN_CONTROL) {
        if (dir == HOME_LEFT && m->card_cursor > 0) {
            m->card_cursor--;
        } else if (dir == HOME_RIGHT && (m->card_cursor + 1) < m->card_count) {
            m->card_cursor++;
        }
        return;
    }

    /* Top bar navigation */
    if (m->top_nav != HOME_TOP_NAV_NONE) {
        if (dir == HOME_DOWN) {
            m->top_nav = HOME_TOP_NAV_NONE;
            return;
        }
        if (dir == HOME_LEFT) {
            if (m->top_nav > HOME_TOP_NAV_TABS) {
                m->top_nav = (home_top_nav_t)((int)m->top_nav - 1);
            }
        } else if (dir == HOME_RIGHT) {
            if (m->top_nav < HOME_TOP_NAV_PROFILE) {
                m->top_nav = (home_top_nav_t)((int)m->top_nav + 1);
            }
        }
        return;
    }

    /* Carousel navigation delegated to generic skin navigation */
    if (screen_is_carousel(screen)) {
        const home_skin_t *skin = home_current_skin(m);
        home_generic_move(m, skin, (int)dir);
        return;
    }

    /* Menu screens */
    home_menu_t *menu = home_current_menu(m);
    if (menu == 0 || menu->count <= 0) {
        return;
    }
    if (dir == HOME_UP && menu->cursor > 0) {
        menu->cursor--;
    } else if (dir == HOME_DOWN && (menu->cursor + 1) < menu->count) {
        menu->cursor++;
    }
}

/* ---- action execution
 * ------------------------------------------------------------------- */

static int perform(home_model_t *m, home_action_t action, int arg) {
    m->last_action = action;
    m->last_arg = arg;
    m->last_refused = 0;

    switch (action) {
    case HOME_ACTION_NONE:
        return 0;
    case HOME_ACTION_OPEN:
        home_open(m, (home_screen_t)arg);
        return 1;
    case HOME_ACTION_BACK:
        return home_back(m);
    case HOME_ACTION_NEXT_THEME:
        home_next_theme(m);
        build_theme_menu(m);
        if (m->host.perform) {
            (void)m->host.perform(m->host.ctx, action, m->skin_idx);
        }
        return 1;
    case HOME_ACTION_SET_THEME:
        home_set_skin(m, arg);
        build_theme_menu(m);
        if (m->host.perform) {
            (void)m->host.perform(m->host.ctx, action, arg);
        }
        return 1;
    case HOME_ACTION_TOGGLE_MODE:
        home_switch_mode(m, (m->mode == HOME_MODE_GAMES) ? HOME_MODE_MEDIA
                                                         : HOME_MODE_GAMES);
        if (m->host.perform) {
            (void)m->host.perform(m->host.ctx, action, (int)m->mode);
        }
        return 1;
    case HOME_ACTION_DISMISS_NOTICE:
        if (arg >= 0 && arg < m->notice_count) {
            m->notices[arg].unread = 0;
            m->status.notifications = home_unread_count(m);
            return 1;
        }
        return 0;
    case HOME_ACTION_TRIGGER_DIALOG:
        home_show_dialog(m, (home_dialog_type_t)arg, "SYSTEM DIALOG",
                         "ACTION REQUIRED");
        return 1;
    case HOME_ACTION_CLOSE_DIALOG:
        home_close_dialog(m);
        return 1;
    case HOME_ACTION_CONFIRM_DIALOG:
        home_close_dialog(m);
        home_show_toast(m, "SUCCESS", "OPERATION COMPLETED");
        return 1;
    case HOME_ACTION_TOGGLE_FAVORITE:
        if (arg >= 0 && arg < m->title_count) {
            m->titles[arg].favorite = !m->titles[arg].favorite;
            if (m->titles[arg].favorite) {
                home_show_toast(m, "FAVORITES", "PINNED TO FAVORITES");
            } else {
                home_show_toast(m, "FAVORITES", "UNPINNED FROM FAVORITES");
            }
            build_title_options_menu(m);
            build_library_menu(m);
            if (m->host.perform) {
                (void)m->host.perform(m->host.ctx, action, arg);
            }
            return 1;
        }
        return 0;
    case HOME_ACTION_SET_LIBRARY_FILTER: {
        int next_filter = ((int)m->library_filter + 1) % HOME_FILTER_COUNT;
        m->library_filter = (home_filter_t)next_filter;
        build_library_menu(m);
        return 1;
    }
    default:
        break;
    }

    if (action == HOME_ACTION_LAUNCH_TITLE && arg >= 0 && arg < m->title_count) {
        m->switcher.has_running_title = 1;
        m->switcher.running_title_index = arg;
        m->switcher.is_suspended = 0;
        if (m->switcher.recent_count == 0 || m->switcher.recent_indices[0] != arg) {
            for (int r = 2; r > 0; r--) {
                m->switcher.recent_indices[r] = m->switcher.recent_indices[r - 1];
            }
            m->switcher.recent_indices[0] = arg;
            if (m->switcher.recent_count < 3)
                m->switcher.recent_count++;
        }
        build_switcher_menu(m);
    } else if (action == HOME_ACTION_SUSPEND_TITLE) {
        m->switcher.is_suspended = 1;
        build_switcher_menu(m);
    } else if (action == HOME_ACTION_RESUME_TITLE) {
        m->switcher.is_suspended = 0;
        build_switcher_menu(m);
    } else if (action == HOME_ACTION_TERMINATE_TITLE) {
        m->switcher.has_running_title = 0;
        m->switcher.is_suspended = 0;
        build_switcher_menu(m);
    }

    if (m->host.perform == 0) {
        m->last_refused = 1;
        return 0;
    }
    int handled = m->host.perform(m->host.ctx, action, arg);
    if (handled == 0) {
        m->last_refused = 1;
    }
    return handled;
}

static const char s_ime_grid[4][12] = {"1234567890-", "QWERTYUIOP.", "ASDFGHJKL_/",
                                       "ZXCVBNM@:!?"};

static int activate_ime(home_model_t *m) {
    home_dialog_t *dlg = &m->dialog;
    if (dlg->ime_row < 4) {
        char ch = s_ime_grid[dlg->ime_row][dlg->ime_col];
        if (ch != '\0' && dlg->ime_len < 32) {
            dlg->ime_buffer[dlg->ime_len] = ch;
            dlg->ime_len++;
            dlg->ime_buffer[dlg->ime_len] = '\0';
            return 1;
        }
    } else {
        /* Bottom row: Space (0-2), Backspace (3-4), Clear (5-6), Done (7-10) */
        if (dlg->ime_col < 3 && dlg->ime_len < 32) {
            dlg->ime_buffer[dlg->ime_len] = ' ';
            dlg->ime_len++;
            dlg->ime_buffer[dlg->ime_len] = '\0';
            return 1;
        } else if (dlg->ime_col < 5) {
            if (dlg->ime_len > 0) {
                dlg->ime_len--;
                dlg->ime_buffer[dlg->ime_len] = '\0';
            }
            return 1;
        } else if (dlg->ime_col < 7) {
            dlg->ime_len = 0;
            dlg->ime_buffer[0] = '\0';
            return 1;
        } else {
            size_t l = 0;
            for (l = 0; l < sizeof(m->last_search) - 1 && dlg->ime_buffer[l]; l++) {
                m->last_search[l] = dlg->ime_buffer[l];
            }
            m->last_search[l] = '\0';
            home_show_toast(m, "SEARCH",
                            (dlg->ime_len > 0) ? dlg->ime_buffer : "ALL TITLES");
            home_close_dialog(m);
            build_search_menu(m);
            home_open(m, HOME_SCREEN_SEARCH);
            return 1;
        }
    }
    return 0;
}

int home_category_item_activate(home_model_t *m, const home_skin_t *skin, int cat_idx,
                                int item_idx) {
    if (m == 0 || skin == 0 || cat_idx < 0 || cat_idx >= skin->category_count)
        return 0;
    home_category_type_t type = skin->categories[cat_idx].type;

    switch (type) {
    case HOME_CAT_GAMES: {
        const home_title_t *title = home_selected_title(m);
        if (title == 0)
            return 0;
        m->selected_title = m->title_cursor;
        home_open(m, HOME_SCREEN_TITLE_OPTIONS);
        return 1;
    }
    case HOME_CAT_MEDIA: {
        if (m->media_cursor >= 0 && m->media_cursor < m->media_count) {
            home_show_toast(m, "MEDIA APP", m->media[m->media_cursor].name);
            return 1;
        }
        return 0;
    }
    case HOME_CAT_SETTINGS: {
        static const home_screen_t s_st_screens[] = {HOME_SCREEN_SETTINGS_SYSTEM,
                                                     HOME_SCREEN_SETTINGS_STORAGE,
                                                     HOME_SCREEN_SETTINGS_SOUND,
                                                     HOME_SCREEN_SETTINGS_VIDEO,
                                                     HOME_SCREEN_SETTINGS_ACCESSORIES,
                                                     HOME_SCREEN_SETTINGS_SAVES,
                                                     HOME_SCREEN_SETTINGS_DEVELOPER,
                                                     HOME_SCREEN_SETTINGS_EMULATOR,
                                                     HOME_SCREEN_SETTINGS_THEME,
                                                     HOME_SCREEN_POWER,
                                                     HOME_SCREEN_PROFILE};
        if (item_idx >= 0 && item_idx < 11) {
            home_open(m, s_st_screens[item_idx]);
            return 1;
        }
        return 0;
    }
    case HOME_CAT_USERS: {
        if (item_idx == 0) {
            home_open(m, HOME_SCREEN_PROFILE);
        } else if (item_idx == 1) {
            return perform(m, HOME_ACTION_SWITCH_USER, 0);
        } else {
            home_open(m, HOME_SCREEN_POWER);
        }
        return 1;
    }
    case HOME_CAT_PHOTO:
        home_open(m, HOME_SCREEN_CAPTURES);
        return 1;
    case HOME_CAT_MUSIC:
        home_open(m, HOME_SCREEN_MUSIC);
        return 1;
    case HOME_CAT_NETWORK: {
        if (item_idx == 0) {
            home_open(m, HOME_SCREEN_SEARCH);
        } else if (item_idx == 1) {
            home_open(m, HOME_SCREEN_LIBRARY);
        } else {
            home_open(m, HOME_SCREEN_NOTIFICATIONS);
        }
        return 1;
    }
    case HOME_CAT_CUSTOM:
    default: {
        if (item_idx == 0) {
            home_open(m, HOME_SCREEN_LIBRARY);
        } else if (item_idx == 1) {
            home_open(m, HOME_SCREEN_GAME_BASE);
        } else if (item_idx == 2) {
            home_open(m, HOME_SCREEN_SEARCH);
        } else {
            home_open(m, HOME_SCREEN_NOTIFICATIONS);
        }
        return 1;
    }
    }
}

int home_activate(home_model_t *m) {
    if (m == 0) {
        return 0;
    }
    m->activate_count++;

    /* Dialog interaction */
    if (m->dialog.type != HOME_DIALOG_NONE) {
        if (m->dialog.type == HOME_DIALOG_CONFIRM) {
            if (m->dialog.confirm_choice == 1) {
                return perform(m, HOME_ACTION_CONFIRM_DIALOG, 0);
            }
            home_close_dialog(m);
            return 1;
        } else if (m->dialog.type == HOME_DIALOG_PROGRESS ||
                   m->dialog.type == HOME_DIALOG_ERROR) {
            home_close_dialog(m);
            return 1;
        } else if (m->dialog.type == HOME_DIALOG_IME) {
            return activate_ime(m);
        }
        return 0;
    }

    /* Top bar interaction */
    if (m->top_nav != HOME_TOP_NAV_NONE) {
        if (m->top_nav == HOME_TOP_NAV_TABS) {
            home_switch_mode(m, (m->mode == HOME_MODE_GAMES) ? HOME_MODE_MEDIA
                                                             : HOME_MODE_GAMES);
            return 1;
        } else if (m->top_nav == HOME_TOP_NAV_SEARCH) {
            home_open(m, HOME_SCREEN_SEARCH);
            return 1;
        } else if (m->top_nav == HOME_TOP_NAV_SETTINGS) {
            home_open(m, HOME_SCREEN_SETTINGS);
            return 1;
        } else if (m->top_nav == HOME_TOP_NAV_PROFILE) {
            home_open(m, HOME_SCREEN_PROFILE);
            return 1;
        }
        return 0;
    }

    home_screen_t screen = home_screen(m);

    /* Carousel activation opens context menu */
    /* Carousel activation */
    if (screen_is_carousel(screen)) {
        const home_skin_t *skin = home_current_skin(m);
        if (skin && skin->activate) {
            if (skin->activate(m, skin)) {
                return 1;
            }
        }
        return home_category_item_activate(m, skin, m->category_idx,
                                           m->category_cursor[m->category_idx]);
    }

    /* Control centre dock activation */
    if (screen == HOME_SCREEN_CONTROL) {
        if (m->card_cursor >= 0 && m->card_cursor < m->card_count) {
            home_card_t *card = &m->cards[m->card_cursor];
            return perform(m, card->action, card->arg);
        }
        return 0;
    }

    /* Menu item activation */
    const home_item_t *item = home_selected_item(m);
    if (item == 0 || item->enabled == 0) {
        return 0;
    }
    return perform(m, item->action, item->arg);
}

/* ---- cursor calculation
 * ----------------------------------------------------------------- */

static int content_top(const home_theme_t *theme) {
    return theme->margin_y + (theme->row_height * 2);
}

int home_cursor_rect(const home_model_t *m, const home_theme_t *theme, int *x, int *y,
                     int *w, int *h) {
    if (m == 0 || theme == 0 || x == 0 || y == 0 || w == 0 || h == 0) {
        return 0;
    }
    home_screen_t screen = home_screen(m);

    if (screen_is_carousel(screen)) {
        const home_skin_t *skin = home_current_skin(m);
        if (skin && skin->get_cursor_rect) {
            skin->get_cursor_rect(m, skin, x, y, w, h);
        } else {
            int cursor = m->category_cursor[m->category_idx];
            int step = theme->tile_width + (theme->margin_x / 2);
            int offset_x = (cursor > 3) ? ((cursor - 3) * step) : 0;
            *x = theme->margin_x + (cursor * step) - offset_x;
            *y = content_top(theme);
            *w = theme->tile_width;
            *h = theme->tile_height + (theme->row_height / 2);
        }

        /* Clamp bounds safely within 1280x720 surface */
        if (*x < 0)
            *x = 0;
        if (*y < 0)
            *y = 0;
        if (*x + *w > 1280)
            *w = 1280 - *x;
        if (*y + *h > 720)
            *h = 720 - *y;
        return 1;
    }

    const home_menu_t *menu = home_current_menu_const(m);
    if (menu == 0 || menu->count <= 0) {
        return 0;
    }
    *x = theme->margin_x;
    *y = content_top(theme) + (menu->cursor * theme->row_height);
    *w = 1280 - (theme->margin_x * 2);
    *h = theme->row_height;
    if (*x < 0)
        *x = 0;
    if (*y < 0)
        *y = 0;
    if (*x + *w > 1280)
        *w = 1280 - *x;
    if (*y + *h > 720)
        *h = 720 - *y;
    return 1;
}

/* ---- renderer
 * --------------------------------------------------------------------------- */

void home_draw_title_icon(oops_surface_t *surf, const home_title_t *title, int ix,
                          int iy, int iw, int ih, const home_theme_t *theme) {
    if (surf == 0 || theme == 0 || iw <= 0 || ih <= 0)
        return;
    if (title != 0 && title->icon_pixels != 0 && title->icon_width > 0 &&
        title->icon_height > 0) {
        oops_surface_t isurf;
        isurf.pixels = (uint32_t *)title->icon_pixels;
        isurf.width = (uint32_t)title->icon_width;
        isurf.height = (uint32_t)title->icon_height;
        isurf.pitch = (uint32_t)title->icon_width;
        isurf.layout = OOPS_SURFACE_LINEAR;
        oops_draw_blit_scaled_blend(surf, ix, iy, iw, ih, &isurf, 0, 0,
                                    title->icon_width, title->icon_height);
    } else {
        char letter[2];
        letter[0] = '?';
        letter[1] = '\0';
        if (title != 0 && title->name != 0) {
            const char *nm = title->name;
            while (*nm != '\0') {
                if ((*nm >= 'A' && *nm <= 'Z') || (*nm >= '0' && *nm <= '9')) {
                    letter[0] = *nm;
                    break;
                }
                if (*nm >= 'a' && *nm <= 'z') {
                    letter[0] = (char)(*nm - 32);
                    break;
                }
                nm++;
            }
        }
        if (letter[0] == '?' && title != 0 && title->id != 0 && title->id[0] != '\0') {
            letter[0] = title->id[0];
        }
        int scale = (iw >= 64) ? 3 : ((iw >= 36) ? 2 : 1);
        (void)oops_draw_text(surf, ix + (iw / 2) - (4 * scale),
                             iy + (ih / 2) - (4 * scale), letter, theme->text, scale);
    }
}

void home_draw_cursor(oops_surface_t *surf, int x, int y, int w, int h,
                      const home_theme_t *theme) {
    if (surf == 0 || theme == 0)
        return;
    if (theme->cursor_style == HOME_CURSOR_BOX) {
        oops_draw_rect(surf, x - 3, y - 3, w + 6, 3, theme->cursor);
        oops_draw_rect(surf, x - 3, y + h, w + 6, 3, theme->cursor);
        oops_draw_rect(surf, x - 3, y - 3, 3, h + 6, theme->cursor);
        oops_draw_rect(surf, x + w, y - 3, 3, h + 6, theme->cursor);
    } else if (theme->cursor_style == HOME_CURSOR_UNDERLINE) {
        oops_draw_rect(surf, x, y + h + 2, w, 4, theme->cursor);
    } else {
        oops_draw_rect_blend(surf, x, y, w, h,
                             (theme->cursor & 0x00FFFFFFu) | 0x44000000u);
        oops_draw_rect(surf, x, y, 4, h, theme->accent);
    }
}

static int render_control_centre(oops_surface_t *surf, const home_model_t *m,
                                 const home_theme_t *theme) {
    int drawn = 0;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    /* Dark translucent overlay across screen */
    oops_draw_rect_blend(surf, 0, 0, sw, sh, 0xDD0A0E16u);

    /* Upper Activity Cards */
    int uy = sh - 250;
    oops_draw_rect(surf, theme->margin_x, uy, 400, 90, theme->panel);
    (void)oops_draw_text(surf, theme->margin_x + 16, uy + 16, "NOW PLAYING",
                         theme->accent, 1);
    if (m->switcher.has_running_title && m->switcher.running_title_index >= 0 &&
        m->switcher.running_title_index < m->title_count) {
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 36,
                             m->titles[m->switcher.running_title_index].name,
                             theme->text, 2);
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 64,
                             "PRESS OPTIONS TO CLOSE", theme->text_dim, 1);
    } else {
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 36, "NO TITLE ACTIVE",
                             theme->text, 2);
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 64,
                             "SELECT A GAME TO PLAY", theme->text_dim, 1);
    }
    drawn += 4;

    /* Bottom 13-dock icons bar */
    int dock_y = sh - 110;
    int card_w = 80;
    int card_gap = 12;
    int start_x = theme->margin_x;

    for (int i = 0; i < m->card_count; i++) {
        int cx = start_x + (i * (card_w + card_gap));
        if (cx + card_w > sw) {
            break;
        }
        int selected = (i == m->card_cursor);
        oops_color_t bg = selected ? theme->accent : theme->panel;
        oops_draw_rect(surf, cx, dock_y, card_w, 60, bg);

        /* Icon letter */
        char letter[2];
        letter[0] = m->cards[i].label[0];
        letter[1] = '\0';
        (void)oops_draw_text(surf, cx + (card_w / 2) - 8, dock_y + 14, letter,
                             selected ? theme->background : theme->text, 2);

        /* Label beneath icon */
        (void)oops_draw_text(surf, cx + 4, dock_y + 42, m->cards[i].label,
                             selected ? theme->background : theme->text_dim, 1);
        drawn += 3;
    }
    return drawn;
}

static int render_storage(oops_surface_t *surf, const home_model_t *m,
                          const home_theme_t *theme) {
    int drawn = 0;
    int y = content_top(theme);

    (void)oops_draw_text(surf, theme->margin_x, y, "STORAGE BREAKDOWN", theme->text, 2);
    y += 40;

    /* Storage bar meter */
    int bar_w = (int)surf->width - (theme->margin_x * 2);
    int bar_h = 32;
    int bx = theme->margin_x;
    int total = m->storage.total_gb;
    if (total <= 0) {
        total = 825;
    }

    int gw = (bar_w * m->storage.games_gb) / total;
    int mw = (bar_w * m->storage.media_gb) / total;
    int sw_w = (bar_w * m->storage.saves_gb) / total;
    int ow = (bar_w * m->storage.system_gb) / total;
    int fw = bar_w - gw - mw - sw_w - ow;

    oops_draw_rect(surf, bx, y, gw, bar_h, 0xFF3D8BFDu); /* Games: Blue */
    bx += gw;
    oops_draw_rect(surf, bx, y, mw, bar_h, 0xFF4CAF50u); /* Media: Green */
    bx += mw;
    oops_draw_rect(surf, bx, y, sw_w, bar_h, 0xFFFFC107u); /* Saves: Amber */
    bx += sw_w;
    oops_draw_rect(surf, bx, y, ow, bar_h, 0xFF9E9E9Eu); /* Other: Grey */
    bx += ow;
    oops_draw_rect(surf, bx, y, fw, bar_h, 0xFF21252Bu); /* Free: Dark */
    y += 50;

    /* Legend */
    (void)oops_draw_text(surf, theme->margin_x, y, "GAMES & APPS: 412 GB", 0xFF3D8BFDu,
                         1);
    (void)oops_draw_text(surf, theme->margin_x + 220, y, "MEDIA: 24 GB", 0xFF4CAF50u,
                         1);
    (void)oops_draw_text(surf, theme->margin_x + 380, y, "SAVES: 8 GB", 0xFFFFC107u, 1);
    (void)oops_draw_text(surf, theme->margin_x + 520, y, "OTHER: 56 GB", 0xFF9E9E9Eu,
                         1);
    (void)oops_draw_text(surf, theme->margin_x + 680, y, "FREE: 325 GB",
                         theme->text_dim, 1);
    y += 50;

    /* Drives list */
    const home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_STORAGE];
    for (int i = 0; i < menu->count; i++) {
        int selected = (i == menu->cursor);
        oops_color_t col = selected ? theme->accent : theme->text;
        if (selected) {
            oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, theme->row_height,
                           theme->panel);
        }
        (void)oops_draw_text(surf, theme->margin_x + 12, y, menu->items[i].label, col,
                             1);
        if (menu->items[i].detail != 0) {
            (void)oops_draw_text(surf, theme->margin_x + 400, y, menu->items[i].detail,
                                 theme->text_dim, 1);
        }
        y += theme->row_height;
        drawn += 2;
    }
    return drawn + 8;
}

static int render_menu(oops_surface_t *surf, const home_model_t *m,
                       const home_theme_t *theme) {
    int drawn = 0;
    const home_menu_t *menu = home_current_menu_const(m);
    if (menu == 0) {
        return 0;
    }
    int y = content_top(theme);
    int bar_w = (int)surf->width - (theme->margin_x * 2);

    if (menu->heading != 0) {
        (void)oops_draw_text(surf, theme->margin_x, y, menu->heading, theme->text, 2);
        y += 40;
        drawn++;
    }

    for (int i = 0; i < menu->count; i++) {
        int selected = (i == menu->cursor);
        oops_color_t col =
            selected ? theme->accent
                     : (menu->items[i].enabled ? theme->text : theme->text_dim);
        if (selected) {
            if (theme->cursor_style == HOME_CURSOR_BOX) {
                oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, theme->row_height,
                               theme->panel);
                oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, 2, theme->cursor);
                oops_draw_rect(surf, theme->margin_x, y - 4 + theme->row_height - 2,
                               bar_w, 2, theme->cursor);
                oops_draw_rect(surf, theme->margin_x, y - 4, 2, theme->row_height,
                               theme->cursor);
                oops_draw_rect(surf, theme->margin_x + bar_w - 2, y - 4, 2,
                               theme->row_height, theme->cursor);
            } else if (theme->cursor_style == HOME_CURSOR_UNDERLINE) {
                oops_draw_rect(surf, theme->margin_x, y - 4 + theme->row_height - 2,
                               bar_w, 2, theme->cursor);
            } else {
                oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, theme->row_height,
                               theme->panel);
                oops_draw_rect(surf, theme->margin_x, y - 4, 4, theme->row_height,
                               theme->accent);
            }
        }
        (void)oops_draw_text(surf, theme->margin_x + 16, y, menu->items[i].label, col,
                             1);
        if (menu->items[i].detail != 0) {
            (void)oops_draw_text(surf, theme->margin_x + 420, y, menu->items[i].detail,
                                 theme->text_dim, 1);
        }
        y += theme->row_height;
        drawn += 2;
    }
    return drawn;
}

static void render_dialog(oops_surface_t *surf, const home_model_t *m,
                          const home_theme_t *theme) {
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    /* Dim full screen */
    oops_draw_rect_blend(surf, 0, 0, sw, sh, 0xEE0A0E14u);

    int dw = 600;
    int dh = 320;
    int dx = (sw - dw) / 2;
    int dy = (sh - dh) / 2;

    /* Dialog window */
    oops_draw_rect(surf, dx, dy, dw, dh, theme->panel);
    oops_draw_rect(surf, dx, dy, dw, 4,
                   (m->dialog.type == HOME_DIALOG_ERROR) ? 0xFFE53935u : theme->accent);

    int cy = dy + 30;
    if (m->dialog.title != 0) {
        (void)oops_draw_text(surf, dx + 30, cy, m->dialog.title, theme->text, 2);
        cy += 40;
    }

    if (m->dialog.type == HOME_DIALOG_CONFIRM) {
        if (m->dialog.message != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.message, theme->text_dim,
                                 1);
        }
        int by = dy + dh - 60;
        int cancel_sel = (m->dialog.confirm_choice == 0);
        oops_draw_rect(surf, dx + 60, by, 180, 40,
                       cancel_sel ? theme->accent : theme->background);
        (void)oops_draw_text(surf, dx + 110, by + 12, "CANCEL",
                             cancel_sel ? theme->background : theme->text, 1);

        int ok_sel = (m->dialog.confirm_choice == 1);
        oops_draw_rect(surf, dx + 360, by, 180, 40,
                       ok_sel ? theme->accent : theme->background);
        (void)oops_draw_text(surf, dx + 420, by + 12, "OK",
                             ok_sel ? theme->background : theme->text, 1);

    } else if (m->dialog.type == HOME_DIALOG_PROGRESS) {
        if (m->dialog.message != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.message, theme->text_dim,
                                 1);
        }
        cy += 40;
        int pw = dw - 60;
        oops_draw_rect(surf, dx + 30, cy, pw, 16, theme->background);
        int pfill = (pw * m->dialog.progress_percent) / 100;
        oops_draw_rect(surf, dx + 30, cy, pfill, 16, theme->accent);

        char pbuf[8];
        pbuf[0] = (char)('0' + (m->dialog.progress_percent / 10));
        pbuf[1] = (char)('0' + (m->dialog.progress_percent % 10));
        pbuf[2] = '%';
        pbuf[3] = '\0';
        (void)oops_draw_text(surf, dx + (dw / 2) - 16, cy + 30, pbuf, theme->text, 1);

    } else if (m->dialog.type == HOME_DIALOG_ERROR) {
        if (m->dialog.error_code != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.error_code, 0xFFE53935u,
                                 2);
            cy += 36;
        }
        if (m->dialog.error_desc != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.error_desc,
                                 theme->text_dim, 1);
        }
        int by = dy + dh - 60;
        oops_draw_rect(surf, dx + 210, by, 180, 40, theme->accent);
        (void)oops_draw_text(surf, dx + 280, by + 12, "OK", theme->background, 1);

    } else if (m->dialog.type == HOME_DIALOG_IME) {
        /* Input text display box */
        oops_draw_rect(surf, dx + 30, cy, dw - 60, 36, theme->background);
        (void)oops_draw_text(
            surf, dx + 40, cy + 10,
            (m->dialog.ime_len > 0) ? m->dialog.ime_buffer : "TYPE HERE...",
            (m->dialog.ime_len > 0) ? theme->text : theme->text_dim, 1);
        cy += 50;

        /* 4-row keyboard grid */
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 11; c++) {
                int kx = dx + 35 + (c * 48);
                int ky = cy + (r * 32);
                int sel = (m->dialog.ime_row == r && m->dialog.ime_col == c);
                if (sel) {
                    oops_draw_rect(surf, kx - 4, ky - 4, 40, 28, theme->accent);
                }
                char kch[2];
                kch[0] = s_ime_grid[r][c];
                kch[1] = '\0';
                (void)oops_draw_text(surf, kx + 8, ky, kch,
                                     sel ? theme->background : theme->text, 1);
            }
        }
        /* Bottom action keys */
        int by = cy + 135;
        int sel_act = (m->dialog.ime_row == 4);
        (void)oops_draw_text(
            surf, dx + 40, by, "[SPACE]",
            (sel_act && m->dialog.ime_col < 3) ? theme->accent : theme->text_dim, 1);
        (void)oops_draw_text(
            surf, dx + 150, by, "[BACKSPACE]",
            (sel_act && m->dialog.ime_col >= 3 && m->dialog.ime_col < 5)
                ? theme->accent
                : theme->text_dim,
            1);
        (void)oops_draw_text(
            surf, dx + 300, by, "[CLEAR]",
            (sel_act && m->dialog.ime_col >= 5 && m->dialog.ime_col < 7)
                ? theme->accent
                : theme->text_dim,
            1);
        (void)oops_draw_text(
            surf, dx + 420, by, "[DONE]",
            (sel_act && m->dialog.ime_col >= 7) ? theme->accent : theme->text_dim, 1);
    }
}

static void render_toast(oops_surface_t *surf, const home_model_t *m,
                         const home_theme_t *theme) {
    if (m->toast.active == 0) {
        return;
    }
    int tw = 380;
    int th = 70;
    int tx = (int)surf->width - tw - theme->margin_x;
    int ty = theme->margin_y + 40;

    oops_draw_rect(surf, tx, ty, tw, th, theme->panel);
    oops_draw_rect(surf, tx, ty, 4, th, theme->accent);
    if (m->toast.title != 0) {
        (void)oops_draw_text(surf, tx + 16, ty + 12, m->toast.title, theme->accent, 1);
    }
    if (m->toast.message != 0) {
        (void)oops_draw_text(surf, tx + 16, ty + 36, m->toast.message, theme->text, 1);
    }
}

int home_render(oops_surface_t *surf, const home_model_t *m,
                const home_theme_t *theme) {
    if (surf == 0 || surf->pixels == 0 || m == 0 || theme == 0) {
        return 0;
    }
    const home_skin_t *skin = home_current_skin(m);

    if (skin && skin->render_background) {
        skin->render_background(surf, m, skin);
    } else {
        oops_draw_clear(surf, theme->background);
    }

    int drawn = 5;
    home_screen_t screen = home_screen(m);

    if (screen == HOME_SCREEN_GAMES || screen == HOME_SCREEN_MEDIA) {
        if (skin && skin->render_main) {
            drawn += skin->render_main(surf, m, skin);
        }
    } else if (screen == HOME_SCREEN_SETTINGS_STORAGE) {
        drawn += render_storage(surf, m, theme);
    } else {
        drawn += render_menu(surf, m, theme);
    }

    if (screen == HOME_SCREEN_CONTROL) {
        drawn += render_control_centre(surf, m, theme);
    }

    if (m->dialog.type != HOME_DIALOG_NONE) {
        render_dialog(surf, m, theme);
        drawn += 10;
    }

    render_toast(surf, m, theme);

    if (m->last_refused != 0) {
        int footer_y = (int)surf->height - theme->margin_y;
        (void)oops_draw_text(surf, theme->margin_x, footer_y - theme->row_height,
                             "NOT AVAILABLE HERE", theme->text_dim, theme->text_scale);
        drawn++;
    }
    return drawn;
}

/* ---- pad input
 * -------------------------------------------------------------------------- */

void home_input_reset(home_input_t *in) {
    if (in == 0) {
        return;
    }
    in->previous = 0u;
    in->primed = 0;
    in->held_frames = 0;
    in->held_dir = 0u;
}

static uint32_t direction_down(uint32_t buttons) {
    if ((buttons & OOPS_BUTTON_UP) != 0u) {
        return OOPS_BUTTON_UP;
    }
    if ((buttons & OOPS_BUTTON_DOWN) != 0u) {
        return OOPS_BUTTON_DOWN;
    }
    if ((buttons & OOPS_BUTTON_LEFT) != 0u) {
        return OOPS_BUTTON_LEFT;
    }
    if ((buttons & OOPS_BUTTON_RIGHT) != 0u) {
        return OOPS_BUTTON_RIGHT;
    }
    return 0u;
}

static void move_for(home_model_t *m, uint32_t bit) {
    if (bit == OOPS_BUTTON_UP) {
        home_move(m, HOME_UP);
    } else if (bit == OOPS_BUTTON_DOWN) {
        home_move(m, HOME_DOWN);
    } else if (bit == OOPS_BUTTON_LEFT) {
        home_move(m, HOME_LEFT);
    } else if (bit == OOPS_BUTTON_RIGHT) {
        home_move(m, HOME_RIGHT);
    }
}

int home_input_apply(home_input_t *in, home_model_t *m, uint32_t buttons) {
    if (in == 0 || m == 0) {
        return 0;
    }
    if (in->primed == 0) {
        in->previous = buttons;
        in->primed = 1;
        in->held_dir = direction_down(buttons);
        in->held_frames = 0;
        return 0;
    }

    uint32_t previous = in->previous;
    int changed = 0;

    /* PS button toggles Control Centre */
    if (app_pressed(buttons, previous, HOME_BUTTON_PS)) {
        home_toggle_control_centre(m);
        changed = 1;
    }

    /* Mode switching / Bumper navigation via L1 / R1 */
    if (app_pressed(buttons, previous, OOPS_BUTTON_L1)) {
        const home_skin_t *skin = home_current_skin(m);
        if (skin && skin->bumper_nav) {
            home_move(m, HOME_LEFT);
        } else {
            home_switch_mode(m, HOME_MODE_GAMES);
        }
        changed = 1;
    }
    if (app_pressed(buttons, previous, OOPS_BUTTON_R1)) {
        const home_skin_t *skin = home_current_skin(m);
        if (skin && skin->bumper_nav) {
            home_move(m, HOME_RIGHT);
        } else {
            home_switch_mode(m, HOME_MODE_MEDIA);
        }
        changed = 1;
    }

    /* Directional navigation: prioritize newly pressed directions over sustained hold
     */
    uint32_t new_dir = direction_down(buttons & ~previous);
    uint32_t dir = (new_dir != 0u) ? new_dir : direction_down(buttons);
    if (dir == 0u) {
        in->held_dir = 0u;
        in->held_frames = 0;
    } else if (dir != in->held_dir) {
        oops_log_debug("INPUT", "apply: dir change 0x%x (screen=%d, cur=%d)",
                       (unsigned int)dir, (int)home_screen(m), m->title_cursor);
        in->held_dir = dir;
        in->held_frames = 0;
        move_for(m, dir);
        changed = 1;
    } else {
        in->held_frames++;
        if (in->held_frames >= HOME_REPEAT_DELAY_FRAMES) {
            int since = in->held_frames - HOME_REPEAT_DELAY_FRAMES;
            int stride = (in->held_frames >= 45) ? HOME_REPEAT_FAST_FRAMES
                                                 : HOME_REPEAT_EVERY_FRAMES;
            if ((since % stride) == 0) {
                move_for(m, dir);
                changed = 1;
            }
        }
    }

    /* Face buttons */
    if (app_pressed(buttons, previous, OOPS_BUTTON_CROSS)) {
        oops_log_debug("INPUT", "apply: CROSS pressed (screen=%d, cur=%d)",
                       (int)home_screen(m), m->title_cursor);
        if (home_activate(m) != 0) {
            changed = 1;
        }
    }
    if (app_pressed(buttons, previous, OOPS_BUTTON_CIRCLE)) {
        oops_log_debug("INPUT", "apply: CIRCLE pressed (screen=%d, depth=%d)",
                       (int)home_screen(m), m->depth);
        if (home_back(m) != 0) {
            changed = 1;
        }
    }
    if (app_pressed(buttons, previous, OOPS_BUTTON_SQUARE)) {
        oops_log_debug("INPUT", "apply: SQUARE pressed (opening library)");
        home_open(m, HOME_SCREEN_LIBRARY);
        changed = 1;
    }
    if (app_pressed(buttons, previous, OOPS_BUTTON_TRIANGLE)) {
        oops_log_debug("INPUT", "apply: TRIANGLE pressed (opening search)");
        home_open(m, HOME_SCREEN_SEARCH);
        changed = 1;
    }
    if (app_pressed(buttons, previous, OOPS_BUTTON_OPTIONS)) {
        home_screen_t screen = home_screen(m);
        oops_log_debug("INPUT", "apply: OPTIONS pressed (screen=%d)", (int)screen);
        if (screen_is_carousel(screen)) {
            m->selected_title = m->title_cursor;
            home_open(m, HOME_SCREEN_TITLE_OPTIONS);
            changed = 1;
        }
    }

    in->previous = buttons;
    return changed;
}
