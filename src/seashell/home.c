/*
 * home - clean-room reimplementation of the Prospero shell (Prospero UX / SceShellCore)
 * for homebrew and the Orbistoun emulator.
 *
 * Shared between the host self-test and the payload, so nothing here may call libc, allocate, or
 * touch a machine. Everything is a pure function of the model, a theme and a surface; the things
 * that are not pure go out through the host dispatch in home.h.
 */

#include "home.h"
#include "oops/system.h"
#include "oops/freestd.h"

/* ---- the skins, which are data --------------------------------------------------------- */

static const home_theme_t s_themes[] = {
    {
        "MODERN",        /* Prospero UX: tiles over a detail strip */
        0xFF10141Cu, 0xFF3D8BFDu, 0xFFF2F5FAu, 0xFF7C8493u, 0xFF3D8BFDu, 0xFF1B212Cu,
        200, 56, 132, 132, 28, 48, 64, 2,
        HOME_LAYOUT_TILES, HOME_CURSOR_BOX
    },
    {
        "XMB",           /* Cross Media Bar (PS3 / PSP) */
        0xFF0B1020u, 0xFF4C8DFFu, 0xFFF0F4FFu, 0xFF7A85A8u, 0xFF4C8DFFu, 0xFF161C30u,
        200, 56, 120, 120, 28, 48, 56, 2,
        HOME_LAYOUT_XMB, HOME_CURSOR_BAR
    },
    {
        "BLADES",        /* The 360 dashboard */
        0xFF0E2A12u, 0xFF7BC043u, 0xFFEFF7E8u, 0xFF5E7A50u, 0xFF7BC043u, 0xFF163A1Cu,
        200, 56, 120, 120, 26, 40, 44, 2,
        HOME_LAYOUT_BLADES, HOME_CURSOR_BAR
    },
    {
        "MEMCARD",       /* The PS1 browser */
        0xFF20242Cu, 0xFFC8CDD6u, 0xFFEDEFF3u, 0xFF767C88u, 0xFFC8CDD6u, 0xFF2B3038u,
        180, 48, 110, 110, 24, 40, 40, 2,
        HOME_LAYOUT_LIST, HOME_CURSOR_BOX
    },
    {
        "BROWSER",       /* The PS2 browser */
        0xFF05070Cu, 0xFF5AA9E6u, 0xFFDCE6F2u, 0xFF4A5566u, 0xFF5AA9E6u, 0xFF0C1119u,
        190, 48, 110, 110, 30, 44, 52, 2,
        HOME_LAYOUT_LIST, HOME_CURSOR_UNDERLINE
    },
    {
        "AMBER",         /* Custom amber theme */
        0xFF120C04u, 0xFFFFB13Cu, 0xFFFFE9C7u, 0xFF8A6A38u, 0xFFFFB13Cu, 0xFF1E1408u,
        220, 52, 120, 120, 30, 56, 64, 2,
        HOME_LAYOUT_XMB, HOME_CURSOR_UNDERLINE
    }
};

int home_theme_count(void) {
    return (int)(sizeof(s_themes) / sizeof(s_themes[0]));
}

const home_theme_t *home_theme_at(int index) {
    if (index < 0 || index >= home_theme_count()) {
        return &s_themes[0];
    }
    return &s_themes[index];
}

/* ---- menu building ---------------------------------------------------------------------- */

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
    menu_add(menu, "PLAY", title->id, HOME_ACTION_LAUNCH_TITLE, index, title->installed);
    menu_add(menu, "CHECK FOR UPDATE", "VERSION 1.002.000", HOME_ACTION_CHECK_UPDATE, index, 1);
    menu_add(menu, "MANAGE GAME CONTENT", "1 ADD-ON INSTALLED", HOME_ACTION_MANAGE_CONTENT, index, 1);
    menu_add(menu, "SAVED DATA", "SYNC WITH CLOUD / USB", HOME_ACTION_SYNC_SAVE, index, 1);
    menu_add(menu, "INFORMATION", "VIEW METADATA & SPECS", HOME_ACTION_OPEN, (int)HOME_SCREEN_TITLE_INFO, 1);
    menu_add(menu, "DELETE", "REMOVE FROM STORAGE", HOME_ACTION_DELETE_TITLE, index, title->installed);
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
    menu_add(menu, "AUDIO FORMAT", "LINEAR PCM 7.1 / TEMPEST 3D", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "PARENTAL LEVEL", "LEVEL 1 (ALL AGES)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_switcher_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SWITCHER];
    menu_reset(menu, "SWITCHER");
    if (m->switcher.has_running_title != 0 &&
        m->switcher.running_title_index >= 0 &&
        m->switcher.running_title_index < m->title_count) {
        const home_title_t *running = &m->titles[m->switcher.running_title_index];
        menu_add(menu, "NOW PLAYING", running->name, HOME_ACTION_NONE, 0, 1);
        menu_add(menu, "SWITCH TO GAME", "RESUME EXECUTION", HOME_ACTION_RESUME_TITLE, 0, 1);
        menu_add(menu, "CLOSE GAME", "TERMINATE PROCESS", HOME_ACTION_TERMINATE_TITLE, 0, 1);
    } else {
        menu_add(menu, "NO GAME RUNNING", "SELECT A TITLE TO LAUNCH", HOME_ACTION_NONE, 0, 0);
    }
    for (int i = 0; i < m->switcher.recent_count; i++) {
        int idx = m->switcher.recent_indices[i];
        if (idx >= 0 && idx < m->title_count) {
            menu_add(menu, "RECENT", m->titles[idx].name, HOME_ACTION_LAUNCH_TITLE, idx, 1);
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
        menu_add(menu, m->captures[i].title, m->captures[i].type_and_res, HOME_ACTION_NONE, 0, 1);
    }
    menu_add(menu, "TAKE SCREENSHOT", "CAPTURE CURRENT FRAME", HOME_ACTION_TAKE_SCREENSHOT, 0, 1);
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
    menu_add(menu, "CONSOLE STORAGE (SSD)", "412 GB / 825 GB USED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "M.2 SSD STORAGE", "550 GB / 2000 GB USED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "USB EXTENDED STORAGE", "190 GB / 500 GB USED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "EMULATOR HOST SHARE", "DIRECT FILESYSTEM MOUNT", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "AUTO CLEANUP UNUSED CACHES", 0, HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_library_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_LIBRARY];
    menu_reset(menu, "GAME LIBRARY");
    for (int i = 0; i < m->title_count; i++) {
        menu_add(menu, m->titles[i].name, m->titles[i].category, HOME_ACTION_LAUNCH_TITLE, i, 1);
    }
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_system_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_SYSTEM];
    menu_reset(menu, "SYSTEM");
    menu_add(menu, "CONSOLE INFORMATION", m->dev.console_info_str[0] ? m->dev.console_info_str : "PROSPERO (FW 12.40)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "HARDWARE TELEMETRY", m->dev.hw_telemetry_str[0] ? m->dev.hw_telemetry_str : "CPU -- C | FAN --%", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "SYSTEM SOFTWARE UPDATE", "CHECK AUTOMATICALLY", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "POWER SAVING", "REST MODE IN 1 HOUR", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "HDMI", "HDMI DEVICE LINK ENABLED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "REMOTE PLAY", "ENABLED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "RESET OPTIONS", "REBUILD DATABASE / CLEAR CACHE", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

static void build_developer_menu(home_model_t *m) {
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_DEVELOPER];
    menu_reset(menu, "DEVELOPER & DEBUG");
    menu_add(menu, "INSTALL PACKAGE (PKG)", "SCAN USB & /DATA/PKG", HOME_ACTION_INSTALL_PACKAGE, 0, 1);
    menu_add(menu, "RUN PAYLOAD (ELF)", "SCAN USB & /DATA/PAYLOADS", HOME_ACTION_RUN_PAYLOAD, 0, 1);
    menu_add(menu, "APP CATEGORY OVERRIDE", "BIG APP 0 (PROSPERO)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "PLTAUTH STATUS", m->dev.pltauth_str[0] ? m->dev.pltauth_str : "ACTIVE", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "LIVE KERNEL LOG (KLOG)", "VIEW SYSTEM LOG STREAM", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "FILESYSTEM BROWSER", "EXPLORE /APP0 /DATA /USER", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "DEV TEST: CONFIRM DIALOG", 0, HOME_ACTION_TRIGGER_DIALOG, (int)HOME_DIALOG_CONFIRM, 1);
    menu_add(menu, "DEV TEST: PROGRESS DIALOG", 0, HOME_ACTION_TRIGGER_DIALOG, (int)HOME_DIALOG_PROGRESS, 1);
    menu_add(menu, "DEV TEST: ERROR CE-108255-1", 0, HOME_ACTION_TRIGGER_DIALOG, (int)HOME_DIALOG_ERROR, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);
}

void home_refresh_telemetry(home_model_t *m) {
    if (m == 0) return;

    oops_system_info_t sys_info;
    if (oops_system_get_info(&sys_info) == 0) {
        m->dev.total_ram_mb = (int)sys_info.total_ram_mb;
        m->dev.direct_mem_mb = (int)sys_info.direct_mem_mb;
        size_t i = 0;
        for (i = 0; i < sizeof(m->dev.fw_version) - 1 && sys_info.firmware_str[i]; i++) {
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
        m->dev.fw_version[0] = '1'; m->dev.fw_version[1] = '2'; m->dev.fw_version[2] = '.';
        m->dev.fw_version[3] = '4'; m->dev.fw_version[4] = '0'; m->dev.fw_version[5] = '\0';
        m->dev.model_name[0] = 'C'; m->dev.model_name[1] = 'F'; m->dev.model_name[2] = 'I';
        m->dev.model_name[3] = '-'; m->dev.model_name[4] = '1'; m->dev.model_name[5] = '1';
        m->dev.model_name[6] = '1'; m->dev.model_name[7] = '6'; m->dev.model_name[8] = 'A';
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

/* ---- model initialization --------------------------------------------------------------- */

void home_model_init(home_model_t *m) {
    if (m == 0) {
        return;
    }
    /* Zero first, then set. Callers hold the model as a local, so everything
     * this function does not reach - padding, the tail of titles[] past
     * title_count, the unused depth of stack[] - would otherwise keep whatever
     * was on the stack. Nothing reads those (every loop is bounded by its
     * count), but home_model_digest() hashes the whole struct, and a digest
     * over indeterminate bytes is only as stable as the bytes happen to be. */
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
    m->theme = 0;

    m->host.ctx = 0;
    m->host.perform = 0;
    m->activate_count = 0;
    m->last_action = HOME_ACTION_NONE;
    m->last_arg = 0;
    m->last_refused = 0;

#ifdef OOPS_HOST_BUILD
    /* Baseline test titles for host selftest */
    static const home_title_t default_titles[] = {
        { "PPSA01325", "ASTRO'S PLAYROOM", "PS5", "1.004.000", 11400, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "PPSA01342", "DEMON'S SOULS",    "PS5", "1.002.000", 66200, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "PPSA01284", "RETURNAL",         "PS5", "1.003.000", 56100, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "PPSA01521", "HORIZON",          "PS5", "1.018.000", 98400, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "OOPS00001", "OBSCENE PROBE",    "ELF", "1.000.000",     4, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "OOPS00002", "PORTHOLE",         "ELF", "1.000.000",     2, 1, 0, 0, 0, 0, NULL, 0, 0 },
        { "CUSA00123", "BLOODBORNE",       "PS4", "1.009.000", 32000, 1, 0, 0, 0, 0, NULL, 0, 0 }
    };
    home_set_titles(m, default_titles, (int)(sizeof(default_titles) / sizeof(default_titles[0])));
#else
    m->title_count = 0;
#endif

    /* Realistic Media apps */
    static const home_title_t default_media[] = {
        { "MEDIA001", "MEDIA PLAYER (USB & LOCAL)", "SYSTEM", "1.00.00", 120, 1, 60, 0, 0, 0, NULL, 0, 0 },
        { "MEDIA002", "MEDIA GALLERY (CAPTURES)",   "SYSTEM", "1.00.00",  85, 1, 30, 0, 0, 0, NULL, 0, 0 },
        { "MEDIA003", "WEB BROWSER (WEBKIT)",       "SYSTEM", "1.00.00",  42, 1, 90, 0, 0, 0, NULL, 0, 0 }
    };
    m->media_count = (int)(sizeof(default_media) / sizeof(default_media[0]));
    for (int i = 0; i < m->media_count && i < HOME_MAX_MEDIA; i++) {
        m->media[i] = default_media[i];
    }

    /* Activities: only real data, none by default */
    m->activity_count = 0;

    /* Control Centre 13-dock icons */
    static const home_card_t default_cards[] = {
        { "HOME",          "RETURN TO SHELL",      HOME_ACTION_OPEN,         (int)HOME_SCREEN_GAMES },
        { "SWITCHER",      "NOW PLAYING",          HOME_ACTION_OPEN,         (int)HOME_SCREEN_SWITCHER },
        { "NOTIFICATIONS", "0 UNREAD",             HOME_ACTION_OPEN,         (int)HOME_SCREEN_NOTIFICATIONS },
        { "GAME BASE",     "NO FRIENDS ONLINE",    HOME_ACTION_OPEN,         (int)HOME_SCREEN_GAME_BASE },
        { "MUSIC",         "NO AUDIO",             HOME_ACTION_OPEN,         (int)HOME_SCREEN_MUSIC },
        { "CAPTURES",      "MEDIA GALLERY",        HOME_ACTION_OPEN,         (int)HOME_SCREEN_CAPTURES },
        { "ACCESSIBILITY", "QUICK TOGGLES",        HOME_ACTION_NONE,         0 },
        { "NETWORK",       "CONNECTED (WI-FI)",    HOME_ACTION_NONE,         0 },
        { "SOUND",         "HEADPHONES (80%)",     HOME_ACTION_TOGGLE_SOUND, 0 },
        { "MIC",           "MUTED (ORANGE LED)",   HOME_ACTION_TOGGLE_MIC,   0 },
        { "ACCESSORIES",   "DUALSENSE 1 (85%)",    HOME_ACTION_OPEN,         (int)HOME_SCREEN_SETTINGS_ACCESSORIES },
        { "PROFILE",       "PLAYER (ONLINE)",      HOME_ACTION_OPEN,         (int)HOME_SCREEN_PROFILE },
        { "POWER",         "REST / RESTART / OFF", HOME_ACTION_OPEN,         (int)HOME_SCREEN_POWER }
    };
    m->card_count = (int)(sizeof(default_cards) / sizeof(default_cards[0]));
    for (int i = 0; i < m->card_count && i < HOME_MAX_CARDS; i++) {
        m->cards[i] = default_cards[i];
    }

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

    /* Gather live hardware and system telemetry */
    home_refresh_telemetry(m);

    /* Build static menus */
    home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS];
    menu_reset(menu, "SETTINGS");
    menu_add(menu, "USERS AND ACCOUNTS", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_PROFILE, 1);
    menu_add(menu, "SYSTEM", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_SYSTEM, 1);
    menu_add(menu, "STORAGE", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_STORAGE, 1);
    menu_add(menu, "SOUND", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_SOUND, 1);
    menu_add(menu, "SCREEN AND VIDEO", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_VIDEO, 1);
    menu_add(menu, "ACCESSORIES", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_ACCESSORIES, 1);
    menu_add(menu, "SAVED DATA AND GAME/APP SETTINGS", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_SAVES, 1);
    menu_add(menu, "DEVELOPER & DEBUG SETTINGS", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_DEVELOPER, 1);
    menu_add(menu, "EMULATOR SETTINGS (ORBISTOUN)", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_SETTINGS_EMULATOR, 1);
    menu_add(menu, "APPEARANCE", "CYCLE SKIN", HOME_ACTION_NEXT_THEME, 0, 1);
    menu_add(menu, "POWER", 0, HOME_ACTION_OPEN, (int)HOME_SCREEN_POWER, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    build_system_menu(m);

    menu = &m->menus[HOME_SCREEN_SETTINGS_SOUND];
    menu_reset(menu, "SOUND");
    menu_add(menu, "AUDIO OUTPUT", "HEADPHONES / STEREO", HOME_ACTION_TOGGLE_SOUND, 0, 1);
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
    menu_add(menu, "CONTROLLER 1", "DUALSENSE (BATTERY 85%)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "COMMUNICATION METHOD", "USB / BLUETOOTH DUAL", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "VIBRATION INTENSITY", "STRONG (STANDARD)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "TRIGGER EFFECT INTENSITY", "STRONG (STANDARD)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "INDICATOR BRIGHTNESS", "MEDIUM", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "BACK", 0, HOME_ACTION_BACK, 0, 1);

    build_developer_menu(m);

    menu = &m->menus[HOME_SCREEN_SETTINGS_EMULATOR];
    menu_reset(menu, "EMULATOR SETTINGS");
    menu_add(menu, "SAVE STATE SLOT", "SLOT 1", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "QUICK SAVE STATE", "SLOT 1", HOME_ACTION_SAVE_STATE, 0, 1);
    menu_add(menu, "QUICK LOAD STATE", "SLOT 1", HOME_ACTION_LOAD_STATE, 0, 1);
    menu_add(menu, "FRAME LIMITER", "60 FPS (VSYNC ON)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "RESOLUTION SCALING", "100% NATIVE (1280X720 / 4K)", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "SHADER CACHE", "1,420 SHADERS COMPILED", HOME_ACTION_NONE, 0, 1);
    menu_add(menu, "PERFORMANCE HUD", "FPS & FRAME TIME OVERLAY", HOME_ACTION_NONE, 0, 1);
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

    menu = &m->menus[HOME_SCREEN_SEARCH];
    menu_reset(menu, "UNIVERSAL SEARCH");
    menu_add(menu, "OPEN VIRTUAL KEYBOARD", "SEARCH TITLES, MEDIA & STORAGE",
             HOME_ACTION_TRIGGER_DIALOG, (int)HOME_DIALOG_IME, 1);
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

/* ---- screen navigation ------------------------------------------------------------------ */

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
        if (m->media_count <= 0 || m->media_cursor < 0 || m->media_cursor >= m->media_count) {
            return 0;
        }
        return &m->media[m->media_cursor];
    }
    if (m->title_count <= 0 || m->title_cursor < 0 || m->title_cursor >= m->title_count) {
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

void home_next_theme(home_model_t *m) {
    if (m == 0) {
        return;
    }
    int count = home_theme_count();
    if (count > 0) {
        m->theme = (m->theme + 1) % count;
    }
}

/* ---- dialogs and notifications ---------------------------------------------------------- */

void home_show_dialog(home_model_t *m, home_dialog_type_t type, const char *title, const char *msg) {
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

/* ---- navigation movement ---------------------------------------------------------------- */

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

    /* Carousel navigation (Games / Media) */
    if (screen_is_carousel(screen)) {
        if (dir == HOME_UP) {
            m->top_nav = HOME_TOP_NAV_TABS;
            return;
        }
        int count = (screen == HOME_SCREEN_GAMES) ? m->title_count : m->media_count;
        if (count <= 0) {
            return;
        }
        if (dir == HOME_LEFT) {
            if (screen == HOME_SCREEN_GAMES) {
                m->title_cursor = (m->title_cursor + count - 1) % count;
            } else {
                m->media_cursor = (m->media_cursor + count - 1) % count;
            }
        } else if (dir == HOME_RIGHT) {
            if (screen == HOME_SCREEN_GAMES) {
                m->title_cursor = (m->title_cursor + 1) % count;
            } else {
                m->media_cursor = (m->media_cursor + 1) % count;
            }
        }
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

/* ---- action execution ------------------------------------------------------------------- */

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
            return 1;
        case HOME_ACTION_TOGGLE_MODE:
            home_switch_mode(m, (m->mode == HOME_MODE_GAMES) ? HOME_MODE_MEDIA : HOME_MODE_GAMES);
            return 1;
        case HOME_ACTION_DISMISS_NOTICE:
            if (arg >= 0 && arg < m->notice_count) {
                m->notices[arg].unread = 0;
                m->status.notifications = home_unread_count(m);
                return 1;
            }
            return 0;
        case HOME_ACTION_TRIGGER_DIALOG:
            home_show_dialog(m, (home_dialog_type_t)arg, "SYSTEM DIALOG", "ACTION REQUIRED");
            return 1;
        case HOME_ACTION_CLOSE_DIALOG:
            home_close_dialog(m);
            return 1;
        case HOME_ACTION_CONFIRM_DIALOG:
            home_close_dialog(m);
            home_show_toast(m, "SUCCESS", "OPERATION COMPLETED");
            return 1;
        default:
            break;
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

static const char s_ime_grid[4][12] = {
    "1234567890-",
    "QWERTYUIOP.",
    "ASDFGHJKL_/",
    "ZXCVBNM@:!?"
};

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
            home_show_toast(m, "SEARCH", (dlg->ime_len > 0) ? dlg->ime_buffer : "ALL TITLES");
            home_close_dialog(m);
            return 1;
        }
    }
    return 0;
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
        } else if (m->dialog.type == HOME_DIALOG_PROGRESS || m->dialog.type == HOME_DIALOG_ERROR) {
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
            home_switch_mode(m, (m->mode == HOME_MODE_GAMES) ? HOME_MODE_MEDIA : HOME_MODE_GAMES);
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
    if (screen_is_carousel(screen)) {
        if (screen == HOME_SCREEN_GAMES) {
            const home_title_t *title = home_selected_title(m);
            if (title == 0) {
                return 0;
            }
            m->selected_title = m->title_cursor;
            home_open(m, HOME_SCREEN_TITLE_OPTIONS);
            return 1;
        } else {
            home_show_toast(m, "MEDIA APP", m->media[m->media_cursor].name);
            return 1;
        }
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

/* ---- cursor calculation ----------------------------------------------------------------- */

static int content_top(const home_theme_t *theme) {
    return theme->margin_y + (theme->row_height * 2);
}

int home_cursor_rect(const home_model_t *m, const home_theme_t *theme,
                     int *x, int *y, int *w, int *h) {
    if (m == 0 || theme == 0 || x == 0 || y == 0 || w == 0 || h == 0) {
        return 0;
    }
    home_screen_t screen = home_screen(m);

    if (screen_is_carousel(screen)) {
        int cursor = (screen == HOME_SCREEN_GAMES) ? m->title_cursor : m->media_cursor;
        *x = theme->margin_x + (cursor * (theme->tile_width + (theme->margin_x / 2)));
        *y = content_top(theme);
        *w = theme->tile_width;
        *h = theme->tile_height + (theme->row_height / 2);
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
    return 1;
}

/* ---- renderer --------------------------------------------------------------------------- */

static void render_top_bar(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
    int y = theme->margin_y;

    /* Left: Mode Tabs (GAMES | MEDIA) */
    oops_color_t games_col = (m->mode == HOME_MODE_GAMES) ? theme->text : theme->text_dim;
    oops_color_t media_col = (m->mode == HOME_MODE_MEDIA) ? theme->text : theme->text_dim;

    int x = theme->margin_x;
    (void)oops_draw_text(surf, x, y, "GAMES", games_col, theme->text_scale);
    if (m->mode == HOME_MODE_GAMES) {
        oops_draw_rect(surf, x, y + 20, 80, 3, theme->accent);
    }
    if (m->top_nav == HOME_TOP_NAV_TABS && m->mode == HOME_MODE_GAMES) {
        oops_draw_rect(surf, x - 4, y - 4, 88, 30, theme->cursor);
    }

    x += 120;
    (void)oops_draw_text(surf, x, y, "MEDIA", media_col, theme->text_scale);
    if (m->mode == HOME_MODE_MEDIA) {
        oops_draw_rect(surf, x, y + 20, 80, 3, theme->accent);
    }
    if (m->top_nav == HOME_TOP_NAV_TABS && m->mode == HOME_MODE_MEDIA) {
        oops_draw_rect(surf, x - 4, y - 4, 88, 30, theme->cursor);
    }

    /* Right: Search, Settings, Profile, Clock */
    int rx = (int)surf->width - theme->margin_x - 360;

    /* Search */
    oops_color_t search_col = (m->top_nav == HOME_TOP_NAV_SEARCH) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, rx, y, "SEARCH", search_col, theme->text_scale);
    if (m->top_nav == HOME_TOP_NAV_SEARCH) {
        oops_draw_rect(surf, rx - 4, y - 4, 96, 26, theme->cursor);
    }
    rx += 110;

    /* Settings */
    oops_color_t set_col = (m->top_nav == HOME_TOP_NAV_SETTINGS) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, rx, y, "SETTINGS", set_col, theme->text_scale);
    if (m->top_nav == HOME_TOP_NAV_SETTINGS) {
        oops_draw_rect(surf, rx - 4, y - 4, 116, 26, theme->cursor);
    }
    rx += 130;

    /* Profile */
    oops_color_t prof_col = (m->top_nav == HOME_TOP_NAV_PROFILE) ? theme->accent : theme->text_dim;
    (void)oops_draw_text(surf, rx, y, m->status.user, prof_col, theme->text_scale);
    if (m->top_nav == HOME_TOP_NAV_PROFILE) {
        oops_draw_rect(surf, rx - 4, y - 4, 88, 26, theme->cursor);
    }

    /* Clock */
    char clock_buf[16];
    clock_buf[0] = (char)('0' + (m->status.hour / 10));
    clock_buf[1] = (char)('0' + (m->status.hour % 10));
    clock_buf[2] = ':';
    clock_buf[3] = (char)('0' + (m->status.minute / 10));
    clock_buf[4] = (char)('0' + (m->status.minute % 10));
    clock_buf[5] = '\0';
    (void)oops_draw_text(surf, (int)surf->width - theme->margin_x - 60, y, clock_buf,
                         theme->text, theme->text_scale);
}

static int render_games_carousel(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
    int drawn = 0;
    int base_y = content_top(theme);
    int step = theme->tile_width + (theme->margin_x / 2);

    for (int i = 0; i < m->title_count; i++) {
        int tx = theme->margin_x + (i * step);
        if (tx + theme->tile_width > (int)surf->width) {
            break;
        }
        int selected = (i == m->title_cursor && m->top_nav == HOME_TOP_NAV_NONE);
        int th = selected ? (theme->tile_height + 20) : theme->tile_height;
        int ty = selected ? (base_y - 10) : base_y;

        /* Tile card background */
        oops_color_t card_bg = selected ? theme->accent : theme->panel;
        oops_draw_rect(surf, tx, ty, theme->tile_width, th, card_bg);

        /* If title has an icon, blit it; otherwise fall back to initial letter */
        if (m->titles[i].icon_pixels != 0 && m->titles[i].icon_width > 0 && m->titles[i].icon_height > 0) {
            int iw = m->titles[i].icon_width;
            int ih = m->titles[i].icon_height;
            int ix = tx + (theme->tile_width - iw) / 2;
            int iy = ty + 8;
            if (iy + ih > ty + th - 20) {
                ih = (ty + th - 20) - iy;
            }
            if (ih > 0 && iw > 0) {
                oops_surface_t isurf;
                isurf.pixels = (uint32_t *)m->titles[i].icon_pixels;
                isurf.width = (uint32_t)m->titles[i].icon_width;
                isurf.height = (uint32_t)m->titles[i].icon_height;
                isurf.pitch = (uint32_t)m->titles[i].icon_width;
                oops_draw_blit_blend(surf, ix, iy, &isurf, 0, 0, iw, ih);
            }
        } else {
            /* Initial letter in centre of tile */
            char letter[2];
            letter[0] = '?';
            letter[1] = '\0';
            if (m->titles[i].name != 0) {
                const char *nm = m->titles[i].name;
                while (*nm) {
                    if ((*nm >= 'A' && *nm <= 'Z') || (*nm >= 'a' && *nm <= 'z') || (*nm >= '0' && *nm <= '9')) {
                        letter[0] = (*nm >= 'a' && *nm <= 'z') ? (char)(*nm - 32) : *nm;
                        break;
                    }
                    nm++;
                }
            }
            if (letter[0] == '?' && m->titles[i].id != 0 && m->titles[i].id[0] != '\0') {
                letter[0] = m->titles[i].id[0];
            }
            (void)oops_draw_text(surf, tx + (theme->tile_width / 2) - 8, ty + (th / 2) - 12,
                                 letter, theme->text, 3);
        }

        /* Category badge - short, centered, strictly clamped to tile width */
        const char *cat = m->titles[i].category ? m->titles[i].category : "APP";
        char badge[16];
        if (cat[0] == 'P' && cat[1] == 'R' && cat[2] == 'O') {
            badge[0] = 'P'; badge[1] = 'S'; badge[2] = '5'; badge[3] = '\0';
        } else if (cat[0] == 'O' && cat[1] == 'R' && cat[2] == 'B') {
            badge[0] = 'P'; badge[1] = 'S'; badge[2] = '4'; badge[3] = '\0';
        } else {
            size_t max_b = (size_t)((theme->tile_width - 12) / 8);
            if (max_b > sizeof(badge) - 1) max_b = sizeof(badge) - 1;
            size_t l = 0;
            while (cat[l] && l < max_b) {
                badge[l] = cat[l];
                l++;
            }
            badge[l] = '\0';
        }
        int badge_len = 0;
        while (badge[badge_len]) badge_len++;
        int bx = tx + (theme->tile_width - (badge_len * 8)) / 2;
        if (bx < tx + 4) bx = tx + 4;
        (void)oops_draw_text(surf, bx, ty + th - 16, badge,
                             selected ? theme->background : theme->text_dim, 1);
        drawn += 3;
    }

    /* Detail Hub beneath carousel */
    if (m->title_cursor >= 0 && m->title_cursor < m->title_count) {
        const home_title_t *title = &m->titles[m->title_cursor];
        int dy = base_y + theme->tile_height + 40;

        /* Title Name */
        (void)oops_draw_text(surf, theme->margin_x, dy, title->name ? title->name : title->id, theme->text, 2);
        dy += 32;

        /* Metadata: [CATEGORY] TITLE_ID    VERSION */
        char meta_line[128];
        oops_snprintf(meta_line, sizeof(meta_line), "[%s]  %s    VER: %s",
                      title->category ? title->category : "APP",
                      title->id ? title->id : "-",
                      title->version ? title->version : "1.00");
        (void)oops_draw_text(surf, theme->margin_x, dy, meta_line, theme->accent, 1);
        dy += 30;

        /* Action button prompts */
        (void)oops_draw_text(surf, theme->margin_x, dy, "[X] PLAY", theme->text, 1);
        (void)oops_draw_text(surf, theme->margin_x + 90, dy, "[OPTIONS] OPTIONS", theme->text_dim, 1);
        (void)oops_draw_text(surf, theme->margin_x + 250, dy, "[SQUARE] LIBRARY", theme->text_dim, 1);
        drawn += 5;
    }
    return drawn;
}

static int render_control_centre(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
    int drawn = 0;
    int sw = (int)surf->width;
    int sh = (int)surf->height;

    /* Dark translucent overlay across screen */
    oops_draw_rect_blend(surf, 0, 0, sw, sh, 0xDD0A0E16u);

    /* Upper Activity Cards */
    int uy = sh - 250;
    oops_draw_rect(surf, theme->margin_x, uy, 400, 90, theme->panel);
    (void)oops_draw_text(surf, theme->margin_x + 16, uy + 16, "NOW PLAYING", theme->accent, 1);
    if (m->switcher.has_running_title && m->switcher.running_title_index >= 0 && m->switcher.running_title_index < m->title_count) {
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 36, m->titles[m->switcher.running_title_index].name, theme->text, 2);
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 64, "PRESS OPTIONS TO CLOSE", theme->text_dim, 1);
    } else {
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 36, "NO TITLE ACTIVE", theme->text, 2);
        (void)oops_draw_text(surf, theme->margin_x + 16, uy + 64, "SELECT A GAME TO PLAY", theme->text_dim, 1);
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

static int render_storage(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
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
    (void)oops_draw_text(surf, theme->margin_x, y, "GAMES & APPS: 412 GB", 0xFF3D8BFDu, 1);
    (void)oops_draw_text(surf, theme->margin_x + 220, y, "MEDIA: 24 GB", 0xFF4CAF50u, 1);
    (void)oops_draw_text(surf, theme->margin_x + 380, y, "SAVES: 8 GB", 0xFFFFC107u, 1);
    (void)oops_draw_text(surf, theme->margin_x + 520, y, "OTHER: 56 GB", 0xFF9E9E9Eu, 1);
    (void)oops_draw_text(surf, theme->margin_x + 680, y, "FREE: 325 GB", theme->text_dim, 1);
    y += 50;

    /* Drives list */
    const home_menu_t *menu = &m->menus[HOME_SCREEN_SETTINGS_STORAGE];
    for (int i = 0; i < menu->count; i++) {
        int selected = (i == menu->cursor);
        oops_color_t col = selected ? theme->accent : theme->text;
        if (selected) {
            oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, theme->row_height, theme->panel);
        }
        (void)oops_draw_text(surf, theme->margin_x + 12, y, menu->items[i].label, col, 1);
        if (menu->items[i].detail != 0) {
            (void)oops_draw_text(surf, theme->margin_x + 400, y, menu->items[i].detail,
                                 theme->text_dim, 1);
        }
        y += theme->row_height;
        drawn += 2;
    }
    return drawn + 8;
}

static int render_menu(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
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
        oops_color_t col = selected ? theme->accent : (menu->items[i].enabled ? theme->text : theme->text_dim);
        if (selected) {
            oops_draw_rect(surf, theme->margin_x, y - 4, bar_w, theme->row_height, theme->panel);
        }
        (void)oops_draw_text(surf, theme->margin_x + 16, y, menu->items[i].label, col, 1);
        if (menu->items[i].detail != 0) {
            (void)oops_draw_text(surf, theme->margin_x + 420, y, menu->items[i].detail,
                                 theme->text_dim, 1);
        }
        y += theme->row_height;
        drawn += 2;
    }
    return drawn;
}

static void render_dialog(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
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
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.message, theme->text_dim, 1);
        }
        int by = dy + dh - 60;
        int cancel_sel = (m->dialog.confirm_choice == 0);
        oops_draw_rect(surf, dx + 60, by, 180, 40, cancel_sel ? theme->accent : theme->background);
        (void)oops_draw_text(surf, dx + 110, by + 12, "CANCEL",
                             cancel_sel ? theme->background : theme->text, 1);

        int ok_sel = (m->dialog.confirm_choice == 1);
        oops_draw_rect(surf, dx + 360, by, 180, 40, ok_sel ? theme->accent : theme->background);
        (void)oops_draw_text(surf, dx + 420, by + 12, "OK",
                             ok_sel ? theme->background : theme->text, 1);

    } else if (m->dialog.type == HOME_DIALOG_PROGRESS) {
        if (m->dialog.message != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.message, theme->text_dim, 1);
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
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.error_code, 0xFFE53935u, 2);
            cy += 36;
        }
        if (m->dialog.error_desc != 0) {
            (void)oops_draw_text(surf, dx + 30, cy, m->dialog.error_desc, theme->text_dim, 1);
        }
        int by = dy + dh - 60;
        oops_draw_rect(surf, dx + 210, by, 180, 40, theme->accent);
        (void)oops_draw_text(surf, dx + 280, by + 12, "OK", theme->background, 1);

    } else if (m->dialog.type == HOME_DIALOG_IME) {
        /* Input text display box */
        oops_draw_rect(surf, dx + 30, cy, dw - 60, 36, theme->background);
        (void)oops_draw_text(surf, dx + 40, cy + 10,
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
        (void)oops_draw_text(surf, dx + 40, by, "[SPACE]",
                             (sel_act && m->dialog.ime_col < 3) ? theme->accent : theme->text_dim, 1);
        (void)oops_draw_text(surf, dx + 150, by, "[BACKSPACE]",
                             (sel_act && m->dialog.ime_col >= 3 && m->dialog.ime_col < 5) ? theme->accent : theme->text_dim, 1);
        (void)oops_draw_text(surf, dx + 300, by, "[CLEAR]",
                             (sel_act && m->dialog.ime_col >= 5 && m->dialog.ime_col < 7) ? theme->accent : theme->text_dim, 1);
        (void)oops_draw_text(surf, dx + 420, by, "[DONE]",
                             (sel_act && m->dialog.ime_col >= 7) ? theme->accent : theme->text_dim, 1);
    }
}

static void render_toast(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
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

int home_render(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme) {
    if (surf == 0 || surf->pixels == 0 || m == 0 || theme == 0) {
        return 0;
    }
    oops_draw_clear(surf, theme->background);
    render_top_bar(surf, m, theme);

    int drawn = 5;
    home_screen_t screen = home_screen(m);

    if (screen == HOME_SCREEN_GAMES || screen == HOME_SCREEN_MEDIA) {
        drawn += render_games_carousel(surf, m, theme);
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

    /* Footer indicators */
    int footer_y = (int)surf->height - theme->margin_y;
    (void)oops_draw_text(surf, theme->margin_x, footer_y, theme->name, theme->text_dim,
                         theme->text_scale);
    drawn++;

    if (m->last_refused != 0) {
        (void)oops_draw_text(surf, theme->margin_x, footer_y - theme->row_height,
                             "NOT AVAILABLE HERE", theme->text_dim, theme->text_scale);
        drawn++;
    }
    return drawn;
}

/* ---- pad input -------------------------------------------------------------------------- */

void home_input_reset(home_input_t *in) {
    if (in == 0) {
        return;
    }
    in->previous = 0u;
    in->primed = 0;
    in->held_frames = 0;
    in->held_dir = 0u;
}

static int pressed(uint32_t previous, uint32_t now, uint32_t mask) {
    return ((now & mask) != 0u) && ((previous & mask) == 0u);
}

static uint32_t direction_down(uint32_t buttons) {
    if ((buttons & OOPS_BUTTON_UP) != 0u) { return OOPS_BUTTON_UP; }
    if ((buttons & OOPS_BUTTON_DOWN) != 0u) { return OOPS_BUTTON_DOWN; }
    if ((buttons & OOPS_BUTTON_LEFT) != 0u) { return OOPS_BUTTON_LEFT; }
    if ((buttons & OOPS_BUTTON_RIGHT) != 0u) { return OOPS_BUTTON_RIGHT; }
    return 0u;
}

static void move_for(home_model_t *m, uint32_t bit) {
    if (bit == OOPS_BUTTON_UP) { home_move(m, HOME_UP); }
    else if (bit == OOPS_BUTTON_DOWN) { home_move(m, HOME_DOWN); }
    else if (bit == OOPS_BUTTON_LEFT) { home_move(m, HOME_LEFT); }
    else if (bit == OOPS_BUTTON_RIGHT) { home_move(m, HOME_RIGHT); }
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
    if (pressed(previous, buttons, HOME_BUTTON_PS)) {
        home_toggle_control_centre(m);
        changed = 1;
    }

    /* Mode switching via L1 / R1 */
    if (pressed(previous, buttons, OOPS_BUTTON_L1)) {
        home_switch_mode(m, HOME_MODE_GAMES);
        changed = 1;
    }
    if (pressed(previous, buttons, OOPS_BUTTON_R1)) {
        home_switch_mode(m, HOME_MODE_MEDIA);
        changed = 1;
    }

    /* Directional navigation */
    uint32_t dir = direction_down(buttons);
    if (dir == 0u) {
        in->held_dir = 0u;
        in->held_frames = 0;
    } else if (dir != in->held_dir) {
        in->held_dir = dir;
        in->held_frames = 0;
        move_for(m, dir);
        changed = 1;
    } else {
        in->held_frames++;
        if (in->held_frames >= HOME_REPEAT_DELAY_FRAMES) {
            int since = in->held_frames - HOME_REPEAT_DELAY_FRAMES;
            if ((since % HOME_REPEAT_EVERY_FRAMES) == 0) {
                move_for(m, dir);
                changed = 1;
            }
        }
    }

    /* Face buttons */
    if (pressed(previous, buttons, OOPS_BUTTON_CROSS)) {
        if (home_activate(m) != 0) {
            changed = 1;
        }
    }
    if (pressed(previous, buttons, OOPS_BUTTON_CIRCLE)) {
        if (home_back(m) != 0) {
            changed = 1;
        }
    }
    if (pressed(previous, buttons, OOPS_BUTTON_SQUARE)) {
        home_open(m, HOME_SCREEN_LIBRARY);
        changed = 1;
    }
    if (pressed(previous, buttons, OOPS_BUTTON_TRIANGLE)) {
        home_open(m, HOME_SCREEN_SEARCH);
        changed = 1;
    }
    if (pressed(previous, buttons, OOPS_BUTTON_OPTIONS)) {
        home_screen_t screen = home_screen(m);
        if (screen_is_carousel(screen)) {
            m->selected_title = m->title_cursor;
            home_open(m, HOME_SCREEN_TITLE_OPTIONS);
            changed = 1;
        }
    }

    in->previous = buttons;
    return changed;
}
