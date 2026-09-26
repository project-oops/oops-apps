#include "shader_dump.h"
#include "tracer.h"

#if defined(OOPS_HOST_BUILD)
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#else
#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/fs.h"
#endif

#define DEDUP_CAP 1024u
static uint64_t s_seen_hashes[DEDUP_CAP];
static uint32_t s_seen_count = 0;
static char s_dump_dir[256] = "/data/shaders";

tracer_hook_t g_hook_agc_create_shader = {0};

void shader_dump_set_directory(const char *dir) {
    if (dir == NULL || dir[0] == '\0') {
        return;
    }
    size_t i = 0;
    for (; i < sizeof(s_dump_dir) - 1u && dir[i] != '\0'; i++) {
        s_dump_dir[i] = dir[i];
    }
    s_dump_dir[i] = '\0';
}

const char *shader_dump_get_directory(void) {
    return s_dump_dir;
}

uint32_t shader_dump_get_count(void) {
    return s_seen_count;
}

void shader_dump_reset_cache(void) {
    s_seen_count = 0;
}

static int is_hash_seen(uint64_t hash) {
    for (uint32_t i = 0; i < s_seen_count; i++) {
        if (s_seen_hashes[i] == hash) {
            return 1;
        }
    }
    return 0;
}

static void add_hash_seen(uint64_t hash) {
    if (s_seen_count < DEDUP_CAP) {
        s_seen_hashes[s_seen_count++] = hash;
    }
}

static void hash_to_hex(uint64_t hash, char *out17) {
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 15; i >= 0; i--) {
        out17[i] = hex_chars[hash & 0x0fu];
        hash >>= 4u;
    }
    out17[16] = '\0';
}

static void make_path(char *dst, size_t dst_size, const char *dir, const char *hex,
                      const char *ext) {
    size_t dlen = 0;
    while (dir[dlen] != '\0')
        dlen++;

    size_t hlen = 0;
    while (hex[hlen] != '\0')
        hlen++;

    size_t elen = 0;
    while (ext[elen] != '\0')
        elen++;

    size_t pos = 0;
    for (size_t i = 0; i < dlen && pos + 1 < dst_size; i++) {
        dst[pos++] = dir[i];
    }
    if (pos > 0 && dst[pos - 1] != '/' && pos + 1 < dst_size) {
        dst[pos++] = '/';
    }
    for (size_t i = 0; i < hlen && pos + 1 < dst_size; i++) {
        dst[pos++] = hex[i];
    }
    for (size_t i = 0; i < elen && pos + 1 < dst_size; i++) {
        dst[pos++] = ext[i];
    }
    dst[pos] = '\0';
}

static void ensure_directory(const char *dir) {
#if defined(OOPS_HOST_BUILD)
    (void)mkdir(dir, 0777);
#else
    (void)oops_fs_mkdir(dir, 0777);
#endif
}

static int write_dump_file(const char *path, const void *data, size_t len) {
    if (path == NULL || data == NULL || len == 0) {
        return -1;
    }
#if defined(OOPS_HOST_BUILD)
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        return -1;
    }
    ssize_t written = write(fd, data, len);
    close(fd);
    return (written == (ssize_t)len) ? 0 : -1;
#else
    return oops_fs_write_all(path, data, len);
#endif
}

static uint32_t parse_shader_stage(const void *header) {
    if (header == NULL) {
        return SHADER_STAGE_CS;
    }
    const uint32_t *h = (const uint32_t *)header;
    uint32_t stage_val = h[0] & 0x0Fu;
    if (stage_val >= SHADER_STAGE_CS && stage_val <= SHADER_STAGE_HS) {
        return stage_val;
    }
    return SHADER_STAGE_CS;
}

static uint32_t parse_shader_size(const void *header, const void *payload) {
    uint32_t size = 0;
    if (header != NULL) {
        const uint32_t *h = (const uint32_t *)header;
        if (h[2] >= 16u && h[2] <= 0x200000u) {
            size = h[2];
        } else if (h[3] >= 16u && h[3] <= 0x200000u) {
            size = h[3];
        }
    }
    if (size == 0 && payload != NULL) {
        const uint32_t *dw = (const uint32_t *)payload;
        for (uint32_t i = 0; i < 16384u; i++) {
            if (dw[i] == 0xbf810000u) { /* s_endpgm */
                uint32_t raw_bytes = (i + 1u) * 4u;
                size = (raw_bytes + 15u) & ~15u;
                break;
            }
        }
    }
    if (size == 0) {
        size = 256u;
    }
    return size;
}

int hook_sceAgcCreateShader(void *shader_obj, const void *header, void *gpu_payload,
                            uint32_t flags) {
    if (gpu_payload != NULL) {
        uint32_t stage = parse_shader_stage(header);
        uint32_t size = parse_shader_size(header, gpu_payload);
        uint64_t hash = obs_trace_fnv1a((const uint8_t *)gpu_payload, size);

        if (!is_hash_seen(hash)) {
            char hex[17];
            hash_to_hex(hash, hex);

            char bin_path[256];
            char hdr_path[256];
            make_path(bin_path, sizeof(bin_path), s_dump_dir, hex, ".bin");
            make_path(hdr_path, sizeof(hdr_path), s_dump_dir, hex, ".hdr");

            ensure_directory(s_dump_dir);
            (void)write_dump_file(bin_path, gpu_payload, (size_t)size);
            if (header != NULL) {
                (void)write_dump_file(hdr_path, header, AGC_CONTAINER_HEADER_SIZE);
            }
            add_hash_seen(hash);
        }

        tracer_record_shader((uint64_t)(uintptr_t)gpu_payload, size, stage,
                             (const uint8_t *)gpu_payload);
    }

    if (g_hook_agc_create_shader.trampoline != NULL) {
        int (*real_create)(void *, const void *, void *, uint32_t) = (int (*)(
            void *, const void *, void *, uint32_t))g_hook_agc_create_shader.trampoline;
        return real_create(shader_obj, header, gpu_payload, flags);
    }
    return 0;
}
