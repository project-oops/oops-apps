/*
 * porthole_companion_selftest.c - Host self-test for porthole-companion.
 *
 * Validates UI state machine, menu navigation, and off-screen rendering without hardware.
 */

#include "porthole_companion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_initialization(void) {
    porthole_companion_state_t st;
    porthole_companion_init(&st);

    assert(st.status == PORTHOLE_STATUS_UNKNOWN);
    assert(strcmp(st.ip_address, "127.0.0.1") == 0);
    assert(st.video_port == 9805);
    assert(st.input_port == 9806);
    assert(st.selected_item == MENU_ACTION_LAUNCH_RESTART);
    assert(st.is_downloading == 0);
    assert(st.elfldr_running == 0);
    assert(st.shsrv_running == 0);
    printf("  [pass] test_initialization\n");
}

static void test_menu_navigation(void) {
    porthole_companion_state_t st;
    porthole_companion_init(&st);

    assert(st.selected_item == MENU_ACTION_LAUNCH_RESTART);

    /* Down */
    porthole_companion_nav_down(&st);
    assert(st.selected_item == MENU_ACTION_DOWNLOAD_PAYLOAD);

    porthole_companion_nav_down(&st);
    assert(st.selected_item == MENU_ACTION_TEST_KEYFRAME);

    porthole_companion_nav_down(&st);
    assert(st.selected_item == MENU_ACTION_REFRESH_STATUS);

    /* Wrap-around */
    porthole_companion_nav_down(&st);
    assert(st.selected_item == MENU_ACTION_LAUNCH_RESTART);

    /* Up wrap-around */
    porthole_companion_nav_up(&st);
    assert(st.selected_item == MENU_ACTION_REFRESH_STATUS);

    porthole_companion_nav_up(&st);
    assert(st.selected_item == MENU_ACTION_TEST_KEYFRAME);
    printf("  [pass] test_menu_navigation\n");
}

static void test_surface_rendering(void) {
    const uint32_t width = 1920;
    const uint32_t height = 1080;
    uint32_t *pixels = (uint32_t *)calloc(width * height, sizeof(uint32_t));
    assert(pixels != NULL);

    oops_surface_t surf = {
        .pixels = pixels,
        .width = width,
        .height = height,
        .pitch = width,
        .layout = OOPS_SURFACE_LINEAR
    };

    porthole_companion_state_t st;
    porthole_companion_init(&st);
    porthole_companion_set_ip(&st, "192.168.1.205");

    /* Render STOPPED state */
    porthole_companion_set_status(&st, PORTHOLE_STATUS_STOPPED, "Daemon not running on ports 9805/9806.");
    porthole_companion_render(&st, &surf);

    /* Assert that screen was drawn (not all zero) */
    uint64_t non_zero_count = 0;
    for (size_t i = 0; i < width * height; i++) {
        if (pixels[i] != 0) non_zero_count++;
    }
    assert(non_zero_count > (width * height / 2));

    /* Render RUNNING state with elfldr */
    st.elfldr_running = 1;
    st.shsrv_running = 0;
    porthole_companion_set_status(&st, PORTHOLE_STATUS_RUNNING, "Streaming Annex-B H.264 at 1080p60.");
    st.has_data_payload = 1;
    porthole_companion_render(&st, &surf);

    /* Render STOPPED state with shsrv active */
    st.elfldr_running = 0;
    st.shsrv_running = 1;
    porthole_companion_set_status(&st, PORTHOLE_STATUS_STOPPED, "Ready via root shell :2323.");
    porthole_companion_render(&st, &surf);

    /* Render DOWNLOADING state */
    st.is_downloading = 1;
    st.dl_pct = 65;
    porthole_companion_render(&st, &surf);

    free(pixels);
    printf("  [pass] test_surface_rendering\n");
}

int main(void) {
    printf("porthole-companion selftest: running suite\n");
    test_initialization();
    test_menu_navigation();
    test_surface_rendering();
    printf("porthole-companion selftest: all checks passed cleanly.\n");
    return 0;
}
