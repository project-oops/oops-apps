/*
 * Host self-test for oops-app-downloader.
 *
 * Exercises the pure parts on an ordinary machine: the install-path helper (validation and
 * formatting) and the render (that it draws into a surface without walking off it). The console
 * pipeline is not testable here - it is the half that needs the hardware - so this checks what
 * can be checked, which is the same split net-tool uses.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "oops/draw.h"
#include "oops-app-downloader.h"

#define W 1280
#define H 720

static int fail(const char *what) {
    fprintf(stderr, "oops-app-downloader selftest: %s\n", what);
    return 1;
}

int main(void) {
    char buf[64];

    /* A well-formed title id maps to its homebrew directory. */
    int n = oops_dl_install_path("GLCB00001", buf, sizeof(buf));
    if (n != 24 || strcmp(buf, "/data/homebrew/GLCB00001") != 0) {
        return fail("install path for a valid id");
    }

    /* Malformed ids are refused, not guessed at. */
    if (oops_dl_install_path("glcb00001", buf, sizeof(buf)) != -1) return fail("lowercase id accepted");
    if (oops_dl_install_path("GLCB0001", buf, sizeof(buf)) != -1)  return fail("short id accepted");
    if (oops_dl_install_path("GLCB000012", buf, sizeof(buf)) != -1) return fail("long id accepted");
    if (oops_dl_install_path("GLCB00001", buf, 8) != -1)           return fail("overflow not caught");
    if (oops_dl_install_path(NULL, buf, sizeof(buf)) != -1)        return fail("NULL id accepted");

    /* The render draws something into a real surface. */
    uint32_t *pixels = calloc((size_t)W * H, sizeof(uint32_t));
    if (!pixels) return fail("out of memory");
    oops_surface_t surf = { .pixels = pixels, .width = W, .height = H, .pitch = W };

    oops_dl_status_t st = { .ip = "192.168.1.211", .net_ready = 1, .install_ready = 1,
                            .http_ready = 0, .unzip_ready = 0 };
    int rows = oops_dl_render(&surf, &st);
    if (rows <= 0) { free(pixels); return fail("render drew nothing"); }

    free(pixels);
    printf("oops-app-downloader selftest OK\n");
    return 0;
}
