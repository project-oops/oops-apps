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

/* Host shims for the SDK's freestanding string helpers the pure parser calls (obs_strlen,
 * obs_strstr). On the target these come from oops-sdk's freestd.c, which the payload links; that
 * file also defines memcpy/memset, which collide with the host C library, so the host test maps
 * just these two onto libc rather than linking the whole freestanding runtime. */
size_t obs_strlen(const char *s);
char *obs_strstr(const char *haystack, const char *needle);
int obs_strcmp(const char *a, const char *b);
char *obs_strncpy(char *dest, const char *src, size_t n);
size_t obs_strlen(const char *s) { return strlen(s); }
char *obs_strstr(const char *haystack, const char *needle) { return strstr(haystack, needle); }
int obs_strcmp(const char *a, const char *b) { return strcmp(a, b); }
char *obs_strncpy(char *dest, const char *src, size_t n) { return strncpy(dest, src, n); }

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
    if (cat.items[0].size != 123) return fail("first size");
    if (cat.items[1].size != 9)   return fail("second size");

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

    /* the download queue */
    oopsy_queue_t q; q.count = 0;
    if (oopsy_queue_add(&q, &cat.items[0]) != 0)  { free(px); return fail("queue add 0"); }
    if (oopsy_queue_add(&q, &cat.items[1]) != 1)  { free(px); return fail("queue add 1"); }
    if (oopsy_queue_add(&q, &cat.items[0]) != -1) { free(px); return fail("queue dedupe"); }
    if (q.count != 2)                { free(px); return fail("queue count"); }
    if (oopsy_queue_pending(&q) != 2){ free(px); return fail("queue pending"); }
    if (oopsy_queue_active(&q) != 0) { free(px); return fail("queue active"); }
    if (q.jobs[0].total != 123)      { free(px); return fail("job total from size"); }
    if (oopsy_job_pct(&q.jobs[0]) != 0) { free(px); return fail("queued pct"); }
    q.jobs[0].state = OOPSY_JOB_DOWNLOADING; q.jobs[0].done = 61;  /* ~50% of 123 */
    { int pc = oopsy_job_pct(&q.jobs[0]); if (pc < 45 || pc > 55) { free(px); return fail("downloading pct"); } }
    q.jobs[0].state = OOPSY_JOB_DONE;
    if (oopsy_job_pct(&q.jobs[0]) != 100) { free(px); return fail("done pct"); }
    if (oopsy_queue_pending(&q) != 1)     { free(px); return fail("pending after done"); }
    if (oopsy_queue_active(&q) != 1)      { free(px); return fail("active skips done"); }

    /* the queue screen draws */
    oopsy_view_t vq = { .cat = &cat, .queue = &q, .selected = 0,
                        .phase = OOPSY_BROWSE, .screen = OOPSY_SCREEN_QUEUE, .message = NULL };
    if (oopsy_render(&surf, &vq) <= 0) { free(px); return fail("queue render drew nothing"); }
    free(px);

    printf("oopsy-daisy selftest OK\n");
    return 0;
}
