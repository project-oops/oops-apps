/*
 * supertux glcheck - every GL entry point SuperTux's renderer calls, against oops-gl's
 * surface.
 *
 * **This is half of the question "can this title render at all"**; the other half is
 * whether `data/shader/shader100.*` compiles, which `oops-sdk/tools/shader-survey.sh`
 * answers and `make shadercheck` runs. This half is the entry points: a GL function
 * oops-gl does not have is a
 * **link error** rather than a slow path, which is also why the port answers
 * `<SDL_opengles2.h>` from its shim instead of pulling in a loader - a missing function
 * should be a named link failure, not a null pointer called in frame one.
 *
 * **The path measured is the ES 2.0 one**, which the port builds with
 * `-DUSE_OPENGLES2`. That define is upstream's own switch and it does three things this
 * tool has to know: it makes `GL33CoreContext` the backend with `shader100.*` as its
 * program, it compiles `gl20_context.cpp` and `gl_pixel_request.cpp` to nothing, and it
 * replaces the three vertex-array-object calls with inline no-ops in `video/gl.hpp`.
 * The last is why those three names are in `STUBBED_BY_UPSTREAM` rather than reported
 * as gaps.
 *
 * It reads both trees at run time - upstream's renderer sources and oops-sdk's own
 * `GL/gl.h` - so it measures them as they are and re-runs whenever either moves.
 *
 * **It measures surface, not behaviour.** A declared entry point can still be wrong,
 * and that is gl1-probe's job rather than this one's. What this catches is the class of
 * gap that stops a link dead, which is the class that decides whether a port is
 * possible at all.
 *
 * **Two lexical hazards are handled, because without them the tool reports needs that
 * are not real.** Calls inside comments are not calls, and neither are calls inside
 * `#if 0` - and upstream has one that matters: `gl_texture.cpp` wraps
 * `glGenerateMipmap` in `#if 0` under the comment "Disable the use of mipmaps for the
 * texture". A tool that counted it would put an entry point on the required list that
 * the game never calls, and the first person to read the report would go and implement
 * it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STX_GL_HEADER
#define STX_GL_HEADER "../../../../oops-sdk/include/GL/gl.h"
#endif

/* The renderer's own files. Named rather than globbed so that a new file upstream shows
 * up as a failure here - which is a prompt to read it and decide whether the ES 2.0
 * build compiles it - instead of being silently swept into whichever column a glob
 * happened to put it in. */
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
    /* **The fixed-function backend, and upstream's `#ifndef USE_OPENGLES2` removes
     * it.** It is what this port would fall back to if the shader path ever had to be
     * given up: build without the define and `GLVideoSystem(false)` takes it. Its needs
     * are a subset of gl1-probe's, so it is not measured here. */
    {"gl20_context.cpp", STX_DROPPED},
    {"gl33core_context.cpp", STX_ES2},
    {"gl_framebuffer.cpp", STX_ES2},
    {"gl_painter.cpp", STX_ES2},
    /* **Dropped twice over.** `#ifndef USE_OPENGLES2` empties it, and even without the
     * define its only use is in `gl_painter.cpp` inside an `#if 0` whose comment reads
     * "FIXME: glFenceSync() causes crashes on Intel I965, so disable GLPixelRequest for
     * now". Its `glFenceSync`/`glClientWaitSync` are GL 3.2 and never reached. */
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
 * **Called, and never a link reference.** Under `USE_OPENGLES2`, `video/gl.hpp` defines
 * these as empty inline functions - "a simple no-op replacement looks prettier than
 * #ifdef", upstream says
 * - because ES 2.0 has no vertex array objects. The calls stay in the source and
 * compile to nothing, so oops-gl not having them costs this port nothing.
 */
static const char *const STUBBED_BY_UPSTREAM[] = {
    "glBindVertexArray",
    "glDeleteVertexArrays",
    "glGenVertexArrays",
};

/*
 * **What is absent today, so that a change to either tree is what this reports.**
 *
 * Without a baseline the only honest exit code is "there are gaps", which is true every
 * run and tells CI nothing; with one, the tool is quiet while the answer is unchanged
 * and speaks when it moves - in *either* direction. Closing one of these should fail
 * here, because the thing that then needs updating is the port's status rather than
 * this list.
 *
 * **Empty since 2026-09-25**, and this comment is what the list used to say: the four
 * framebuffer entry points were the port's one real gap. `GLTextureRenderer` renders
 * the lightmap into a texture and `GLVideoSystem` creates it unconditionally - outside
 * any test of which backend is in use - so it was not a GL33Core luxury the GL 2.0 path
 * could avoid.
 *
 * oops-gl has them now, and draws into a texture on the console: `gl2-probe`'s
 * `fbo/texture`, measured the same day. This tool failing is how that was noticed here,
 * which is the direction a baseline is *also* for - it reported four gaps closed and
 * refused to stay quiet about it.
 *
 * An empty list means every entry point the ES 2.0 path calls is present. It is still a
 * baseline: a new call in an upstream bump, or an entry point that goes away, fails
 * this.
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
 * **Blanks what is not code**, in place and preserving newlines so that nothing shifts:
 * `//` and block comments, string and character literals, and `#if 0` regions.
 *
 * The `#if 0` pass counts nested conditionals so that an `#ifdef` inside a disabled
 * block does not end it early. It is a lexical approximation rather than a preprocessor
 * - which is the right size for the question, because the alternative is evaluating
 * macros this tool has no reason to know about.
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
 * Every `gl<Upper>...` token that is **followed by `(`** - so a call or a declaration,
 * rather than a name that merely appears. `GL_`-prefixed enumerants do not match,
 * because the character after `gl` must be an upper-case letter and `_` is not one.
 *
 * `adding` says whether an unseen name joins the table. **The source pass adds and the
 * header pass does not**, and the order is load-bearing rather than incidental:
 * oops-sdk's `GL/gl.h` declares several hundred entry points, so a header pass that
 * interned would fill the table before a single renderer source was read. That is not a
 * hypothetical - it is what the first version of this tool did, and the failure was
 * silent and clean-looking: every name upstream called that oops-gl *lacks* failed to
 * intern, `gl_framebuffer.cpp` scored zero call sites, and the gap list printed
 * "(none)". A tool that reports no gaps because it ran out of room is worse than no
 * tool, so the table now only ever holds what upstream actually calls, and the header
 * can only mark those.
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

    /* **The renderer's sources first**, because they are what fixes the table's
     * contents - see `scan`. A file that will not open is a failure and not a zero: an
     * upstream that moved its renderer should stop this tool, not quietly report
     * nothing. */
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

    /* Then oops-gl's surface, marking what it has. A header that will not open, or one
     * that turns out to declare nothing, is a failure rather than a report that
     * everything is absent - which is the shape a wrong path would otherwise take. */
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

    /* The report proper: what the ES 2.0 path needs and does not have. The stubbed
     * names and the dropped files are listed separately rather than counted against
     * this port, because neither reaches the link. */
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

    /* The other direction: a gap the baseline still names but oops-gl now has. Quiet
     * success here would leave the list - and the port's status beside it - slowly
     * becoming fiction. */
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

    /* **A stubbed name oops-gl starts declaring is a compile error, not good news.**
     * `gl.hpp`'s no-ops are C++ inline definitions, and oops-gl's are `extern "C"`
     * declarations of the same name - the two cannot coexist in one translation unit.
     * The shim's `SDL_opengles2.h` is where that gets answered. */
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

    /* **Passing means "unchanged", not "no gaps".** The gaps are this title's status
     * and are recorded in KNOWN_GAPS; what would be news is the set moving. */
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
