/*
 * SDL3's filesystem and path-operations backend, over `oops/fs.h`.
 * `SDL_FILESYSTEM_PRIVATE` answers where a program's data lives; `SDL_FSOPS_PRIVATE`
 * acts on a path.
 *
 * The one reliably writable place is `oops_fs_get_storage_dir(OOPS_STORAGE_APP_DATA)`
 * (`/data/<app id>`, created on demand); a homebrew title's package may not be
 * reachable through `/app0`. The base path is that directory, and the pref path is
 * `<org>/<app>` under it. A port's assets need a path of their own.
 *
 * Every returned path is `SDL_malloc`'d and the caller frees it, as SDL requires.
 */
#include "SDL_internal.h"

#include "oops/fs.h"

#if defined(SDL_FILESYSTEM_PRIVATE) || defined(SDL_FSOPS_PRIVATE)
#include "filesystem/SDL_sysfilesystem.h"
#endif

#ifdef SDL_FILESYSTEM_PRIVATE

/* The writable root. `oops_fs_get_storage_dir` creates it. */
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
    /* SDL's base path always ends in a separator. */
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
    /* A NULL or empty `org` leaves that level out, as SDL's own backends do. */
    if (org && *org) {
        (void)SDL_snprintf(path, sizeof(path), "%s/%s", root, org);
        (void)oops_fs_mkdir(path, 0755);
        (void)SDL_snprintf(path, sizeof(path), "%s/%s/%s/", root, org, app);
    } else {
        (void)SDL_snprintf(path, sizeof(path), "%s/%s/", root, app);
    }
    /* Created without the trailing separator, which some kernels refuse in `mkdir`,
     * and returned with it. */
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
 * Every `SDL_Folder` answers with the one writable directory: a payload cannot reach
 * the system software's Screenshots or Saved Data, and a real directory serves a caller
 * better than a failure. Folders are not distinguished.
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
 * A payload's working directory is fixed (see `common/posix`'s `chdir`); relative
 * paths resolve from `/`.
 */
char *SDL_SYS_GetCurrentDirectory(void) {
    return SDL_strdup("/");
}

/*
 * Unsupported: there is no `/proc/self/exe` equivalent (`common/posix`'s `readlink`
 * fails with `EINVAL`), and callers such as PhysFS and OpenAL Soft fall back.
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
        /* `oops_fs_readdir` returns `.` and `..`; SDL's callers do not expect them. */
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
    /* -1 is a read failure, 0 is the end of the directory. */
    if (rc < 0) {
        return SDL_SetError("Error reading directory '%s'", path);
    }
    return true;
}

/*
 * `oops/fs.h` has no stat call, so a path is a directory when `oops_fs_opendir`
 * succeeds, and `oops_fs_file_size` answers for a file.
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
        /* The timestamps stay zero: the SDK has no modification-time call (as in
         * `common/posix`'s `struct stat`), so every file compares equal in time. */
    }
    return true;
}

bool SDL_SYS_RemovePath(const char *path) {
    /* `oops_fs_unlink` removes a file; with no `rmdir`, removing a directory fails. */
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
 * Reads the whole file and writes it back out; the SDK has no copy call. Bounded by
 * memory, because `oops_fs_read_all` allocates the whole file.
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
        /* An existing directory is success, as `SDL_CreateDirectory` documents. The
         * SDK returns no errno, so the filesystem is asked. */
        if (oops_sdl_is_directory(path)) {
            return true;
        }
        return SDL_SetError("Couldn't create directory '%s'", path);
    }
    return true;
}

#endif /* SDL_FSOPS_PRIVATE */
