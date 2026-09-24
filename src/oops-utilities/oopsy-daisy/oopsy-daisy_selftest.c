/*
 * Host self-test for OOPSy-daisy - the pure parts, on an ordinary machine.
 * The console pipeline (HTTPS + unzip) is the hardware half and is not exercised here.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/draw.h"
#include "oopsy-daisy.h"

#define W 1280
#define H 720

static int fail(const char *what) {
    fprintf(stderr, "oopsy-daisy selftest: %s\n", what);
    return 1;
}

/* A trimmed GitHub "release by tag" body: two title zips, plus an eboot and an elf that must
 * be ignored, and a url with a slash in the path to check basename handling. */
static const char *SAMPLE =
    "{\"tag_name\":\"latest-main\",\"assets\":[\n"
    "{\"name\":\"neverball-title-prospero.zip\",\"size\":123,"
    "\"browser_download_url\":\"https://github.com/project-oops/oops-apps/releases/download/latest-main/neverball-title-prospero.zip\"},\n"
    "{\"name\":\"gl1-cube-prospero.elf\",\"size\":9,"
    "\"browser_download_url\":\"https://github.com/project-oops/oops-apps/releases/download/latest-main/gl1-cube-prospero.elf\"},\n"
    "{\"name\":\"gl1-cube-eboot-prospero.bin\",\"size\":9,"
    "\"browser_download_url\":\"https://github.com/project-oops/oops-apps/releases/download/latest-main/gl1-cube-eboot-prospero.bin\"},\n"
    "{\"name\":\"gl1-cube-title-prospero.zip\",\"size\":9,"
    "\"browser_download_url\":\"https://github.com/project-oops/oops-apps/releases/download/latest-main/gl1-cube-title-prospero.zip\"}\n"
    "]}";

int main(void) {
    oopsy_catalog_t cat;

    int n = oopsy_parse_catalog(SAMPLE, &cat);
    if (n != 2) return fail("expected 2 title zips");
    if (strcmp(cat.items[0].name, "neverball") != 0) return fail("first name");
    if (strcmp(cat.items[1].name, "gl1-cube") != 0) return fail("second name");
    if (strcmp(cat.items[0].url,
               "https://github.com/project-oops/oops-apps/releases/download/latest-main/neverball-title-prospero.zip") != 0)
        return fail("first url");

    /* No assets, empty, and NULL are all just "nothing to install". */
    if (oopsy_parse_catalog("{\"assets\":[]}", &cat) != 0) return fail("empty assets");
    if (oopsy_parse_catalog(NULL, &cat) != 0) return fail("null json");

    /* install path helper */
    char buf[64];
    if (oopsy_install_path("GLCB00001", buf, sizeof(buf)) != 24 ||
        strcmp(buf, "/data/homebrew/GLCB00001") != 0)
        return fail("install path");
    if (oopsy_install_path("nope", buf, sizeof(buf)) != -1) return fail("bad id accepted");

    /* the screen draws */
    uint32_t *px = calloc((size_t)W * H, sizeof(uint32_t));
    if (!px) return fail("out of memory");
    oops_surface_t surf = { .pixels = px, .width = W, .height = H, .pitch = W };
    oopsy_parse_catalog(SAMPLE, &cat);
    oopsy_view_t v = { .cat = &cat, .selected = 1, .phase = OOPSY_BROWSE, .message = NULL };
    if (oopsy_render(&surf, &v) <= 0) { free(px); return fail("render drew nothing"); }
    free(px);

    printf("oopsy-daisy selftest OK\n");
    return 0;
}
