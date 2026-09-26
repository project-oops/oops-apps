/*
 * SDL's filesystem backend: `SDL_GetBasePath` and `SDL_GetPrefPath`.
 *
 * The base path is `/app0/`, where the payload's package is mounted read-only. The pref
 * path is `/data/<app>/`, created on demand and ending in a separator as SDL documents.
 * `org` is ignored: the app identifier is already unique, and SDL documents the layout
 * as platform-dependent.
 */
#include "SDL_internal.h"

#ifdef SDL_FILESYSTEM_PROSPERO

#include "SDL_error.h"
#include "SDL_filesystem.h"

#include "oops/fs.h"

#define PROSPERO_BASE_PATH "/app0/"
#define PROSPERO_DATA_ROOT "/data/"

char *SDL_GetBasePath(void) {
    return SDL_strdup(PROSPERO_BASE_PATH);
}

char *SDL_GetPrefPath(const char *org, const char *app) {
    char *path;
    size_t len;

    (void)org;

    if (!app || !*app) {
        SDL_InvalidParamError("app");
        return NULL;
    }

    /* root + app + '/' + NUL */
    len = SDL_strlen(PROSPERO_DATA_ROOT) + SDL_strlen(app) + 2;
    path = (char *)SDL_malloc(len);
    if (!path) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_snprintf(path, len, "%s%s/", PROSPERO_DATA_ROOT, app);

    /*
     * An existing directory counts as success, so a failed mkdir is an error only when
     * nothing is there afterwards.
     */
    if (oops_fs_mkdir(path, 0777) != 0 && !oops_fs_exists(path)) {
        SDL_free(path);
        SDL_SetError("prospero: could not make a writable directory for %s", app);
        return NULL;
    }

    return path;
}

#endif /* SDL_FILESYSTEM_PROSPERO */
