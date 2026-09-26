/*
 * Dedicated API & GPU Shader Tracer Payload for OOPS.
 *
 * Runs inside target processes (via injector) or as a standalone diagnostic payload.
 * Records API calls, argument shapes, struct layouts, AGC shaders, and DCBs into a
 * compact binary ring buffer and dumps distinct RDNA2 shaders to disk.
 */

#include <stdint.h>
#include <stddef.h>

#include "oops/krw.h"

#if defined(OOPS_HOST_BUILD)
#include <string.h>
#include <stdio.h>
#define TRACER_LOG(msg) ((void)(msg))
#else
#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/fs.h"
#include "oops/system.h"
#include "oops/inject.h"
#define TRACER_LOG(msg) oops_klog("TRACER", msg)
#endif

static char s_trace_path[64] = "/data/trace.bin";

#include "trace_format.h"
#include "trace_encode.h"
#include "hook.h"
#include "shader_dump.h"
#include "tracer.h"

#define TRACER_MAX_RECS 8192u
#define TRACER_SAMPLER_CAP 512u
#define TRACER_FLUSH_INTERVAL 60u

typedef struct {
    uint64_t gpu_addr;
    uint32_t size;
    uint16_t flags;
    uint16_t pad;
} tracer_dcb_desc_t;

static struct obs_trace_rec s_recs[TRACER_MAX_RECS];
static uint64_t s_sampler_nids[TRACER_SAMPLER_CAP];
static uint32_t s_sampler_counts[TRACER_SAMPLER_CAP];

static struct obs_trace_buf s_buf;
static struct obs_trace_sampler s_sampler;
static int s_initialized = 0;
static uint32_t s_flip_counter = 0;
static uint32_t s_tracer_seq = 1;

tracer_hook_t g_hook_agc_submit_dcb = {0};
tracer_hook_t g_hook_video_out_flip = {0};
tracer_hook_t g_hook_apr_resolve = {0};
tracer_hook_t g_hook_mapper_param = {0};
tracer_hook_t g_hook_sce_open = {0};
tracer_hook_t g_hook_posix_open = {0};
tracer_hook_t g_hook_sce_close = {0};
tracer_hook_t g_hook_posix_close = {0};
tracer_hook_t g_hook_sce_fstat = {0};
tracer_hook_t g_hook_posix_fstat = {0};
tracer_hook_t g_hook_sysmodule_load = {0};
tracer_hook_t g_hook_alloc_direct_mem = {0};
tracer_hook_t g_hook_map_direct_mem = {0};

/* Weak reference to runtime dynamic linker on Prospero */
#if !defined(OOPS_HOST_BUILD)
static const obs_kexport_table_t *s_kexport_table = NULL;
__attribute__((weak)) int sceKernelDlsym(int handle, const char *symbol,
                                         void **address_out);
#endif

void tracer_init(struct obs_trace_rec *storage, uint32_t cap) {
    if (storage != NULL && cap > 0) {
        obs_trace_buf_init(&s_buf, storage, cap);
    } else {
        obs_trace_buf_init(&s_buf, s_recs, TRACER_MAX_RECS);
    }
    obs_trace_sampler_init(&s_sampler, s_sampler_nids, s_sampler_counts,
                           TRACER_SAMPLER_CAP, OBS_TRACE_CAP);
    s_initialized = 1;
}

void tracer_record_entry(uint16_t tid, uint32_t seq, uint64_t nid, const uint64_t *args,
                         uint8_t argc) {
    if (!s_initialized) {
        tracer_init(NULL, 0);
    }
    if (obs_trace_hit(&s_sampler, nid)) {
        obs_trace_entry(&s_buf, tid, seq, nid, args, argc);
    }
}

void tracer_record_exit(uint16_t tid, uint32_t seq, uint64_t nid, uint64_t ret) {
    if (!s_initialized) {
        return;
    }
    obs_trace_exit(&s_buf, tid, seq, nid, ret);
}

void tracer_record_outbuf(uint64_t nid, uint64_t addr, const uint8_t *buf,
                          uint32_t len) {
    if (!s_initialized || buf == NULL || len == 0) {
        return;
    }
    obs_trace_outbuf(&s_buf, nid, addr, buf, len, OBS_TRACE_OUTBUF_INLINE);
}

void tracer_record_shader(uint64_t addr, uint32_t size, uint32_t stage,
                          const uint8_t *code) {
    if (!s_initialized || code == NULL || size == 0) {
        return;
    }
    uint64_t hash = obs_trace_fnv1a(code, size);
    obs_trace_shader_rec(&s_buf, addr, size, stage, hash);
}

void tracer_record_dcb(uint64_t addr, uint32_t dwords, uint32_t queue) {
    if (!s_initialized) {
        tracer_init(NULL, 0);
    }
    obs_trace_dcb_rec(&s_buf, addr, dwords, queue);
}

int tracer_flush_to_file(const char *path) {
    if (!s_initialized || path == NULL || s_buf.head == 0) {
        return -1;
    }

    struct obs_trace_hdr hdr;
    hdr.magic[0] = 'O';
    hdr.magic[1] = 'B';
    hdr.magic[2] = 'S';
    hdr.magic[3] = 'T';
    hdr.magic[4] = 'R';
    hdr.magic[5] = 'A';
    hdr.magic[6] = 'C';
    hdr.magic[7] = 'E';
    hdr.version = OBS_TRACE_VERSION;
    hdr.rec_size = OBS_TRACE_REC_SIZE;
    hdr.endian = OBS_TRACE_ENDIAN;
    hdr.reserved[0] = 0;
    hdr.reserved[1] = 0;
    hdr.reserved[2] = 0;

    size_t data_bytes = (size_t)s_buf.head * sizeof(struct obs_trace_rec);

#if defined(OOPS_HOST_BUILD)
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    (void)fwrite(&hdr, 1, sizeof(hdr), f);
    (void)fwrite(s_buf.recs, 1, data_bytes, f);
    fclose(f);
#else
    int fd = oops_fs_open(path, OOPS_O_WRONLY | OOPS_O_CREAT | OOPS_O_TRUNC, 0666);
    if (fd < 0) {
        return -1;
    }
    (void)oops_fs_write(fd, &hdr, sizeof(hdr));
    (void)oops_fs_write(fd, s_buf.recs, data_bytes);
    (void)oops_fs_close(fd);
#endif

    return 0;
}

/* Interceptor for sceAgcDriverSubmitDcb */
static int hook_sceAgcDriverSubmitDcb(const tracer_dcb_desc_t *desc) {
    if (desc != NULL) {
        tracer_record_dcb(desc->gpu_addr, desc->size, (uint32_t)desc->flags);
    }
    if (g_hook_agc_submit_dcb.trampoline != NULL) {
        int (*real_submit)(const tracer_dcb_desc_t *) =
            (int (*)(const tracer_dcb_desc_t *))g_hook_agc_submit_dcb.trampoline;
        return real_submit(desc);
    }
    return 0;
}

/* Interceptor for sceVideoOutSubmitFlip (per-frame telemetry & auto-drain) */
static int hook_sceVideoOutSubmitFlip(int handle, int buffer_index, int flip_mode,
                                      int64_t flip_arg) {
    s_flip_counter++;
    if ((s_flip_counter % TRACER_FLUSH_INTERVAL == 0) || (s_buf.head >= 256u)) {
        (void)tracer_flush_to_file(s_trace_path);
    }
    if (g_hook_video_out_flip.trampoline != NULL) {
        int (*real_flip)(int, int, int, int64_t) =
            (int (*)(int, int, int, int64_t))g_hook_video_out_flip.trampoline;
        return real_flip(handle, buffer_index, flip_mode, flip_arg);
    }
    return 0;
}

/* Interceptor for sceKernelAprResolveFilepathsToIdsAndFileSizes */
int hook_sceKernelAprResolveFilepathsToIdsAndFileSizes(
    const char **paths, uint32_t count, uint32_t *ids, uint64_t *sizes,
    uint32_t *statuses, void *arg5) {
    uint32_t seq = s_tracer_seq++;
    uint64_t entry_args[3] = {(uint64_t)(uintptr_t)paths, (uint64_t)count,
                              (uint64_t)(uintptr_t)ids};
    tracer_record_entry(0, seq, 0x4150525245530000ULL, entry_args, 3);

    int (*real_apr)(const char **, uint32_t, uint32_t *, uint64_t *, uint32_t *, void *) =
        (int (*)(const char **, uint32_t, uint32_t *, uint64_t *, uint32_t *, void *))
            g_hook_apr_resolve.trampoline;
    int rc = real_apr ? real_apr(paths, count, ids, sizes, statuses, arg5) : -1;

    tracer_record_exit(0, seq, 0x4150525245530000ULL, (uint64_t)(int64_t)rc);

    if (paths != NULL && ids != NULL && sizes != NULL) {
        uint32_t limit = count > 64u ? 64u : count;
        for (uint32_t i = 0; i < limit; i++) {
            const char *p = paths[i];
            uint32_t id = ids[i];
            uint64_t sz = sizes[i];
            uint32_t st = statuses ? statuses[i] : 0;
            obs_trace_apr_resolve(&s_buf, i, (uint64_t)id, sz, st, p);
        }
    }
    return rc;
}

/* Interceptor for sceKernelMapperGetParam */
int hook_sceKernelMapperGetParam(void *param_buf) {
    uint32_t seq = s_tracer_seq++;
    uint64_t entry_args[1] = {(uint64_t)(uintptr_t)param_buf};
    tracer_record_entry(0, seq, 0x4d41505045520000ULL, entry_args, 1);

    int (*real_mapper)(void *) =
        (int (*)(void *))g_hook_mapper_param.trampoline;
    int rc = real_mapper ? real_mapper(param_buf) : -1;

    tracer_record_exit(0, seq, 0x4d41505045520000ULL, (uint64_t)(int64_t)rc);

    if (rc == 0 && param_buf != NULL) {
        tracer_record_outbuf(0x4d41505045520000ULL, (uint64_t)(uintptr_t)param_buf,
                             (const uint8_t *)param_buf, 56u);
    }
    return rc;
}

/* Interceptors for file opening */
int hook_sceKernelOpen(const char *path, int flags, int mode) {
    int (*real_open)(const char *, int, int) =
        (int (*)(const char *, int, int))g_hook_sce_open.trampoline;
    int fd = real_open ? real_open(path, flags, mode) : -1;

    uint32_t seq = s_tracer_seq++;
    obs_trace_open_rec(&s_buf, seq, 0x5343454f50454e00ULL, fd, (uint64_t)flags, path);
    return fd;
}

int hook_posix_open(const char *path, int flags, int mode) {
    int (*real_open)(const char *, int, int) =
        (int (*)(const char *, int, int))g_hook_posix_open.trampoline;
    int fd = real_open ? real_open(path, flags, mode) : -1;

    uint32_t seq = s_tracer_seq++;
    obs_trace_open_rec(&s_buf, seq, 0x4f50454e00000000ULL, fd, (uint64_t)flags, path);
    return fd;
}

/* Interceptors for file closing */
int hook_sceKernelClose(int fd) {
    int (*real_close)(int) =
        (int (*)(int))g_hook_sce_close.trampoline;
    int rc = real_close ? real_close(fd) : -1;

    uint32_t seq = s_tracer_seq++;
    uint64_t args[1] = {(uint64_t)(int64_t)fd};
    tracer_record_entry(0, seq, 0x534345434c4f5300ULL, args, 1);
    tracer_record_exit(0, seq, 0x534345434c4f5300ULL, (uint64_t)(int64_t)rc);
    return rc;
}

int hook_posix_close(int fd) {
    int (*real_close)(int) =
        (int (*)(int))g_hook_posix_close.trampoline;
    int rc = real_close ? real_close(fd) : -1;

    uint32_t seq = s_tracer_seq++;
    uint64_t args[1] = {(uint64_t)(int64_t)fd};
    tracer_record_entry(0, seq, 0x434c4f5345000000ULL, args, 1);
    tracer_record_exit(0, seq, 0x434c4f5345000000ULL, (uint64_t)(int64_t)rc);
    return rc;
}

/* Interceptors for file stat */
int hook_sceKernelFstat(int fd, void *sb) {
    int (*real_fstat)(int, void *) =
        (int (*)(int, void *))g_hook_sce_fstat.trampoline;
    int rc = real_fstat ? real_fstat(fd, sb) : -1;

    if (rc == 0 && sb != NULL) {
        uint32_t seq = s_tracer_seq++;
        uint64_t ino = *(const uint64_t *)((const uint8_t *)sb + 8);
        tracer_record_outbuf(0x5343455354415400ULL, (uint64_t)(uintptr_t)sb,
                             (const uint8_t *)sb, 32u);
        obs_trace_stat_rec(&s_buf, seq, ino, 0, 0, fd);
    }
    return rc;
}

int hook_posix_fstat(int fd, void *sb) {
    int (*real_fstat)(int, void *) =
        (int (*)(int, void *))g_hook_posix_fstat.trampoline;
    int rc = real_fstat ? real_fstat(fd, sb) : -1;

    if (rc == 0 && sb != NULL) {
        uint32_t seq = s_tracer_seq++;
        uint64_t ino = *(const uint64_t *)((const uint8_t *)sb + 8);
        tracer_record_outbuf(0x5354415400000000ULL, (uint64_t)(uintptr_t)sb,
                             (const uint8_t *)sb, 32u);
        obs_trace_stat_rec(&s_buf, seq, ino, 0, 0, fd);
    }
    return rc;
}

/* Interceptor for module loading */
int hook_sceSysmoduleLoadModule(uint16_t id) {
    uint32_t seq = s_tracer_seq++;
    uint64_t args[1] = {(uint64_t)id};
    tracer_record_entry(0, seq, 0x5359534d4f440000ULL, args, 1);

    int (*real_fn)(uint16_t) =
        (int (*)(uint16_t))g_hook_sysmodule_load.trampoline;
    int rc = real_fn ? real_fn(id) : -1;

    tracer_record_exit(0, seq, 0x5359534d4f440000ULL, (uint64_t)(int64_t)rc);
    return rc;
}

/* Interceptors for direct memory */
int hook_sceKernelAllocateDirectMemory(int64_t search_low, int64_t search_high,
                                      size_t len, size_t alignment,
                                      int mem_type, int64_t *phys_out) {
    uint32_t seq = s_tracer_seq++;
    uint64_t args[3] = {(uint64_t)len, (uint64_t)alignment, (uint64_t)mem_type};
    tracer_record_entry(0, seq, 0x444952414c4c4f43ULL, args, 3);

    int (*real_fn)(int64_t, int64_t, size_t, size_t, int, int64_t *) =
        (int (*)(int64_t, int64_t, size_t, size_t, int, int64_t *))
            g_hook_alloc_direct_mem.trampoline;
    int rc = real_fn ? real_fn(search_low, search_high, len, alignment, mem_type, phys_out) : -1;

    tracer_record_exit(0, seq, 0x444952414c4c4f43ULL, (uint64_t)(int64_t)rc);
    if (rc == 0 && phys_out != NULL) {
        uint64_t out_arg[1] = {(uint64_t)*phys_out};
        tracer_record_entry(0, s_tracer_seq++, 0x4449525048595300ULL, out_arg, 1);
    }
    return rc;
}

int hook_sceKernelMapDirectMemory(void **addr_out, size_t len, int prot,
                                 int flags, int64_t direct_mem,
                                 size_t alignment) {
    uint32_t seq = s_tracer_seq++;
    uint64_t args[4] = {(uint64_t)len, (uint64_t)prot, (uint64_t)flags, (uint64_t)direct_mem};
    tracer_record_entry(0, seq, 0x4449524d41500000ULL, args, 4);

    int (*real_fn)(void **, size_t, int, int, int64_t, size_t) =
        (int (*)(void **, size_t, int, int, int64_t, size_t))
            g_hook_map_direct_mem.trampoline;
    int rc = real_fn ? real_fn(addr_out, len, prot, flags, direct_mem, alignment) : -1;

    tracer_record_exit(0, seq, 0x4449524d41500000ULL, (uint64_t)(int64_t)rc);
    if (rc == 0 && addr_out != NULL) {
        uint64_t out_arg[1] = {(uint64_t)(uintptr_t)*addr_out};
        tracer_record_entry(0, s_tracer_seq++, 0x4449525641444452ULL, out_arg, 1);
    }
    return rc;
}

int tracer_install_symbols(const tracer_symbols_t *syms) {
    if (syms == NULL) {
        return -1;
    }
    (void)tracer_hook_subsystem_init();

    if (syms->p_create_shader != NULL) {
        (void)tracer_hook_install(&g_hook_agc_create_shader, syms->p_create_shader,
                                  (void *)hook_sceAgcCreateShader, 16);
    }
    if (syms->p_submit_dcb != NULL) {
        (void)tracer_hook_install(&g_hook_agc_submit_dcb, syms->p_submit_dcb,
                                  (void *)hook_sceAgcDriverSubmitDcb, 16);
    }
    if (syms->p_submit_flip != NULL) {
        (void)tracer_hook_install(&g_hook_video_out_flip, syms->p_submit_flip,
                                  (void *)hook_sceVideoOutSubmitFlip, 16);
    }
    if (syms->p_apr_resolve != NULL) {
        (void)tracer_hook_install(&g_hook_apr_resolve, syms->p_apr_resolve,
                                  (void *)hook_sceKernelAprResolveFilepathsToIdsAndFileSizes, 16);
    }
    if (syms->p_mapper_param != NULL) {
        (void)tracer_hook_install(&g_hook_mapper_param, syms->p_mapper_param,
                                  (void *)hook_sceKernelMapperGetParam, 16);
    }
    if (syms->p_sce_open != NULL) {
        (void)tracer_hook_install(&g_hook_sce_open, syms->p_sce_open,
                                  (void *)hook_sceKernelOpen, 16);
    }
    if (syms->p_posix_open != NULL) {
        (void)tracer_hook_install(&g_hook_posix_open, syms->p_posix_open,
                                  (void *)hook_posix_open, 16);
    }
    if (syms->p_sce_close != NULL) {
        (void)tracer_hook_install(&g_hook_sce_close, syms->p_sce_close,
                                  (void *)hook_sceKernelClose, 16);
    }
    if (syms->p_posix_close != NULL) {
        (void)tracer_hook_install(&g_hook_posix_close, syms->p_posix_close,
                                  (void *)hook_posix_close, 16);
    }
    if (syms->p_sce_fstat != NULL) {
        (void)tracer_hook_install(&g_hook_sce_fstat, syms->p_sce_fstat,
                                  (void *)hook_sceKernelFstat, 16);
    }
    if (syms->p_posix_fstat != NULL) {
        (void)tracer_hook_install(&g_hook_posix_fstat, syms->p_posix_fstat,
                                  (void *)hook_posix_fstat, 16);
    }
    if (syms->p_sysmodule_load != NULL) {
        (void)tracer_hook_install(&g_hook_sysmodule_load, syms->p_sysmodule_load,
                                  (void *)hook_sceSysmoduleLoadModule, 16);
    }
    if (syms->p_alloc_direct_mem != NULL) {
        (void)tracer_hook_install(&g_hook_alloc_direct_mem, syms->p_alloc_direct_mem,
                                  (void *)hook_sceKernelAllocateDirectMemory, 16);
    }
    if (syms->p_map_direct_mem != NULL) {
        (void)tracer_hook_install(&g_hook_map_direct_mem, syms->p_map_direct_mem,
                                  (void *)hook_sceKernelMapDirectMemory, 16);
    }
    return 0;
}

int tracer_install_hooks(void *p_create_shader, void *p_submit_dcb,
                         void *p_submit_flip) {
    tracer_symbols_t syms;
    memset(&syms, 0, sizeof(syms));
    syms.p_create_shader = p_create_shader;
    syms.p_submit_dcb = p_submit_dcb;
    syms.p_submit_flip = p_submit_flip;
    return tracer_install_symbols(&syms);
}

void tracer_uninstall_hooks(void) {
    tracer_hook_t *all_hooks[] = {
        &g_hook_agc_create_shader,
        &g_hook_agc_submit_dcb,
        &g_hook_video_out_flip,
        &g_hook_apr_resolve,
        &g_hook_mapper_param,
        &g_hook_sce_open,
        &g_hook_posix_open,
        &g_hook_sce_close,
        &g_hook_posix_close,
        &g_hook_sce_fstat,
        &g_hook_posix_fstat,
        &g_hook_sysmodule_load,
        &g_hook_alloc_direct_mem,
        &g_hook_map_direct_mem,
    };
    for (size_t i = 0; i < sizeof(all_hooks) / sizeof(all_hooks[0]); i++) {
        if (all_hooks[i]->installed) {
            (void)tracer_hook_uninstall(all_hooks[i]);
        }
    }
}

#if !defined(OOPS_HOST_BUILD)
static void *resolve_symbol(const char *nid_str, const char *name_str) {
    void *addr = NULL;
    char computed_nid[16];
    computed_nid[0] = '$';
    computed_nid[1] = '\0';
    if (name_str != NULL) {
        obs_compute_nid(name_str, computed_nid + 1);
    }

    if (&sceKernelDlsym != NULL) {
        /* 1. Try default handle 0x2001 (all loaded objects) */
        if (nid_str != NULL && sceKernelDlsym(0x2001, nid_str, &addr) == 0 &&
            addr != NULL) {
            return addr;
        }
        if (name_str != NULL && sceKernelDlsym(0x2001, name_str, &addr) == 0 &&
            addr != NULL) {
            return addr;
        }
        if (computed_nid[1] != '\0' &&
            sceKernelDlsym(0x2001, computed_nid, &addr) == 0 && addr != NULL) {
            return addr;
        }
        /* 2. Try handle 1 (libkernel) */
        if (nid_str != NULL && sceKernelDlsym(1, nid_str, &addr) == 0 && addr != NULL) {
            return addr;
        }
        if (name_str != NULL && sceKernelDlsym(1, name_str, &addr) == 0 &&
            addr != NULL) {
            return addr;
        }
        if (computed_nid[1] != '\0' &&
            sceKernelDlsym(1, computed_nid, &addr) == 0 && addr != NULL) {
            return addr;
        }
        /* 3. Try handle 2 (main executable) */
        if (nid_str != NULL && sceKernelDlsym(2, nid_str, &addr) == 0 && addr != NULL) {
            return addr;
        }
        if (name_str != NULL && sceKernelDlsym(2, name_str, &addr) == 0 &&
            addr != NULL) {
            return addr;
        }
    }

    /* 4. Fallback: check kernel export table if available */
    if (s_kexport_table != NULL) {
        if (nid_str != NULL) {
            const char *raw_nid = nid_str[0] == '$' ? nid_str + 1 : nid_str;
            const void *ptr = obs_kexport_lookup(s_kexport_table, raw_nid);
            if (ptr != NULL && (uintptr_t)ptr >= 0x10000UL) {
                return (void *)(uintptr_t)ptr;
            }
        }
        if (computed_nid[1] != '\0') {
            const void *ptr = obs_kexport_lookup(s_kexport_table, computed_nid + 1);
            if (ptr != NULL && (uintptr_t)ptr >= 0x10000UL) {
                return (void *)(uintptr_t)ptr;
            }
        }
    }

    return NULL;
}
#endif

/* Standalone / injected entry point */
int tracer_start(payload_args_t *args);

int tracer_start(payload_args_t *args) {
#if !defined(OOPS_HOST_BUILD)
    if (args != NULL) {
        sys_call_init(args);
        (void)krw_init(args);
        if (args->kexport_table != NULL) {
            s_kexport_table = (const obs_kexport_table_t *)args->kexport_table;
        }
    }
    pid_t my_pid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
    char title_id[32] = {0};
    if (target_get_title_id(my_pid, title_id, sizeof(title_id)) > 0 && title_id[0] != '\0') {
        size_t tlen = 0;
        while (title_id[tlen] != '\0' && tlen < 20) {
            tlen++;
        }
        if (tlen > 0) {
            char path[64] = "/data/trace-";
            memcpy(path + 12, title_id, tlen);
            memcpy(path + 12 + tlen, ".bin", 5);
            memcpy(s_trace_path, path, sizeof(s_trace_path));
        }
    }
#else
    (void)args;
#endif

#ifndef OOPS_APP_VERSION
#define OOPS_APP_VERSION "dev"
#endif

    TRACER_LOG(
        "tracer: starting telemetry & shader capture session (v " OOPS_APP_VERSION ")");
    tracer_init(NULL, 0);

    /* Record startup marker */
    uint64_t startup_args[1] = {OBS_TRACE_VERSION};
    tracer_record_entry(0, 0, 0x5452414345520000ULL /* "TRACER\0\0" */, startup_args,
                        1);

#if !defined(OOPS_HOST_BUILD)
    tracer_symbols_t syms;
    memset(&syms, 0, sizeof(syms));

    syms.p_create_shader = resolve_symbol("$f3dg2CSgRKY", "sceAgcCreateShader");
    syms.p_submit_dcb = resolve_symbol("$UglJIZjGssM", "sceAgcDriverSubmitDcb");
    syms.p_submit_flip = resolve_symbol("$CdWp0oHWGr0", "sceVideoOutSubmitFlip");
    syms.p_apr_resolve = resolve_symbol(NULL, "sceKernelAprResolveFilepathsToIdsAndFileSizes");
    if (syms.p_apr_resolve == NULL) {
        syms.p_apr_resolve = resolve_symbol(NULL, "sceKernelAprResolveFilepathsToIds");
    }
    syms.p_mapper_param = resolve_symbol("$1yXS+iqB3wQ", "sceKernelMapperGetParam");
    syms.p_sce_open = resolve_symbol("$D1s+bHTr3gI", "sceKernelOpen");
    syms.p_posix_open = resolve_symbol(NULL, "open");
    syms.p_sce_close = resolve_symbol("$b7LcLbE7U8g", "sceKernelClose");
    syms.p_posix_close = resolve_symbol(NULL, "close");
    syms.p_sce_fstat = resolve_symbol(NULL, "sceKernelFstat");
    syms.p_posix_fstat = resolve_symbol(NULL, "fstat");
    syms.p_sysmodule_load = resolve_symbol(NULL, "sceSysmoduleLoadModule");
    if (syms.p_sysmodule_load == NULL) {
        syms.p_sysmodule_load = resolve_symbol(NULL, "sceSysmoduleLoadModuleInternal");
    }
    syms.p_alloc_direct_mem = resolve_symbol(NULL, "sceKernelAllocateDirectMemory");
    syms.p_map_direct_mem = resolve_symbol(NULL, "sceKernelMapDirectMemory");

    tracer_install_symbols(&syms);
    TRACER_LOG("tracer: installed available telemetry and I/O hooks");
#endif

    TRACER_LOG("tracer: initialized successfully");
    return 0;
}
