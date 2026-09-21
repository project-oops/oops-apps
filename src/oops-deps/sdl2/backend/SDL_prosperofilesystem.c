/*
 * `SDL_GetBasePath` and `SDL_GetPrefPath` - the whole of SDL's filesystem backend.
 *
 * # Where a payload's own files are
 *
 * A running payload has its own package mounted at `/app0`, which is the SDK's own convention
 * and what `oops/offsets.h` and `oops/system.h` both reference by that name. That is what a
 * title means by its base path: the directory its data was shipped in, read-only.
 *
 * # Where it may write
 *
 * `/data` is the writable area. `SDL_GetPrefPath` is documented to create the directory if it is
 * not there and to return a path ending in a separator, and both are done here - a title that
 * gets a path back and then fails to write into it has been told something untrue.
 *
 * **The organisation name is dropped, deliberately.** SDL's desktop convention nests
 * `<org>/<app>` because a desktop has many vendors' programs in one home directory. Here a
 * payload is alone, its identifier is already unique, and a nested path would mean two
 * directories to create and a `mkdir` that can half-succeed. `org` is accepted and ignored,
 * which SDL permits - it documents the layout as platform-dependent.
 */
#include "SDL_internal.h"

#ifdef SDL_FILESYSTEM_PROSPERO

#include "SDL_error.h"
#include "SDL_filesystem.h"

#include "oops/fs.h"

#define PROSPERO_BASE_PATH "/app0/"
#define PROSPERO_DATA_ROOT "/data/"

char *SDL_GetBasePath(void)
{
    return SDL_strdup(PROSPERO_BASE_PATH);
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
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
     * Created without the trailing separator, because that is the directory's name. An existing
     * directory is success, so the return value is only trusted after a failure: if the mkdir
     * failed and nothing is there, the path is a promise we cannot keep and NULL is the answer.
     */
    if (oops_fs_mkdir(path, 0777) != 0 && !oops_fs_exists(path)) {
        SDL_free(path);
        SDL_SetError("prospero: could not make a writable directory for %s", app);
        return NULL;
    }

    return path;
}

#endif /* SDL_FILESYSTEM_PROSPERO */
