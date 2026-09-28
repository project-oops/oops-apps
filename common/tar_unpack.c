/*
 * `tar_unpack.h` says what this reads and why.
 */
#include "tar_unpack.h"

#include "oops/freestd.h"
#include "oops/fs.h"
#include "oops/system.h"

#define TAR_BLOCK 512u
#define TAR_PATH_MAX 1024u

/* A copy buffer, static because a data tree's files are unpacked one at a time. */
static unsigned char s_copy[64u * 1024u];

static int tar_read_full(int fd, void *buf, size_t n) {
    size_t got = 0u;
    while (got < n) {
        const int64_t r = oops_fs_read(fd, (unsigned char *)buf + got, n - got);
        if (r <= 0) {
            return -1;
        }
        got += (size_t)r;
    }
    return 0;
}

/* An octal field, NUL- or space-terminated, as ustar writes sizes and modes. */
static uint64_t tar_octal(const char *field, size_t len) {
    uint64_t v = 0u;
    size_t i = 0u;
    while (i < len && field[i] == ' ') {
        i++;
    }
    for (; i < len && field[i] >= '0' && field[i] <= '7'; i++) {
        v = (v << 3) | (uint64_t)(field[i] - '0');
    }
    return v;
}

static uint64_t tar_padded(uint64_t size) {
    return (size + TAR_BLOCK - 1u) / TAR_BLOCK * TAR_BLOCK;
}

/* Refuses an absolute path and any `..` component: an entry must stay inside `dest`. */
static int tar_path_is_safe(const char *p) {
    if (p[0] == '/' || p[0] == '\0') {
        return 0;
    }
    for (;;) {
        if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == '\0')) {
            return 0;
        }
        while (*p && *p != '/') {
            p++;
        }
        if (*p == '\0') {
            return 1;
        }
        p++;
    }
}

/* Every directory along `path`, which ends in a file name that is not made. */
static void tar_make_parents(char *path) {
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            (void)oops_fs_mkdir(path, 0755);
            *p = '/';
        }
    }
}

static int tar_skip(int fd, uint64_t n) {
    return oops_fs_seek(fd, (int64_t)n, 1 /* SEEK_CUR */) < 0 ? -1 : 0;
}

static int tar_copy_out(int in, const char *path, uint64_t size) {
    const int out =
        oops_fs_open(path, OOPS_O_WRONLY | OOPS_O_CREAT | OOPS_O_TRUNC, 0644);
    uint64_t left = size;

    if (out < 0) {
        oops_log_error("TAR", "cannot create %s", path);
        return -1;
    }
    while (left > 0u) {
        const size_t n = left < sizeof(s_copy) ? (size_t)left : sizeof(s_copy);
        if (tar_read_full(in, s_copy, n) != 0 ||
            oops_fs_write(out, s_copy, n) != (int64_t)n) {
            oops_log_error("TAR", "short copy writing %s", path);
            (void)oops_fs_close(out);
            return -1;
        }
        left -= n;
    }
    (void)oops_fs_close(out);
    return tar_skip(in, tar_padded(size) - size);
}

int oops_tar_unpack_once(const char *archive, const char *dest, const char *marker,
                         oops_tar_progress_fn progress, void *user) {
    unsigned char hdr[TAR_BLOCK];
    char longname[TAR_PATH_MAX];
    char name[TAR_PATH_MAX];
    char full[TAR_PATH_MAX + 64u];
    oops_tar_progress_t state = {0u, 0u, 0u, (const char *)0};
    size_t files = 0u;
    int have_long = 0;
    int fd;

    if (oops_fs_exists(marker)) {
        return 0;
    }
    fd = oops_fs_open(archive, OOPS_O_RDONLY, 0);
    if (fd < 0) {
        oops_log_error("TAR", "%s is missing: the title's data ships in it", archive);
        return -1;
    }
    oops_log_info("TAR", "unpacking %s into %s", archive, dest);
    (void)oops_fs_mkdir(dest, 0755);
    {
        const int64_t total = oops_fs_file_size(archive);
        state.bytes_total = total > 0 ? (uint64_t)total : 0u;
    }

    for (;;) {
        size_t zero = 0u;
        uint64_t size;
        const char *rel = name;
        char type;

        if (tar_read_full(fd, hdr, sizeof(hdr)) != 0) {
            oops_log_error("TAR", "%s ends without an end-of-archive block", archive);
            (void)oops_fs_close(fd);
            return -1;
        }
        while (zero < sizeof(hdr) && hdr[zero] == 0u) {
            zero++;
        }
        if (zero == sizeof(hdr)) {
            break; /* the end-of-archive block */
        }
        size = tar_octal((const char *)hdr + 124, 12u);
        type = (char)hdr[156];

        if (type == 'L') { /* GNU: the next entry's name, which did not fit */
            if (size >= sizeof(longname) ||
                tar_read_full(fd, longname, (size_t)size) != 0 ||
                tar_skip(fd, tar_padded(size) - size) != 0) {
                oops_log_error("TAR", "a long name in %s is malformed", archive);
                (void)oops_fs_close(fd);
                return -1;
            }
            longname[size] = '\0';
            have_long = 1;
            continue;
        }

        if (have_long) {
            (void)oops_snprintf(name, sizeof(name), "%s", longname);
            have_long = 0;
        } else if (hdr[345] != 0u) { /* ustar: prefix, then name */
            (void)oops_snprintf(name, sizeof(name), "%.155s/%.100s",
                                (const char *)hdr + 345, (const char *)hdr);
        } else {
            (void)oops_snprintf(name, sizeof(name), "%.100s", (const char *)hdr);
        }
        {
            const char *p = name;
            while (p[0] == '.' && p[1] == '/') {
                p += 2; /* `./data/x`, as `tar -C dir .` writes it */
            }
            if (*p == '\0') {
                if (tar_skip(fd, tar_padded(size)) != 0)
                    break;
                continue;
            }
            if (!tar_path_is_safe(p)) {
                oops_log_error("TAR", "refusing %s: it leaves the destination", name);
                (void)oops_fs_close(fd);
                return -1;
            }
            (void)oops_snprintf(full, sizeof(full), "%s/%s", dest, p);
            rel = p;
        }

        if (type == '5') {
            tar_make_parents(full);
            (void)oops_fs_mkdir(full, 0755);
            if (tar_skip(fd, tar_padded(size)) != 0)
                break;
        } else if (type == '0' || type == '\0') {
            tar_make_parents(full);
            if (tar_copy_out(fd, full, size) != 0) {
                (void)oops_fs_close(fd);
                return -1;
            }
            files++;
            if (progress) {
                const int64_t at = oops_fs_seek(fd, 0, 1 /* SEEK_CUR */);
                state.files_done = files;
                state.bytes_done = at > 0 ? (uint64_t)at : state.bytes_done;
                state.name = rel;
                progress(&state, user);
            }
        } else {
            oops_log_info("TAR",
                          "skipping %s: entry type '%c' is not a file or directory",
                          name, type);
            if (tar_skip(fd, tar_padded(size)) != 0)
                break;
        }
    }
    (void)oops_fs_close(fd);

    {
        const int m =
            oops_fs_open(marker, OOPS_O_WRONLY | OOPS_O_CREAT | OOPS_O_TRUNC, 0644);
        if (m < 0) {
            oops_log_error("TAR", "unpacked %zu files but cannot write %s", files,
                           marker);
            return -1;
        }
        (void)oops_fs_close(m);
    }
    oops_log_info("TAR", "unpacked %zu files from %s", files, archive);
    return 0;
}
