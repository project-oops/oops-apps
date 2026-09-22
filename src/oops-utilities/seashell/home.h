#ifndef OOPS_APPS_HOME_H
#define OOPS_APPS_HOME_H

#include "oops/draw.h"
#include "oops/input.h"

/*
 * home - clean-room reimplementation of the Prospero shell (Prospero UX / SceShellCore) for homebrew
 * and the Orbistoun emulator.
 *
 * Two audiences, one codebase:
 * 1. The front-end UI and system software shell for Orbistoun.
 * 2. An on-console launcher payload for real hardware.
 *
 * # The split: the model does everything that does not touch a machine
 *
 * - The model (home.c) manages screens, navigation, dialogs, cursor state, and software drawing.
 *   It runs headlessly, allocates nothing from libc, and has zero external dependencies.
 * - The host (home_host_t, home_main.c) handles title launching, process lifecycle, package
 *   installation, save states, captures, and machine power. In Orbistoun, the host dispatch
 *   drives the emulator's subsystems; on real hardware, it talks to kernel services.
 *
 * # Controller binding
 *
 * - PS Button (bit 16, HOME_BUTTON_PS): opens/closes the Control Centre overlay.
 * - L1 / R1: switches between GAMES and MEDIA mode.
 * - D-Pad / Sticks: navigation with initial delay and hold-to-repeat.
 * - Cross: select / activate.
 * - Circle: back / cancel.
 * - Square: quick jump to Game Library.
 * - Triangle: universal search (with virtual keyboard).
 * - Options: context menu on highlighted game/app.
 * - L1 + R1 + Options: clean exit to loader.
 */

/* Standard PS/Home system button bitmask (bit 16 confirmed on Prospero hardware) */
#define HOME_BUTTON_PS (1u << 16)

/* Fixed sizes throughout: a payload has no allocator */
#define HOME_MAX_ITEMS 64
#define HOME_MAX_TITLES 64
#define HOME_MAX_MEDIA 8
#define HOME_MAX_CARDS 14
#define HOME_MAX_NOTICES 12
#define HOME_MAX_ACTIVITIES 4
#define HOME_MAX_FRIENDS 8
#define HOME_MAX_SAVES 8
#define HOME_MAX_CAPTURES 8
#define HOME_NAV_DEPTH 8

/* ---- skins & themes --------------------------------------------------------------------- */

#include "skin.h"

/* Legacy layout enum retained for test/API compatibility */
typedef enum home_layout {
    HOME_LAYOUT_XMB = 0,    /* Cross Media Bar */
    HOME_LAYOUT_LIST = 1,   /* Framed column */
    HOME_LAYOUT_BLADES = 2, /* Overlapping tab panels */
    HOME_LAYOUT_TILES = 3   /* Tiles over detail strip */
} home_layout_t;

/* Skin Management API */
const home_skin_t *home_current_skin(const struct home_model *m);
void home_set_skin(struct home_model *m, int index);
void home_next_skin(struct home_model *m);

/* Legacy Theme API (forwards to active skin's theme) */
int home_theme_count(void);
const home_theme_t *home_theme_at(int index);
void home_next_theme(struct home_model *m);

/* ---- navigation modes & top bar --------------------------------------------------------- */

typedef enum home_mode {
    HOME_MODE_GAMES = 0,
    HOME_MODE_MEDIA = 1
} home_mode_t;

typedef enum home_top_nav {
    HOME_TOP_NAV_NONE = 0,     /* Focus on main carousel/content */
    HOME_TOP_NAV_TABS = 1,     /* Focus on GAMES / MEDIA tab selector */
    HOME_TOP_NAV_SEARCH = 2,   /* Focus on Search [SEARCH] */
    HOME_TOP_NAV_SETTINGS = 3, /* Focus on Settings [GEAR] */
    HOME_TOP_NAV_PROFILE = 4   /* Focus on Profile avatar */
} home_top_nav_t;

/* ---- screens ---------------------------------------------------------------------------- */

typedef enum home_screen {
    HOME_SCREEN_GAMES = 0,             /* Games carousel & game hub (default) */
    HOME_SCREEN_MEDIA,                 /* Media apps carousel */
    HOME_SCREEN_LIBRARY,               /* Installed / Collection / Homebrew */
    HOME_SCREEN_TITLE_OPTIONS,         /* Options context menu on title */
    HOME_SCREEN_TITLE_INFO,            /* Metadata & file details */
    HOME_SCREEN_CONTROL,               /* Quick menu: 13-dock + cards */
    HOME_SCREEN_SWITCHER,              /* Active running title & recent switcher */
    HOME_SCREEN_NOTIFICATIONS,         /* Unread and history notifications */
    HOME_SCREEN_GAME_BASE,             /* Friends online & voice parties */
    HOME_SCREEN_MUSIC,                 /* Background audio player */
    HOME_SCREEN_CAPTURES,              /* Media gallery (screenshots & video clips) */
    HOME_SCREEN_PROFILE,               /* User status, trophies, switch user */
    HOME_SCREEN_POWER,                 /* Rest mode, Restart, Power Off, Log Out */
    HOME_SCREEN_SETTINGS,              /* Root settings tree */
    HOME_SCREEN_SETTINGS_SYSTEM,       /* Console info, FW, Power Saving, HDMI */
    HOME_SCREEN_SETTINGS_STORAGE,      /* Storage visual meter & content manager */
    HOME_SCREEN_SETTINGS_SOUND,        /* Output device, 3D audio, volume, mic */
    HOME_SCREEN_SETTINGS_VIDEO,        /* Resolution, refresh rate, HDR, display */
    HOME_SCREEN_SETTINGS_ACCESSORIES,  /* Controllers, input devices, haptics */
    HOME_SCREEN_SETTINGS_SAVES,        /* Save data management (backup, USB, delete) */
    HOME_SCREEN_SETTINGS_DEVELOPER,    /* Package installer, payload runner, klog */
    HOME_SCREEN_SETTINGS_EMULATOR,     /* Save states, FPS toggle, host sync */
    HOME_SCREEN_SETTINGS_THEME,        /* Themes and skins selection */
    HOME_SCREEN_SEARCH,                /* Universal search with on-screen keyboard */
    HOME_SCREEN_COUNT
} home_screen_t;

/* Aliases for compatibility */
#define HOME_SCREEN_HOME  HOME_SCREEN_GAMES
#define HOME_SCREEN_TITLE HOME_SCREEN_TITLE_OPTIONS

/* ---- actions ---------------------------------------------------------------------------- */

typedef enum home_action {
    HOME_ACTION_NONE = 0,
    HOME_ACTION_OPEN,             /* arg: home_screen_t */
    HOME_ACTION_BACK,
    HOME_ACTION_NEXT_THEME,
    HOME_ACTION_SET_THEME,         /* arg: skin index */
    HOME_ACTION_TOGGLE_MODE,      /* Games <-> Media */
    HOME_ACTION_LAUNCH_TITLE,     /* arg: index into titles */
    HOME_ACTION_TITLE_INFO,       /* arg: index into titles */
    HOME_ACTION_DELETE_TITLE,     /* arg: index into titles */
    HOME_ACTION_CHECK_UPDATE,     /* arg: index into titles */
    HOME_ACTION_MANAGE_CONTENT,   /* arg: index into titles */
    HOME_ACTION_SYNC_SAVE,        /* arg: index into titles */
    HOME_ACTION_SUSPEND_TITLE,    /* suspend current title */
    HOME_ACTION_RESUME_TITLE,     /* resume suspended title */
    HOME_ACTION_TERMINATE_TITLE,  /* close running title */
    HOME_ACTION_INSTALL_PACKAGE,  /* install pkg */
    HOME_ACTION_RUN_PAYLOAD,      /* run payload */
    HOME_ACTION_SWITCH_USER,
    HOME_ACTION_TOGGLE_SOUND,
    HOME_ACTION_TOGGLE_MIC,
    HOME_ACTION_TOGGLE_MUSIC,
    HOME_ACTION_EXPORT_SAVE,
    HOME_ACTION_IMPORT_SAVE,
    HOME_ACTION_DELETE_SAVE,
    HOME_ACTION_SAVE_STATE,
    HOME_ACTION_LOAD_STATE,
    HOME_ACTION_TAKE_SCREENSHOT,
    HOME_ACTION_REST_MODE,
    HOME_ACTION_RESTART,
    HOME_ACTION_POWER_OFF,
    HOME_ACTION_DISMISS_NOTICE,
    HOME_ACTION_TRIGGER_DIALOG,   /* arg: home_dialog_type_t */
    HOME_ACTION_CLOSE_DIALOG,
    HOME_ACTION_CONFIRM_DIALOG,
    HOME_ACTION_IME_KEY,          /* arg: character */
    HOME_ACTION_IME_BACKSPACE,
    HOME_ACTION_IME_SUBMIT,
    HOME_ACTION_TOGGLE_FAVORITE,  /* arg: index into titles */
    HOME_ACTION_SET_LIBRARY_FILTER, /* arg: home_filter_t */
    HOME_ACTION_RESCAN_TITLES     /* refresh/rescan titles on disk */
} home_action_t;

/* Library category filter modes */
typedef enum home_filter {
    HOME_FILTER_ALL = 0,
    HOME_FILTER_NATIVE,
    HOME_FILTER_HOMEBREW,
    HOME_FILTER_FAVORITES,
    HOME_FILTER_COUNT
} home_filter_t;

/* One menu entry */
typedef struct home_item {
    const char *label;
    const char *detail;
    home_action_t action;
    int arg;
    int enabled;
} home_item_t;

/* A menu screen */
typedef struct home_menu {
    const char *heading;
    home_item_t items[HOME_MAX_ITEMS];
    int count;
    int cursor;
} home_menu_t;

/* ---- data structures -------------------------------------------------------------------- */

/* One title on the carousel / library */
typedef struct home_title {
    const char *id;           /* "PPSA01325" or "OOPS00001" */
    const char *name;         /* Uppercase */
    const char *category;     /* "PROSPERO BIG APP (0)", "MINI APP (1)", "ORBIS", "ELF" */
    const char *version;      /* "1.002.000" */
    int size_mb;
    int installed;            /* 1 = installed, 0 = available */
    int minutes_played;
    int last_played_days_ago;
    int trophy_unlocked;
    int trophy_total;
    const uint32_t *icon_pixels; /* 32bpp ARGB pixels or NULL */
    int icon_width;
    int icon_height;
    int favorite;             /* 1 = pinned to favorites, 0 = normal */
} home_title_t;

/* One activity card beneath carousel */
typedef struct home_activity {
    const char *title;
    const char *subtitle;
    int progress_percent;
} home_activity_t;

/* One control-centre card */
typedef struct home_card {
    const char *label;
    const char *detail;
    home_action_t action;
    int arg;
} home_card_t;

/* One notification */
typedef struct home_notice {
    const char *text;
    int unread;
} home_notice_t;

/* One friend in Game Base */
typedef struct home_friend {
    const char *name;
    const char *activity;
    int online;
    int in_party;
} home_friend_t;

/* One save data entry */
typedef struct home_save {
    const char *title_id;
    const char *name;
    const char *timestamp;
    int size_kb;
} home_save_t;

/* One media capture */
typedef struct home_capture {
    const char *title;
    const char *type_and_res;
    const char *timestamp;
    int size_mb;
} home_capture_t;

/* Switcher state for running games */
typedef struct home_switcher {
    int has_running_title;
    int running_title_index;
    int is_suspended;
    int recent_indices[3];
    int recent_count;
} home_switcher_t;

/* Storage visualization breakdown */
typedef struct home_storage_breakdown {
    int total_gb;
    int games_gb;
    int media_gb;
    int saves_gb;
    int system_gb;
    int free_gb;
} home_storage_breakdown_t;

/* Developer and emulator settings state */
typedef struct home_dev_state {
    int big_app_override;
    int pltauth_active;
    int save_state_slot;
    int fps_overlay;
    int frame_limit_60;
    int shaders_compiled;
    int cpu_temp_c;
    int soc_temp_c;
    int fan_duty_pct;
    int total_ram_mb;
    int direct_mem_mb;
    char fw_version[16];
    char model_name[32];
    char username[32];
    char console_info_str[64];
    char pltauth_str[48];
    char hw_telemetry_str[64];
} home_dev_state_t;

/* Common dialog types */
typedef enum home_dialog_type {
    HOME_DIALOG_NONE = 0,
    HOME_DIALOG_CONFIRM,   /* OK / Cancel prompt */
    HOME_DIALOG_PROGRESS,  /* Visual progress bar */
    HOME_DIALOG_IME,       /* Virtual on-screen keyboard */
    HOME_DIALOG_ERROR      /* Formatted error code modal */
} home_dialog_type_t;

/* Common dialog state */
typedef struct home_dialog {
    home_dialog_type_t type;
    const char *title;
    const char *message;
    int progress_percent;
    int confirm_choice;    /* 0 = Cancel, 1 = Confirm */
    home_action_t on_confirm;
    int on_confirm_arg;

    /* Virtual keyboard (IME) */
    char ime_buffer[48];
    int ime_len;
    int ime_row;
    int ime_col;

    /* Error code */
    const char *error_code;
    const char *error_desc;
} home_dialog_t;

/* Floating toast notification */
typedef struct home_toast {
    int active;
    int frames_left;
    const char *title;
    const char *message;
} home_toast_t;

/* Status bar */
typedef struct home_status {
    int hour;
    int minute;
    const char *user;
    int notifications;
    int storage_used_gb;
    int storage_total_gb;
    int network_up;
    int pad_battery;
} home_status_t;

/* Persistent settings structure (stored at /data/homebrew/SCSH00001/settings.bin) */
#define HOME_SETTINGS_MAGIC 0x53435348u /* "SCSH" */
#define HOME_SETTINGS_VERSION 1u
#define HOME_MAX_PERSIST_FAVORITES 16

typedef struct home_settings_persist {
    uint32_t magic;           /* HOME_SETTINGS_MAGIC */
    uint32_t version;         /* HOME_SETTINGS_VERSION */
    int theme_index;
    int mode;
    int sound_volume;
    int mic_muted;
    int favorite_count;
    char favorite_ids[HOME_MAX_PERSIST_FAVORITES][16];
    char reserved[32];
} home_settings_persist_t;

/* Host dispatch seam */
typedef struct home_host {
    void *ctx;
    int (*perform)(void *ctx, home_action_t action, int arg);
} home_host_t;

/* ---- the model -------------------------------------------------------------------------- */

typedef struct home_model {
    home_menu_t menus[HOME_SCREEN_COUNT];

    /* Navigation stack */
    home_screen_t stack[HOME_NAV_DEPTH];
    int depth;

    /* Top bar & mode */
    home_mode_t mode;
    home_top_nav_t top_nav;

    /* Filtering & Universal Search */
    home_filter_t library_filter;
    char last_search[48];

    /* Games library & carousel */
    home_title_t titles[HOME_MAX_TITLES];
    int title_count;
    int title_cursor;
    int selected_title;

    /* Media apps carousel */
    home_title_t media[HOME_MAX_MEDIA];
    int media_count;
    int media_cursor;

    /* Active game activities */
    home_activity_t activities[HOME_MAX_ACTIVITIES];
    int activity_count;

    /* Control Centre dock & cards */
    home_card_t cards[HOME_MAX_CARDS];
    int card_count;
    int card_cursor;

    /* Notifications */
    home_notice_t notices[HOME_MAX_NOTICES];
    int notice_count;

    /* Friends / Game Base */
    home_friend_t friends[HOME_MAX_FRIENDS];
    int friend_count;

    /* Saves */
    home_save_t saves[HOME_MAX_SAVES];
    int save_count;

    /* Captures */
    home_capture_t captures[HOME_MAX_CAPTURES];
    int capture_count;

    /* Subsystem states */
    home_switcher_t switcher;
    home_storage_breakdown_t storage;
    home_dev_state_t dev;

    /* Common dialog overlay */
    home_dialog_t dialog;

    /* Toast overlay */
    home_toast_t toast;

    /* Status bar, skin & theme */
    home_status_t status;
    int skin_idx;                         /* 0..home_skin_count()-1 */
    int theme;                            /* alias synchronized with skin_idx */

    /* Generic multi-category navigation state */
    int category_idx;                                /* active category / section */
    int category_cursor[HOME_MAX_SKIN_CATEGORIES];   /* per-category item cursor */

    home_host_t host;

    /* Telemetry / Last action */
    int activate_count;
    home_action_t last_action;
    int last_arg;
    int last_refused;
} home_model_t;

typedef enum home_direction {
    HOME_LEFT = 0,
    HOME_RIGHT = 1,
    HOME_UP = 2,
    HOME_DOWN = 3
} home_direction_t;

/* ---- model API -------------------------------------------------------------------------- */

void home_model_init(home_model_t *m);
void home_refresh_telemetry(home_model_t *m);
void home_set_host(home_model_t *m, const home_host_t *host);
void home_set_titles(home_model_t *m, const home_title_t *titles, int count);

home_screen_t home_screen(const home_model_t *m);
void home_open(home_model_t *m, home_screen_t screen);
int  home_back(home_model_t *m);
void home_toggle_control_centre(home_model_t *m);
void home_switch_mode(home_model_t *m, home_mode_t mode);

home_menu_t *home_current_menu(home_model_t *m);
const home_menu_t *home_current_menu_const(const home_model_t *m);

void home_move(home_model_t *m, home_direction_t dir);
int  home_activate(home_model_t *m);

void home_show_dialog(home_model_t *m, home_dialog_type_t type, const char *title, const char *msg);
void home_show_error(home_model_t *m, const char *code, const char *desc);
void home_close_dialog(home_model_t *m);
void home_show_toast(home_model_t *m, const char *title, const char *msg);
void home_tick(home_model_t *m);

/*
 * A digest of the whole model, for deciding whether a frame needs drawing at
 * all. Equal digests on consecutive frames mean home_render() would produce the
 * same pixels, so the caller can skip the render and the flip; see the note on
 * the definition for why it hashes everything rather than a list of fields.
 */
uint64_t home_model_digest(const home_model_t *m);

const home_title_t *home_selected_title(const home_model_t *m);
const home_item_t *home_selected_item(const home_model_t *m);

void home_next_theme(home_model_t *m);
int  home_unread_count(const home_model_t *m);

/* ---- rendering -------------------------------------------------------------------------- */

int home_cursor_rect(const home_model_t *m, const home_theme_t *theme,
                     int *x, int *y, int *w, int *h);
int home_render(oops_surface_t *surf, const home_model_t *m, const home_theme_t *theme);

/* ---- pad input -------------------------------------------------------------------------- */

#define HOME_REPEAT_DELAY_FRAMES 18
#define HOME_REPEAT_EVERY_FRAMES 5
#define HOME_REPEAT_FAST_FRAMES  2

typedef struct home_input {
    uint32_t previous;
    int primed;
    int held_frames;
    uint32_t held_dir;
} home_input_t;

void home_input_reset(home_input_t *in);
int  home_input_apply(home_input_t *in, home_model_t *m, uint32_t buttons);

#endif /* OOPS_APPS_HOME_H */
