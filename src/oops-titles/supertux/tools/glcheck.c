/*
 * supertux glcheck - every GL entry point SuperTux's renderer calls, against oops-gl's
 * surface.
 *
 * A GL function oops-gl lacks is a link error, so this reports it first;
 * `make shadercheck` covers the shaders. It measures the `-DUSE_OPENGLES2` path, under
 * which `gl20_context.cpp` and `gl_pixel_request.cpp` compile to nothing and
 * `video/gl.hpp` makes the vertex-array-object calls inline no-ops
 * (`STUBBED_BY_UPSTREAM`).
 *
 * It reads upstream's renderer sources and oops-sdk's `GL/gl.h` at run time, and checks
 * declarations, not behaviour. Calls inside comments and `#if 0` are ignored
 * (`gl_texture.cpp` disables `glGenerateMipmap` that way).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STX_GL_HEADER
#define STX_GL_HEADER "../../../../oops-sdk/include/GL/gl.h"
#endif

/* The renderer's files, named rather than globbed so that each one's reach under the
 * ES 2.0 build is a recorded decision. */
typedef enum {
    STX_ES2 = 0, /* compiled, and reached, on the ES 2.0 path this port takes */
    STX_DROPPED, /* compiled to nothing under USE_OPENGLES2 - see the note beside each
                    one */
} stx_reach_t;

typedef struct {
    const char *file;
    stx_reach_t reach;
} stx_source_t;

static const stx_source_t SOURCES[] = {
    /* The fixed-function backend, removed by upstream's `#ifndef USE_OPENGLES2`. */
    {"gl20_context.cpp", STX_DROPPED},
    {"gl33core_context.cpp", STX_ES2},
    {"gl_framebuffer.cpp", STX_ES2},
    {"gl_painter.cpp", STX_ES2},
    /* Emptied by `#ifndef USE_OPENGLES2`; its only use, in `gl_painter.cpp`, is also
     * inside `#if 0`. */
    {"gl_pixel_request.cpp", STX_DROPPED},
    {"gl_program.cpp", STX_ES2},
    {"gl_renderer.cpp", STX_ES2},
    {"gl_screen_renderer.cpp", STX_ES2},
    {"gl_shader.cpp", STX_ES2},
    {"gl_texture.cpp", STX_ES2},
    {"gl_texture_renderer.cpp", STX_ES2},
    {"gl_vertex_arrays.cpp", STX_ES2},
    {"gl_video_system.cpp", STX_ES2},
};

/*
 * Called, but never a link reference: under `USE_OPENGLES2`, `video/gl.hpp` defines
 * these as empty inline functions because ES 2.0 has no vertex array objects.
 */
static const char *const STUBBED_BY_UPSTREAM[] = {
    "glBindVertexArray",
    "glDeleteVertexArrays",
    "glGenVertexArrays",
};

/*
 * The baseline of entry points the ES 2.0 path calls and oops-gl lacks. The tool fails
 * when the absent set changes in either direction, so a new call in an upstream bump, a
 * removed entry point, or a closed gap each need this list updated. Empty means every
 * entry point is present.
 */
static const char *const KNOWN_GAPS[] = {
    NULL, /* the array is never empty in C; the loop below skips a NULL */
};

#define MAX_NAMES 512
#define MAX_NAME_LEN 64

typedef struct {
    char name[MAX_NAME_LEN];
    int es2;     /* called from a file the ES 2.0 build compiles */
    int dropped; /* called from a file the ES 2.0 build compiles to nothing */
    int have;    /* declared by oops-sdk's GL/gl.h */
} stx_entry_t;

static int is_stubbed(const char *name) {
    for (size_t k = 0; k < sizeof(STUBBED_BY_UPSTREAM) / sizeof(STUBBED_BY_UPSTREAM[0]);
         k++) {
        if (strcmp(STUBBED_BY_UPSTREAM[k], name) == 0)
            return 1;
    }
    return 0;
}

static stx_entry_t g_entries[MAX_NAMES];
static size_t g_entry_count;

static int g_overflow;

/* ------------------------------------------------------------------------- */

static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    const long end = ftell(f);
    if (end < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    const size_t len = (size_t)end;
    char *buf = (char *)malloc(len + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    const size_t got = fread(buf, 1u, len, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len)
        *out_len = got;
    return buf;
}

/*
 * Blanks what is not code, in place and preserving newlines: `//` and block comments,
 * string and character literals, and `#if 0` regions.
 *
 * The `#if 0` pass counts nested conditionals so that an `#ifdef` inside a disabled
 * block does not end it early. It is lexical and evaluates no macros.
 */
static void blank_non_code(char *s) {
    size_t i = 0;
    while (s[i] != '\0') {
        if (s[i] == '/' && s[i + 1u] == '/') {
            while (s[i] != '\0' && s[i] != '\n')
                s[i++] = ' ';
        } else if (s[i] == '/' && s[i + 1u] == '*') {
            s[i++] = ' ';
            s[i++] = ' ';
            while (s[i] != '\0' && !(s[i] == '*' && s[i + 1u] == '/')) {
                if (s[i] != '\n')
                    s[i] = ' ';
                i++;
            }
            if (s[i] != '\0') {
                s[i++] = ' ';
                s[i++] = ' ';
            }
        } else if (s[i] == '"' || s[i] == '\'') {
            const char quote = s[i];
            s[i++] = ' ';
            while (s[i] != '\0' && s[i] != quote) {
                if (s[i] == '\\' && s[i + 1u] != '\0')
                    s[i++] = ' ';
                if (s[i] != '\n')
                    s[i] = ' ';
                i++;
            }
            if (s[i] != '\0')
                s[i++] = ' ';
        } else {
            i++;
        }
    }

    /* `#if 0` regions, now that comments cannot hide a directive. */
    i = 0;
    while (s[i] != '\0') {
        if (s[i] == '#' && (i == 0u || s[i - 1u] == '\n' || s[i - 1u] == ' ')) {
            size_t j = i + 1u;
            while (s[j] == ' ' || s[j] == '\t')
                j++;
            if (strncmp(&s[j], "if", 2u) == 0) {
                size_t k = j + 2u;
                while (s[k] == ' ' || s[k] == '\t')
                    k++;
                if (s[k] == '0' &&
                    (s[k + 1u] == '\n' || s[k + 1u] == ' ' || s[k + 1u] == '\r')) {
                    int depth = 1;
                    size_t p = k;
                    while (s[p] != '\0' && depth > 0) {
                        if (s[p] == '#') {
                            size_t q = p + 1u;
                            while (s[q] == ' ' || s[q] == '\t')
                                q++;
                            if (strncmp(&s[q], "if", 2u) == 0)
                                depth++;
                            else if (strncmp(&s[q], "endif", 5u) == 0)
                                depth--;
                        }
                        if (s[p] != '\n')
                            s[p] = ' ';
                        p++;
                    }
                    i = p;
                    continue;
                }
            }
        }
        i++;
    }
}

static int is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '_';
}

static stx_entry_t *lookup(const char *name, size_t len) {
    if (len >= MAX_NAME_LEN)
        return NULL;
    for (size_t i = 0; i < g_entry_count; i++) {
        if (strncmp(g_entries[i].name, name, len) == 0 &&
            g_entries[i].name[len] == '\0') {
            return &g_entries[i];
        }
    }
    return NULL;
}

static stx_entry_t *intern(const char *name, size_t len) {
    if (len >= MAX_NAME_LEN)
        return NULL;
    stx_entry_t *found = lookup(name, len);
    if (found)
        return found;
    if (g_entry_count >= MAX_NAMES) {
        g_overflow = 1;
        return NULL;
    }
    stx_entry_t *e = &g_entries[g_entry_count++];
    memcpy(e->name, name, len);
    e->name[len] = '\0';
    e->es2 = 0;
    e->dropped = 0;
    e->have = 0;
    return e;
}

/*
 * Every `gl<Upper>...` token followed by `(`, so a call or a declaration. `GL_`
 * enumerants do not match, because the character after `gl` must be upper-case.
 *
 * `adding` says whether an unseen name joins the table. The source pass adds and the
 * header pass only marks: `GL/gl.h` declares enough entry points to fill the table, so
 * interning them would crowd out upstream's calls and hide the gaps.
 */
static void scan(char *text, stx_reach_t reach, int adding, int *out_count) {
    size_t i = 0;
    int n = 0;
    while (text[i] != '\0') {
        if (text[i] == 'g' && text[i + 1u] == 'l' && text[i + 2u] >= 'A' &&
            text[i + 2u] <= 'Z' && (i == 0u || !is_ident_char(text[i - 1u]))) {
            size_t j = i + 2u;
            while (is_ident_char(text[j]))
                j++;
            size_t k = j;
            while (text[k] == ' ' || text[k] == '\t' || text[k] == '\n' ||
                   text[k] == '\r')
                k++;
            if (text[k] == '(') {
                if (adding) {
                    stx_entry_t *e = intern(&text[i], j - i);
                    if (e) {
                        if (reach == STX_DROPPED)
                            e->dropped = 1;
                        else
                            e->es2 = 1;
                    }
                    n++;
                } else {
                    stx_entry_t *e = lookup(&text[i], j - i);
                    if (e)
                        e->have = 1;
                    n++;
                }
            }
            i = j;
        } else {
            i++;
        }
    }
    if (out_count)
        *out_count = n;
}

static int cmp_entry(const void *a, const void *b) {
    const stx_entry_t *ea = (const stx_entry_t *)a;
    const stx_entry_t *eb = (const stx_entry_t *)b;
    return strcmp(ea->name, eb->name);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "upstream/src/video/gl";
    const char *header = argc > 2 ? argv[2] : STX_GL_HEADER;

    printf("supertux glcheck: %s against %s\n", dir, header);

    /* The renderer's sources first, because they fix the table's contents (see `scan`).
     * A file that will not open is a failure, not a zero. */
    char path[1024];
    int missing_files = 0;
    for (size_t s = 0; s < sizeof(SOURCES) / sizeof(SOURCES[0]); s++) {
        (void)snprintf(path, sizeof(path), "%s/%s", dir, SOURCES[s].file);
        size_t len = 0;
        char *text = read_file(path, &len);
        if (!text) {
            printf("  %-26s MISSING\n", SOURCES[s].file);
            missing_files++;
            continue;
        }
        blank_non_code(text);
        int n = 0;
        scan(text, SOURCES[s].reach, 1, &n);
        free(text);
        const char *tag =
            SOURCES[s].reach == STX_DROPPED ? "   (empty under USE_OPENGLES2)" : "";
        printf("  %-26s %3d call sites%s\n", SOURCES[s].file, n, tag);
    }
    if (g_entry_count == 0u) {
        printf("supertux glcheck: FAILED - no GL calls found under %s\n", dir);
        return 1;
    }

    /* Then oops-gl's surface, marking what it has. A header that will not open or
     * declares nothing is a failure, since a wrong path would otherwise report
     * everything absent. */
    size_t hlen = 0;
    char *htext = read_file(header, &hlen);
    if (!htext) {
        printf("supertux glcheck: FAILED - cannot read %s\n", header);
        return 1;
    }
    blank_non_code(htext);
    int declared = 0;
    scan(htext, STX_ES2, 0, &declared);
    free(htext);
    if (declared == 0) {
        printf("supertux glcheck: FAILED - no entry points found in %s\n", header);
        return 1;
    }

    qsort(g_entries, g_entry_count, sizeof(g_entries[0]), cmp_entry);

    /* What the ES 2.0 path needs and oops-gl lacks. Stubbed names and dropped files are
     * listed separately because neither reaches the link. */
    int need = 0, gap = 0, gapstub = 0, gapdrop = 0;
    int unexpected = 0, closed = 0;

    printf("\n  absent from oops-gl, on the ES 2.0 path:\n");
    for (size_t i = 0; i < g_entry_count; i++) {
        const stx_entry_t *e = &g_entries[i];
        if (!e->es2 || is_stubbed(e->name))
            continue;
        need++;
        if (e->have)
            continue;
        gap++;
        int known = 0;
        for (size_t k = 0; k < sizeof(KNOWN_GAPS) / sizeof(KNOWN_GAPS[0]); k++) {
            if (KNOWN_GAPS[k] == NULL)
                continue;
            if (strcmp(KNOWN_GAPS[k], e->name) == 0) {
                known = 1;
                break;
            }
        }
        if (!known)
            unexpected++;
        printf("    %-28s %s\n", e->name, known ? "" : "<- NEW, not in KNOWN_GAPS");
    }
    if (gap == 0)
        printf("    (none)\n");

    /* The other direction: a gap the baseline names but oops-gl now has. */
    for (size_t k = 0; k < sizeof(KNOWN_GAPS) / sizeof(KNOWN_GAPS[0]); k++) {
        if (KNOWN_GAPS[k] == NULL)
            continue;
        const stx_entry_t *e = lookup(KNOWN_GAPS[k], strlen(KNOWN_GAPS[k]));
        if (e && e->es2 && e->have) {
            printf("    %-28s <- now present; drop it from KNOWN_GAPS\n",
                   KNOWN_GAPS[k]);
            closed++;
        }
    }

    printf("\n  absent, but inline no-ops under USE_OPENGLES2 (video/gl.hpp):\n");
    for (size_t i = 0; i < g_entry_count; i++) {
        const stx_entry_t *e = &g_entries[i];
        if (!e->es2 || !is_stubbed(e->name) || e->have)
            continue;
        printf("    %s\n", e->name);
        gapstub++;
    }
    if (gapstub == 0)
        printf("    (none)\n");

    /* A stubbed name oops-gl declares is a compile error: `gl.hpp`'s C++ inline no-ops
     * cannot coexist with oops-gl's `extern "C"` declarations in one translation unit.
     * The shim's `SDL_opengles2.h` must keep them apart. */
    int stubclash = 0;
    for (size_t i = 0; i < g_entry_count; i++) {
        const stx_entry_t *e = &g_entries[i];
        if (!e->es2 || !is_stubbed(e->name) || !e->have)
            continue;
        printf("    %-28s <- oops-gl declares it now; upstream's inline stub will "
               "collide\n",
               e->name);
        stubclash++;
    }

    printf("\n  absent, but only in files the ES 2.0 build empties:\n");
    for (size_t i = 0; i < g_entry_count; i++) {
        const stx_entry_t *e = &g_entries[i];
        if (e->es2 || !e->dropped || e->have)
            continue;
        printf("    %s\n", e->name);
        gapdrop++;
    }
    if (gapdrop == 0)
        printf("    (none)\n");

    if (g_overflow) {
        printf("\nsupertux glcheck: FAILED - more than %d distinct names; raise "
               "MAX_NAMES\n",
               MAX_NAMES);
        return 1;
    }
    if (missing_files) {
        printf("\nsupertux glcheck: FAILED - %d renderer source(s) missing; upstream "
               "moved\n",
               missing_files);
        return 1;
    }

    printf("\nsupertux glcheck: %d of %d entry points on the ES 2.0 path are in oops-gl"
           " (surface only - behaviour is gl2-probe's)\n",
           need - gap, need);

    /* Passing means the absent set matches KNOWN_GAPS, not that it is empty. */
    if (stubclash) {
        printf("supertux glcheck: FAILED - %d vertex-array stub(s) now collide with "
               "oops-gl;"
               " keep them out of shim/include/SDL_opengles2.h\n",
               stubclash);
        return 1;
    }
    if (unexpected || closed) {
        printf("supertux glcheck: FAILED - the absent set moved (%d new, %d closed);"
               " update KNOWN_GAPS and docs/PORTING.md\n",
               unexpected, closed);
        return 1;
    }
    if (gap == 0) {
        printf("supertux glcheck: every entry point the ES 2.0 path calls is in "
               "oops-gl\n");
    } else {
        printf("supertux glcheck: %d known gap(s) - unchanged\n", gap);
    }
    return 0;
}
