/* The gallery pages, a pure function of gallery_state_t (see gallery.h). */
#include "gallery.h"

#include "app_ui.h"
#include "oops/draw.h"
#include "oops/freestd.h"

#define BG 0xFF0D1116u
#define ACCENT OOPS_COLOR_CYAN
#define LABEL OOPS_COLOR_GRAY
#define VALUE OOPS_COLOR_WHITE

static const char *page_title(int page) {
    switch (page) {
    case GALLERY_PAGE_SHAPES:
        return "draw";
    case GALLERY_PAGE_INPUT:
        return "input";
    case GALLERY_PAGE_SYSTEM:
        return "system";
    case GALLERY_PAGE_AUDIO:
        return "audio";
    case GALLERY_PAGE_NET:
        return "network";
    case GALLERY_PAGE_CAPS:
        return "capabilities";
    case GALLERY_PAGE_RUNTIME:
        return "runtime";
    default:
        return "?";
    }
}

/* The frame every page shares: background, title, a rule, and the page counter. */
static void frame(oops_surface_t *surf, int page) {
    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS gallery", ACCENT, 4);
    oops_draw_text(surf, 400, 52, page_title(page), VALUE, 3);
    app_ui_version(surf, -1, 56);

    oops_draw_rect(surf, 48, 96, (int)surf->width - 96, 3, ACCENT);
    char footer[32];
    oops_snprintf(footer, sizeof(footer), "%d/%d", page + 1, gallery_page_count());
    oops_draw_text(surf, 48, (int)surf->height - 56, footer, LABEL, 2);
    oops_draw_text(surf, 160, (int)surf->height - 56, "L1/R1 to page, circle to exit",
                   LABEL, 2);
}

/* draw page: a horizontal gradient, the primitives, and text at three scales. */
static void page_shapes(oops_surface_t *surf) {
    const int top = 130;
    for (int x = 48; x < (int)surf->width - 48; x++) {
        uint8_t t = (uint8_t)(((x - 48) * 255) / ((int)surf->width - 96));
        oops_color_t c =
            0xFF000000u | ((uint32_t)t << 16) | ((uint32_t)(255 - t) << 8) | 0x80u;
        oops_draw_rect(surf, x, top, 1, 80, c);
    }
    oops_draw_rect(surf, 48, top + 110, 120, 80, OOPS_COLOR_RED);
    oops_draw_circle(surf, 260, top + 150, 44, OOPS_COLOR_GREEN, 1);
    oops_draw_line(surf, 340, top + 110, 520, top + 190, OOPS_COLOR_YELLOW);
    oops_draw_text(surf, 48, top + 220, "scale 2", VALUE, 2);
    oops_draw_text(surf, 48, top + 250, "scale 3", VALUE, 3);
    oops_draw_text(surf, 48, top + 290, "scale 4", VALUE, 4);
}

static void page_input(oops_surface_t *surf, const oops_pad_state_t *pad) {
    char num[16];
    int y = 140;
    y = app_ui_row(surf, y, "connected", pad->connected ? "yes" : "no");
    /* Buttons as a hex mask rather than a glyph per button. */
    oops_snprintf(num, sizeof(num), "0x%08x", (unsigned)pad->buttons);
    y = app_ui_row(surf, y, "buttons", num);
    oops_snprintf(num, sizeof(num), "%d", (int)pad->left_stick_x);
    y = app_ui_row(surf, y, "left stick x", num);
    oops_snprintf(num, sizeof(num), "%d", (int)pad->left_stick_y);
    y = app_ui_row(surf, y, "left stick y", num);
    oops_snprintf(num, sizeof(num), "%u", (unsigned)pad->l2_trigger);
    y = app_ui_row(surf, y, "L2", num);
    oops_snprintf(num, sizeof(num), "%u", (unsigned)pad->r2_trigger);
    y = app_ui_row(surf, y, "R2", num);
    (void)y;
}

static void page_system(oops_surface_t *surf, const oops_system_info_t *info) {
    char num[16];
    int y = 140;
    const char *gen = info->generation == 5   ? "Prospero"
                      : info->generation == 4 ? "Orbis"
                                              : "unknown";
    y = app_ui_row(surf, y, "generation", gen);
    y = app_ui_row(surf, y, "firmware",
                   info->firmware_str[0] ? info->firmware_str : "-");
    oops_snprintf(num, sizeof(num), "%u", (unsigned)info->total_ram_mb);
    y = app_ui_row(surf, y, "memory MB", num);
    y = app_ui_row(surf, y, "user", info->user_name[0] ? info->user_name : "-");
    (void)y;
}

static void page_audio(oops_surface_t *surf, int open) {
    int y = 140;
    y = app_ui_row(surf, y, "port", open ? "open" : "closed");
    oops_draw_text(surf, 48, y + 8,
                   open ? "hold Cross (X) to play 440Hz test tone" : "no audio port",
                   LABEL, 2);
}

static void page_net(oops_surface_t *surf, int linked, const char *ip) {
    int y = 140;
    y = app_ui_row(surf, y, "link", linked ? "up" : "down");
    y = app_ui_row(surf, y, "address", ip ? ip : "-");
    (void)y;
}

/* A capability row: label on the left, a green "available" or a red "absent" on the
 * right, so the whole matrix reads at a glance. */
static int caps_row(oops_surface_t *surf, int y, const char *label, int available) {
    oops_draw_text(surf, 48, y, label, LABEL, 3);
    oops_draw_text(surf, 420, y, available ? "available" : "absent",
                   available ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 3);
    return y + 42;
}

/* capabilities page: what this console offers the SDK's media-decode and input-device
 * subsystems, from the oops_*_available() calls the payload gathered. */
static void page_caps(oops_surface_t *surf, const gallery_caps_t *c) {
    int y = 132;
    y = caps_row(surf, y, "AGC GPU tiler", c->agc_gpu);
    y = caps_row(surf, y, "video decode", c->videodec);
    y = caps_row(surf, y, "audio decode", c->audiodec);
    y = caps_row(surf, y, "  offload (AJM)", c->audiodec_offload);
    y = caps_row(surf, y, "keyboard", c->keyboard);
    y = caps_row(surf, y, "mouse", c->mouse);
    y = caps_row(surf, y, "adaptive triggers", c->adaptive_triggers);
    oops_draw_text(surf, 48, y + 10,
                   "available = library and entry points resolved on this console",
                   LABEL, 2);
}

static void page_runtime(oops_surface_t *surf, const gallery_runtime_t *r) {
    char buf[32];
    int y = 132;
    y = caps_row(surf, y, "heap: slab allocator (mmap)", 1);
    oops_snprintf(buf, sizeof(buf), "%u KB", (unsigned)(r->heap_allocated / 1024));
    y = app_ui_row(surf, y, "  allocated virtual", buf);
    y = caps_row(surf, y, "filesystem: /data mount", r->fs_ready);
    y = caps_row(surf, y, "math: 3D linear algebra (trig, mat4)", r->math_ready);
    y = caps_row(surf, y, "network: RFC 1035 UDP DNS", r->dns_ready);
    oops_draw_text(surf, 48, y + 16,
                   "freestanding runtime: zero DMEM dependency, category 65536 safe",
                   LABEL, 2);
}

int gallery_page_count(void) {
    return GALLERY_PAGE_COUNT;
}

int gallery_wrap_page(int page) {
    int n = GALLERY_PAGE_COUNT;
    int p = page % n;
    if (p < 0) {
        p += n;
    }
    return p;
}

int gallery_render(oops_surface_t *surf, const gallery_state_t *state) {
    if (!surf || !state) {
        return -1;
    }
    int page = gallery_wrap_page(state->page);
    frame(surf, page);
    switch (page) {
    case GALLERY_PAGE_SHAPES:
        page_shapes(surf);
        break;
    case GALLERY_PAGE_INPUT:
        page_input(surf, &state->pad);
        break;
    case GALLERY_PAGE_SYSTEM:
        page_system(surf, &state->system);
        break;
    case GALLERY_PAGE_AUDIO:
        page_audio(surf, state->audio_open);
        break;
    case GALLERY_PAGE_NET:
        page_net(surf, state->net_linked, state->net_ip);
        break;
    case GALLERY_PAGE_CAPS:
        page_caps(surf, &state->caps);
        break;
    case GALLERY_PAGE_RUNTIME:
        page_runtime(surf, &state->runtime);
        break;
    default:
        break;
    }
    return page;
}
