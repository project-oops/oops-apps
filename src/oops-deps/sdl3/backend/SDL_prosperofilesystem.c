/*
 * SDL3's filesystem and path-operations backend, over `oops/fs.h`.
 *
 * Eleven symbols in two groups. `SDL_FILESYSTEM_PRIVATE` is the five that answer "where
 * does a program's data live"; `SDL_FSOPS_PRIVATE` is the six that act on a path.
 *
 * # Where the paths come from, and why two of them are the same directory
 *
 * A payload has one place it can reliably write -
 * `oops_fs_get_storage_dir(OOPS_STORAGE_APP_DATA)`, which resolves to `/data/<app id>`
 * and creates it. The package it was loaded from is read-only or, for a homebrew title,
 * not reachable at all through `/app0` (Craft measured that). So:
 *
 *   base path   the writable storage directory. SDL says this is "where the application
 * was run from", which on a desktop is the executable's directory and is usually where
 * the assets are. Here the assets are in the package and the package may not be
 *               openable, so this answers with the one directory that is always there
 * and always works. A port that needs its *assets* wants a path of its own, not this.
 *   pref path   the same directory with `<org>/<app>` under it, created. That is what
 * SDL promises - a writable place, per application - and it is exactly right.
 *
 * **Both are `SDL_malloc`'d and the caller frees**, which is SDL's convention for every
 * one of these and is easy to get wrong in the direction that leaks.
 */
#include "SDL_internal.h"

#include "oops/fs.h"

#if defined(SDL_FILESYSTEM_PRIVATE) || defined(SDL_FSOPS_PRIVATE)
#include "filesystem/SDL_sysfilesystem.h"
#endif

#ifdef SDL_FILESYSTEM_PRIVATE

/* The writable root, resolved once. `oops_fs_get_storage_dir` creates it, so a caller
 * can write into the answer without checking first. */
static bool oops_sdl_storage_root(char *out, size_t max) {
    if (oops_fs_get_storage_dir(OOPS_STORAGE_APP_DATA, out, max) != 0) {
        SDL_SetError("oops_fs_get_storage_dir() failed");
        return false;
    }
    return true;
}

char *SDL_SYS_GetBasePath(void) {
    char root[512];

    if (!oops_sdl_storage_root(root, sizeof(root))) {
        return NULL;
    }
    /* SDL's base path always ends in a separator; every one of its own backends appends
     * one. */
    return SDL_strdup(root[0] && root[SDL_strlen(root) - 1] == '/'
                          ? root
                          : (SDL_strlcat(root, "/", sizeof(root)), root));
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app) {
    char path[512];
    char root[512];

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!oops_sdl_storage_root(root, sizeof(root))) {
        return NULL;
    }
    /* `org` is allowed to be NULL or empty, and SDL's own backends then leave that
     * level out rather than creating a directory called "". */
    if (org && *org) {
        (void)SDL_snprintf(path, sizeof(path), "%s/%s", root, org);
        (void)oops_fs_mkdir(path, 0755);
        (void)SDL_snprintf(path, sizeof(path), "%s/%s/%s/", root, org, app);
    } else {
        (void)SDL_snprintf(path, sizeof(path), "%s/%s/", root, app);
    }
    /* Created without the trailing separator, then handed back with it - `mkdir` on a
     * path ending in `/` is accepted by some kernels and refused by others, and this
     * one is not worth finding out about at run time. */
    {
        char made[512];
        SDL_strlcpy(made, path, sizeof(made));
        {
            const size_t n = SDL_strlen(made);
            if (n > 0 && made[n - 1] == '/') {
                made[n - 1] = '\0';
            }
        }
        (void)oops_fs_mkdir(made, 0755);
    }
    return SDL_strdup(path);
}

/*
 * **Every folder is the one writable directory, and that is a compromise worth
 * naming.**
 *
 * `SDL_FOLDER_DOCUMENTS`, `SDL_FOLDER_SCREENSHOTS`, `SDL_FOLDER_SAVEDGAMES` and the
 * rest are a desktop's idea of where a user keeps things. This console has no such
 * places a payload can reach: there is one directory it may write to, and the system
 * software's own Screenshots and Saved Data are not it.
 *
 * The alternative was to fail for every folder, which SDL allows. That would be more
 * precise and less useful: a program asking where to put a screenshot would get nothing
 * and either give up or write somewhere worse. Answering with the writable directory
 * means the file lands somewhere real and somewhere the user can retrieve it, which is
 * what the question was actually about.
 *
 * So the divergence is: **folders are not distinguished**. A program that writes to two
 * of these expecting two directories gets one.
 */
char *SDL_SYS_GetUserFolder(SDL_Folder folder) {
    char root[512];

    (void)folder;
    if (!oops_sdl_storage_root(root, sizeof(root))) {
        return NULL;
    }
    (void)SDL_strlcat(root, "/", sizeof(root));
    return SDL_strdup(root);
}

/*
 * **A payload has one working directory and cannot change it**, which is the same
 * finding `common/posix`'s `chdir` records. `/` is the truthful answer: it is where a
 * path without a leading separator is resolved from.
 */
char *SDL_SYS_GetCurrentDirectory(void) {
    return SDL_strdup("/");
}

/*
 * **Fails, because there is no answer that is not a guess.** SDL's own backends read
 * `/proc/self/exe` or the equivalent; this platform has no such link - `common/posix`'s
 * `readlink` fails with `EINVAL` for exactly this reason, and PhysFS and OpenAL Soft
 * both fall back when it does. Returning the package path instead would be inventing a
 * name for a file the caller could not then open.
 */
char *SDL_SYS_GetExeName(void) {
    SDL_Unsupported();
    return NULL;
}

#endif /* SDL_FILESYSTEM_PRIVATE */

#ifdef SDL_FSOPS_PRIVATE

bool SDL_SYS_EnumerateDirectory(const char *path, SDL_EnumerateDirectoryCallback cb,
                                void *userdata) {
    oops_dir_t *dir = oops_fs_opendir(path);
    oops_dirent_t ent;
    int rc;

    if (!dir) {
        return SDL_SetError("Couldn't open directory '%s'", path);
    }
    while ((rc = oops_fs_readdir(dir, &ent)) == 1) {
        /* `oops_fs_readdir` returns `.` and `..` deliberately - they are directory
         * entries, and hiding them would be the SDK deciding what a listing means.
         * SDL's contract is the other way round: its callers do not expect them, and
         * every one of its backends filters them. */
        if (SDL_strcmp(ent.name, ".") == 0 || SDL_strcmp(ent.name, "..") == 0) {
            continue;
        }
        {
            const SDL_EnumerationResult result = cb(userdata, path, ent.name);
            if (result != SDL_ENUM_CONTINUE) {
                (void)oops_fs_closedir(dir);
                return (result == SDL_ENUM_SUCCESS);
            }
        }
    }
    (void)oops_fs_closedir(dir);
    /* -1 is a read failure, 0 is the end of the directory - which the API distinguishes
     * precisely so that this can too. */
    if (rc < 0) {
        return SDL_SetError("Error reading directory '%s'", path);
    }
    return true;
}

/*
 * **`oops/fs.h` declares `oops_file_info_t` and nothing fills it in.** The type is
 * there - a size and an `is_directory` - with no `oops_fs_stat` to produce one, so this
 * asks the two questions separately: `oops_fs_opendir` succeeding is what "directory"
 * means here, and `oops_fs_file_size` answers for a file. That costs an extra call per
 * path, which SDL's globbing does a lot of; an `oops_fs_stat` in the SDK would collapse
 * it, and the type waiting for one suggests that was the intention.
 */
static bool oops_sdl_is_directory(const char *path) {
    oops_dir_t *dir = oops_fs_opendir(path);
    if (!dir) {
        return false;
    }
    (void)oops_fs_closedir(dir);
    return true;
}

bool SDL_SYS_GetPathInfo(const char *path, SDL_PathInfo *info) {
    const bool is_dir = oops_sdl_is_directory(path);

    if (!is_dir && !oops_fs_exists(path)) {
        return SDL_SetError("Couldn't stat '%s'", path);
    }
    if (info) {
        SDL_zerop(info);
        info->type = is_dir ? SDL_PATHTYPE_DIRECTORY : SDL_PATHTYPE_FILE;
        if (!is_dir) {
            const int64_t size = oops_fs_file_size(path);
            info->size = (Uint64)(size < 0 ? 0 : size);
        }
        /* **The three timestamps stay zero.** The SDK's filesystem answers existence,
         * size and directory-ness and has no call for a modification time - the same
         * absence `common/posix`'s `struct stat` records. Zero is 1970 rather than
         * "unknown", and there is no third value in the interface to mean the latter; a
         * caller comparing two files' times will find them equal. Named here so that
         * whoever hits it looks in the right place. */
    }
    return true;
}

bool SDL_SYS_RemovePath(const char *path) {
    /* `oops_fs_unlink` removes a file; there is no `rmdir`, so a directory fails rather
     * than silently doing nothing. SDL_RemovePath's contract is that removing a
     * non-empty directory fails anyway, and an empty one here is a gap rather than a
     * success. */
    if (oops_fs_unlink(path) != 0) {
        return SDL_SetError("Couldn't remove '%s'", path);
    }
    return true;
}

bool SDL_SYS_RenamePath(const char *oldpath, const char *newpath) {
    if (oops_fs_rename(oldpath, newpath) != 0) {
        return SDL_SetError("Couldn't rename '%s' to '%s'", oldpath, newpath);
    }
    return true;
}

/*
 * Read the whole file and write it back out. There is no copy call underneath and no
 * sendfile, so this is the honest implementation rather than a wrapper - and it is
 * bounded by memory, which is the one thing to know about it: `oops_fs_read_all`
 * allocates the whole file. SDL's callers copy configuration and save files; a caller
 * copying something large should not use this.
 */
bool SDL_SYS_CopyFile(const char *oldpath, const char *newpath) {
    void *data = NULL;
    size_t size = 0;
    bool ok;

    if (oops_fs_read_all(oldpath, &data, &size) != 0) {
        return SDL_SetError("Couldn't read '%s'", oldpath);
    }
    ok = (oops_fs_write_all(newpath, data, size) == 0);
    oops_fs_free_data(data);
    if (!ok) {
        return SDL_SetError("Couldn't write '%s'", newpath);
    }
    return true;
}

bool SDL_SYS_CreateDirectory(const char *path) {
    if (oops_fs_mkdir(path, 0755) != 0) {
        /* **An existing directory is success**, which is SDL_CreateDirectory's
         * documented behaviour and not something to report as a failure. The SDK
         * answers a sign rather than an errno, so this asks the filesystem instead of
         * guessing from the return. */
        if (oops_sdl_is_directory(path)) {
            return true;
        }
        return SDL_SetError("Couldn't create directory '%s'", path);
    }
    return true;
}

#endif /* SDL_FSOPS_PRIVATE */
