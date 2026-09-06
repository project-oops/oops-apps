/*
 * Host self-test for save-browser.
 *
 * Checks the two things that are pure logic and easy to get wrong: the list draws, and the
 * cursor moves and scrolls within bounds. The mounting of a real disk is the console-only
 * half and is not exercised here.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/display.h"
#include "oops/draw.h"

#include "save-browser.h"

uint32_t *oops_display_get_framebuffer(oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_width(const oops_display_t *disp) { (void)disp; return 0; }
unsigned int oops_display_get_height(const oops_display_t *disp) { (void)disp; return 0; }

#define W 1280
#define H 720

static void make_list(save_entry_t *e, int count) {
    for (int i = 0; i < count; i++) {
        snprintf(e[i].name, sizeof(e[i].name), "save-%02d", i);
        strcpy(e[i].user, "player one");
        e[i].size_kb = (uint32_t)((i + 1) * 128);
    }
}

int main(void) {
    uint32_t *pixels = calloc((size_t)W * H, sizeof(uint32_t));
    if (!pixels) {
        return 1;
    }
    oops_surface_t surf = { .pixels = pixels, .width = W, .height = H, .pitch = W };

    int ok = 1;
    const int count = 40;
    save_entry_t entries[40];
    make_list(entries, count);

    int visible = save_browser_visible_rows(H);
    if (visible < 1 || visible > count) {
        fprintf(stderr, "save-browser selftest: visible rows %d is unreasonable\n", visible);
        ok = 0;
    }

    save_browser_view_t view = { 0, 0 };

    /* Selection clamps at the top: moving up from 0 stays at 0. */
    save_browser_move(&view, count, visible, -1);
    if (view.selected != 0 || view.scroll != 0) {
        fprintf(stderr, "save-browser selftest: moved past the top\n");
        ok = 0;
    }

    /* Move to the end: selection clamps and the scroll follows so it stays on screen. */
    for (int i = 0; i < count + 5; i++) {
        save_browser_move(&view, count, visible, 1);
    }
    if (view.selected != count - 1) {
        fprintf(stderr, "save-browser selftest: did not clamp at the end (%d)\n", view.selected);
        ok = 0;
    }
    if (view.selected < view.scroll || view.selected >= view.scroll + visible) {
        fprintf(stderr, "save-browser selftest: selection scrolled off screen\n");
        ok = 0;
    }

    int drawn = save_browser_render(&surf, entries, count, &view);
    if (drawn < 1 || drawn > visible) {
        fprintf(stderr, "save-browser selftest: drew %d rows, out of range\n", drawn);
        ok = 0;
    }
    int any = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        if (pixels[i] == 0xFF00FFFFu) { any = 1; break; }
    }
    if (!any) {
        fprintf(stderr, "save-browser selftest: nothing drew\n");
        ok = 0;
    }

    /* The empty case says so rather than drawing a blank list. */
    memset(pixels, 0, (size_t)W * H * sizeof(uint32_t));
    view.selected = 0;
    view.scroll = 0;
    if (save_browser_render(&surf, entries, 0, &view) != 0) {
        fprintf(stderr, "save-browser selftest: empty list drew rows\n");
        ok = 0;
    }

    free(pixels);
    if (ok) {
        printf("save-browser selftest: ok (list draws; cursor clamps and scrolls in bounds)\n");
        return 0;
    }
    return 1;
}
