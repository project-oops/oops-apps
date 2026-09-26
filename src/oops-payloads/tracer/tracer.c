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
#define TRACER_LOG(msg) oops_klog("TRACER", msg)
#endif

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

tracer_hook_t g_hook_agc_submit_dcb = {0};
tracer_hook_t g_hook_video_out_flip = {0};

/* Weak reference to runtime dynamic linker on Prospero */
#if !defined(OOPS_HOST_BUILD)
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
        (void)tracer_flush_to_file("/data/trace.bin");
    }
    if (g_hook_video_out_flip.trampoline != NULL) {
        int (*real_flip)(int, int, int, int64_t) =
            (int (*)(int, int, int, int64_t))g_hook_video_out_flip.trampoline;
        return real_flip(handle, buffer_index, flip_mode, flip_arg);
    }
    return 0;
}

int tracer_install_hooks(void *p_create_shader, void *p_submit_dcb,
                         void *p_submit_flip) {
    (void)tracer_hook_subsystem_init();

    if (p_create_shader != NULL) {
        (void)tracer_hook_install(&g_hook_agc_create_shader, p_create_shader,
                                  (void *)hook_sceAgcCreateShader, 16);
    }
    if (p_submit_dcb != NULL) {
        (void)tracer_hook_install(&g_hook_agc_submit_dcb, p_submit_dcb,
                                  (void *)hook_sceAgcDriverSubmitDcb, 16);
    }
    if (p_submit_flip != NULL) {
        (void)tracer_hook_install(&g_hook_video_out_flip, p_submit_flip,
                                  (void *)hook_sceVideoOutSubmitFlip, 16);
    }
    return 0;
}

void tracer_uninstall_hooks(void) {
    if (g_hook_agc_create_shader.installed) {
        (void)tracer_hook_uninstall(&g_hook_agc_create_shader);
    }
    if (g_hook_agc_submit_dcb.installed) {
        (void)tracer_hook_uninstall(&g_hook_agc_submit_dcb);
    }
    if (g_hook_video_out_flip.installed) {
        (void)tracer_hook_uninstall(&g_hook_video_out_flip);
    }
}

#if !defined(OOPS_HOST_BUILD)
static void *resolve_symbol(const char *nid_str, const char *name_str) {
    if (&sceKernelDlsym == NULL) {
        return NULL;
    }
    void *addr = NULL;
    /* Try default handle 0x2001 (all loaded objects) */
    if (nid_str != NULL && sceKernelDlsym(0x2001, nid_str, &addr) == 0 &&
        addr != NULL) {
        return addr;
    }
    if (name_str != NULL && sceKernelDlsym(0x2001, name_str, &addr) == 0 &&
        addr != NULL) {
        return addr;
    }
    /* Fallback: handle 2 */
    if (nid_str != NULL && sceKernelDlsym(2, nid_str, &addr) == 0 && addr != NULL) {
        return addr;
    }
    if (name_str != NULL && sceKernelDlsym(2, name_str, &addr) == 0 && addr != NULL) {
        return addr;
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
    /* Auto-resolve and hook AGC and presentation entry points */
    void *p_create_shader = resolve_symbol("$f3dg2CSgRKY", "sceAgcCreateShader");
    void *p_submit_dcb = resolve_symbol("$UglJIZjGssM", "sceAgcDriverSubmitDcb");
    void *p_submit_flip = resolve_symbol("$CdWp0oHWGr0", "sceVideoOutSubmitFlip");

    if (p_create_shader != NULL || p_submit_dcb != NULL || p_submit_flip != NULL) {
        tracer_install_hooks(p_create_shader, p_submit_dcb, p_submit_flip);
        TRACER_LOG("tracer: installed runtime hooks on AGC and VideoOut");
    } else {
        TRACER_LOG("tracer: running in passive mode (no AGC symbols resolved)");
    }
#endif

    TRACER_LOG("tracer: initialized successfully");
    return 0;
}
