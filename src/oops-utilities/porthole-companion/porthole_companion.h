#ifndef OOPS_APPS_PORTHOLE_COMPANION_H
#define OOPS_APPS_PORTHOLE_COMPANION_H

#include "oops/draw.h"
#include <stdint.h>
#include <stddef.h>

#if defined(__has_include)
#  if __has_include("porthole.h")
#    include "porthole.h"
#  endif
#endif

#ifndef PORTHOLE_PORT_VIDEO
#  define PORTHOLE_PORT_VIDEO 9805u
#endif
#ifndef PORTHOLE_PORT_INPUT
#  define PORTHOLE_PORT_INPUT 9806u
#endif

#define PORTHOLE_DEFAULT_VIDEO_PORT PORTHOLE_PORT_VIDEO
#define PORTHOLE_DEFAULT_INPUT_PORT PORTHOLE_PORT_INPUT
#define PORTHOLE_ELFLDR_PORT        9021
#define PORTHOLE_SHSRV_PORT         2323

#define PORTHOLE_PAYLOAD_DATA_PATH  "/data/pldmgr/payloads/porthole/porthole.elf"
#define PORTHOLE_PAYLOAD_APP0_PATH  "/app0/payload/porthole.elf"
#define PORTHOLE_RELEASE_URL        "https://github.com/project-oops/oops-apps/releases/download/latest-main/porthole-prospero.elf"

typedef enum porthole_daemon_status {
    PORTHOLE_STATUS_UNKNOWN = 0,
    PORTHOLE_STATUS_RUNNING,
    PORTHOLE_STATUS_STOPPED,
    PORTHOLE_STATUS_PROBING,
    PORTHOLE_STATUS_ERROR
} porthole_daemon_status_t;

typedef enum porthole_menu_item {
    MENU_ACTION_LAUNCH_RESTART = 0,
    MENU_ACTION_DOWNLOAD_PAYLOAD,
    MENU_ACTION_TEST_KEYFRAME,
    MENU_ACTION_REFRESH_STATUS,
    MENU_ACTION_COUNT
} porthole_menu_item_t;

typedef struct porthole_companion_state {
    porthole_daemon_status_t status;
    char ip_address[32];
    uint16_t video_port;
    uint16_t input_port;
    int has_bundled_payload;
    int has_data_payload;
    int client_connected;
    int elfldr_running;
    int shsrv_running;

    porthole_menu_item_t selected_item;

    /* Interactive status message banner */
    char status_message[128];
    uint32_t status_color;

    /* Download state */
    int is_downloading;
    uint64_t dl_bytes_done;
    uint64_t dl_bytes_total;
    int dl_pct;

    /* Animation and pacing counter */
    uint32_t frame_count;
} porthole_companion_state_t;

/* Pure state operations (host testable) */
void porthole_companion_init(porthole_companion_state_t *st);
void porthole_companion_set_ip(porthole_companion_state_t *st, const char *ip);
void porthole_companion_set_status(porthole_companion_state_t *st, porthole_daemon_status_t status, const char *msg);
void porthole_companion_nav_up(porthole_companion_state_t *st);
void porthole_companion_nav_down(porthole_companion_state_t *st);

/* Render pure UI into destination surface */
void porthole_companion_render(const porthole_companion_state_t *st, oops_surface_t *surf);

#endif /* OOPS_APPS_PORTHOLE_COMPANION_H */
