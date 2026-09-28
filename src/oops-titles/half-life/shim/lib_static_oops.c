/*
 * The engine's library loader for a static build, in place of upstream's
 * `engine/platform/misc/lib_static.c`.
 *
 * Each game library - the filesystem, the renderer, the menu, the client and the server
 * - is linked into the payload as one relocatable object whose only global symbol is
 * its export table, `lib_<name>_exports`. `generated_library_tables.h` lists those
 * tables by library name. Loading a library is finding its table; resolving a function
 * is finding a name in it.
 *
 * Two things differ from upstream's version:
 *
 * - **Libraries are matched by base name.** The engine asks for a library the way it
 *   would on disk - `cl_dlls/client.so`, `dlls/hl.so`, `filesystem_stdio.so` - and
 *   upstream's table matches the string whole. Here the directory and the extension are
 *   dropped first, so the table carries one name per library whatever path the engine
 *   builds. A name that matches nothing is logged, because that is the one failure this
 *   file can cause and it would otherwise read as "library missing" with no hint why.
 *
 * - **`COM_NameForFunction` answers.** Save games record entity callbacks - think,
 * touch, use - by name, and resolve them by name on load. Upstream's static version
 * returns NULL, which makes every save that holds a callback fail. The client and
 * server tables here are generated from every function their objects define, mangled
 * C++ names included - the names `dladdr` gives on a desktop - so the reverse lookup is
 * a search of the same table.
 */
#include "platform/platform.h"
#include "library.h"

#if XASH_LIB == LIB_STATIC

typedef struct table_s {
    const char *name;
    void *pointer;
} table_t;

#include "generated_library_tables.h"

static void *Lib_Find(table_t *tbl, const char *name) {
    while (tbl->name) {
        if (!Q_strcmp(tbl->name, name)) {
            return tbl->pointer;
        }
        tbl++;
    }
    return 0;
}

/* `cl_dlls/client.so` -> `client`. */
static void Lib_BaseName(const char *path, char *out, size_t size) {
    const char *base = path;
    const char *p;
    size_t n = 0;

    for (p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    while (base[n] && base[n] != '.' && n + 1 < size) {
        out[n] = base[n];
        n++;
    }
    out[n] = '\0';
}

void *COM_LoadLibrary(const char *dllname, int build_ordinals_table,
                      qboolean directpath) {
    char base[64];
    void *lib;

    (void)build_ordinals_table;
    (void)directpath;

    Lib_BaseName(dllname, base, sizeof(base));
    lib = Lib_Find((table_t *)libs, base);
    if (!lib) {
        Con_Printf(S_ERROR "%s: no library named \"%s\" is linked into this build\n",
                   __func__, base);
    }
    return lib;
}

void COM_FreeLibrary(void *hInstance) {
    /* Linked in; there is nothing to unload. */
    (void)hInstance;
}

void *COM_GetProcAddress(void *hInstance, const char *name) {
    return hInstance ? Lib_Find(hInstance, name) : NULL;
}

void *COM_GetProcAddressFromDependency(void *hInstance, const char *depname,
                                       const char *name) {
    (void)hInstance;
    (void)depname;
    (void)name;
    return NULL;
}

void *COM_FunctionFromName(void *hInstance, const char *pName) {
    return hInstance ? Lib_Find(hInstance, pName) : NULL;
}

const char *COM_NameForFunction(void *hInstance, void *function) {
    table_t *tbl = hInstance;

    if (!tbl) {
        return NULL;
    }
    for (; tbl->name; tbl++) {
        if (tbl->pointer == function) {
            return tbl->name;
        }
    }
    return NULL;
}

#endif /* XASH_LIB == LIB_STATIC */
