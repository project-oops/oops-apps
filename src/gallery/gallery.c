#include "gallery.h"

#include "oops/draw.h"

/* Freestanding number formatting, so this file compiles for the target as well as the host. */
static const char *u32_dec(uint32_t value, char *out) {
    char rev[11];
    int n = 0;
    do {
        rev[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && n < 10);
    int i = 0;
    while (n > 0) {
        out[i++] = rev[--n];
    }
    out[i] = '\0';
    return out;
}

static const char *i32_dec(int value, char *out) {
    if (value < 0) {
        out[0] = '-';
        u32_dec((uint32_t)(-(long)value), out + 1);
        return out;
    }
    return u32_dec((uint32_t)value, out);
}

#define BG 0xFF0D1116u
#define ACCENT OOPS_COLOR_CYAN
#define LABEL OOPS_COLOR_GRAY
#define VALUE OOPS_COLOR_WHITE

static const char *page_title(int page) {
    switch (page) {
        case GALLERY_PAGE_SHAPES: return "draw";
        case GALLERY_PAGE_INPUT:  return "input";
        case GALLERY_PAGE_SYSTEM: return "system";
        case GALLERY_PAGE_AUDIO:  return "audio";
        case GALLERY_PAGE_NET:    return "network";
        case GALLERY_PAGE_CAPS:   return "capabilities";
        case GALLERY_PAGE_RUNTIME: return "runtime";
        default:                  return "?";
    }
}

/* The frame every page shares: background, title, a rule, and the page counter. */
static void frame(oops_surface_t *surf, int page) {
    char num[11];
    oops_draw_clear(surf, BG);
    oops_draw_text(surf, 48, 40, "OOPS gallery", ACCENT, 4);
    oops_draw_text(surf, 400, 52, page_title(page), VALUE, 3);
    oops_draw_rect(surf, 48, 96, (int)surf->width - 96, 3, ACCENT);
    /* page N of M, bottom-left */
    char footer[32];
    int k = 0;
    const char *p = u32_dec((uint32_t)page + 1u, num);
    while (*p) footer[k++] = *p++;
    footer[k++] = '/';
    p = u32_dec((uint32_t)gallery_page_count(), num);
    while (*p) footer[k++] = *p++;
    footer[k] = '\0';
    oops_draw_text(surf, 48, (int)surf->height - 56, footer, LABEL, 2);
    oops_draw_text(surf, 160, (int)surf->height - 56, "L1/R1 to page, circle to exit", LABEL, 2);
}

/* draw page: a horizontal gradient, the primitives, and text at three scales. */
static void page_shapes(oops_surface_t *surf) {
    const int top = 130;
    for (int x = 48; x < (int)surf->width - 48; x++) {
        uint8_t t = (uint8_t)(((x - 48) * 255) / ((int)surf->width - 96));
        oops_color_t c = 0xFF000000u | ((uint32_t)t << 16) | ((uint32_t)(255 - t) << 8) | 0x80u;
        oops_draw_rect(surf, x, top, 1, 80, c);
    }
    oops_draw_rect(surf, 48, top + 110, 120, 80, OOPS_COLOR_RED);
    oops_draw_circle(surf, 260, top + 150, 44, OOPS_COLOR_GREEN, 1);
    oops_draw_line(surf, 340, top + 110, 520, top + 190, OOPS_COLOR_YELLOW);
    oops_draw_text(surf, 48, top + 220, "scale 2", VALUE, 2);
    oops_draw_text(surf, 48, top + 250, "scale 3", VALUE, 3);
    oops_draw_text(surf, 48, top + 290, "scale 4", VALUE, 4);
}

static int row(oops_surface_t *surf, int y, const char *label, const char *value) {
    oops_draw_text(surf, 48, y, label, LABEL, 3);
    oops_draw_text(surf, 380, y, value, VALUE, 3);
    return y + 44;
}

static void page_input(oops_surface_t *surf, const oops_pad_state_t *pad) {
    char num[12];
    int y = 140;
    y = row(surf, y, "connected", pad->connected ? "yes" : "no");
    /* Buttons as a hex mask - enough to see something change without a glyph per button. */
    char hex[11];
    uint32_t b = pad->buttons;
    hex[0] = '0';
    hex[1] = 'x';
    for (int i = 0; i < 8; i++) {
        uint32_t nyb = (b >> ((7 - i) * 4)) & 0xF;
        hex[2 + i] = (char)(nyb < 10 ? '0' + nyb : 'a' + (nyb - 10));
    }
    hex[10] = '\0';
    y = row(surf, y, "buttons", hex);
    y = row(surf, y, "left stick x", i32_dec(pad->left_stick_x, num));
    y = row(surf, y, "left stick y", i32_dec(pad->left_stick_y, num));
    y = row(surf, y, "L2", u32_dec(pad->l2_trigger, num));
    y = row(surf, y, "R2", u32_dec(pad->r2_trigger, num));
    (void)y;
}

static void page_system(oops_surface_t *surf, const oops_system_info_t *info) {
    char num[11];
    int y = 140;
    const char *gen = info->generation == 5 ? "Prospero"
                      : info->generation == 4 ? "Orbis"
                                              : "unknown";
    y = row(surf, y, "generation", gen);
    y = row(surf, y, "firmware", info->firmware_str[0] ? info->firmware_str : "-");
    y = row(surf, y, "memory MB", u32_dec((uint32_t)info->total_ram_mb, num));
    y = row(surf, y, "user", info->user_name[0] ? info->user_name : "-");
    (void)y;
}

static void page_audio(oops_surface_t *surf, int open) {
    int y = 140;
    y = row(surf, y, "port", open ? "open" : "closed");
    oops_draw_text(surf, 48, y + 8, open ? "hold Cross (X) to play 440Hz test tone" : "no audio port",
                   LABEL, 2);
}

static void page_net(oops_surface_t *surf, int linked, const char *ip) {
    int y = 140;
    y = row(surf, y, "link", linked ? "up" : "down");
    y = row(surf, y, "address", ip ? ip : "-");
    (void)y;
}

/* A capability row: label on the left, a green "available" or a red "absent" on the right, so
 * the whole matrix reads at a glance. */
static int caps_row(oops_surface_t *surf, int y, const char *label, int available) {
    oops_draw_text(surf, 48, y, label, LABEL, 3);
    oops_draw_text(surf, 420, y, available ? "available" : "absent",
                   available ? OOPS_COLOR_GREEN : OOPS_COLOR_RED, 3);
    return y + 42;
}

/* capabilities page: what this console offers the SDK's media-decode and input-device
 * subsystems, straight from the oops_*_available() calls the payload gathered. */
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
                   "available = library and entry points resolved on this console", LABEL, 2);
}

static void page_runtime(oops_surface_t *surf, const gallery_runtime_t *r) {
    char num[16];
    char buf[32];
    int y = 132;
    y = caps_row(surf, y, "heap: slab allocator (mmap)", 1);
    u32_dec((uint32_t)(r->heap_allocated / 1024), num);
    int k = 0;
    const char *p = num;
    while (*p) buf[k++] = *p++;
    buf[k++] = ' '; buf[k++] = 'K'; buf[k++] = 'B'; buf[k] = '\0';
    y = row(surf, y, "  allocated virtual", buf);
    y = caps_row(surf, y, "filesystem: /data mount", r->fs_ready);
    y = caps_row(surf, y, "math: 3D linear algebra (trig, mat4)", r->math_ready);
    y = caps_row(surf, y, "network: RFC 1035 UDP DNS", r->dns_ready);
    oops_draw_text(surf, 48, y + 16,
                   "freestanding runtime: zero DMEM dependency, category 65536 safe", LABEL, 2);
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
        case GALLERY_PAGE_SHAPES:  page_shapes(surf); break;
        case GALLERY_PAGE_INPUT:   page_input(surf, &state->pad); break;
        case GALLERY_PAGE_SYSTEM:  page_system(surf, &state->system); break;
        case GALLERY_PAGE_AUDIO:   page_audio(surf, state->audio_open); break;
        case GALLERY_PAGE_NET:     page_net(surf, state->net_linked, state->net_ip); break;
        case GALLERY_PAGE_CAPS:    page_caps(surf, &state->caps); break;
        case GALLERY_PAGE_RUNTIME: page_runtime(surf, &state->runtime); break;
        default: break;
    }
    return page;
}
