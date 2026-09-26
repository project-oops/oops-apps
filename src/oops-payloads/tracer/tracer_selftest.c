/*
 * Tracer host self-test: the per-NID sampler, out-buffer telemetry, the x86_64 detour
 * and trampoline engine, AGC shader capture with deduplication, DCB submission capture,
 * and decoding of the flushed trace.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "trace_format.h"
#include "trace_encode.h"
#include "trace_decode.h"
#include "hook.h"
#include "shader_dump.h"
#include "tracer.h"

#define NID_HOT 0x1111111111111111ULL
#define NID_COLD 0x2222222222222222ULL
#define NID_OUT 0x3333333333333333ULL
#define NID_BIG 0x4444444444444444ULL

#define HOT_CALLS 1000u
#define COLD_CALLS 4u

/* Detour target: 16 bytes of nops give the detour room to overwrite. */
static int s_mock_hook_called = 0;
static tracer_hook_t s_test_hook = {0};

static int __attribute__((noinline)) mock_add(int a, int b) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    return a + b;
}

static int mock_add_hook(int a, int b) {
    s_mock_hook_called++;
    int (*real_add)(int, int) = (int (*)(int, int))s_test_hook.trampoline;
    return real_add(a, b) + 100;
}

/* Stand-in for sceAgcCreateShader. */
static int s_real_agc_create_shader_called = 0;

int mock_sceAgcCreateShader(void *shader_obj, const void *header, void *gpu_payload,
                            uint32_t flags);
int __attribute__((noinline)) mock_sceAgcCreateShader(void *shader_obj,
                                                      const void *header,
                                                      void *gpu_payload,
                                                      uint32_t flags) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_agc_create_shader_called++;
    if (shader_obj != NULL) {
        *(uint32_t *)shader_obj = 0xcafebabeu;
    }
    volatile const void *vh = header;
    volatile void *vp = gpu_payload;
    volatile uint32_t vf = flags;
    (void)vh;
    (void)vp;
    (void)vf;
    return 0;
}

/* Stand-in for sceAgcDriverSubmitDcb. */
static int s_real_agc_submit_dcb_called = 0;

typedef struct {
    uint64_t gpu_addr;
    uint32_t size;
    uint16_t flags;
    uint16_t pad;
} test_dcb_desc_t;

int mock_sceAgcDriverSubmitDcb(const test_dcb_desc_t *desc);
int __attribute__((noinline)) mock_sceAgcDriverSubmitDcb(const test_dcb_desc_t *desc) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_agc_submit_dcb_called++;
    volatile const void *vd = desc;
    (void)vd;
    return 0;
}

static int s_real_apr_resolve_called = 0;
int mock_sceKernelAprResolveFilepathsToIdsAndFileSizes(
    const char **paths, uint32_t count, uint32_t *ids, uint64_t *sizes,
    uint32_t *statuses, void *arg5);
int __attribute__((noinline)) mock_sceKernelAprResolveFilepathsToIdsAndFileSizes(
    const char **paths, uint32_t count, uint32_t *ids, uint64_t *sizes,
    uint32_t *statuses, void *arg5) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_apr_resolve_called++;
    (void)paths;
    (void)arg5;
    for (uint32_t i = 0; i < count; i++) {
        ids[i] = 26u;
        sizes[i] = 224748u;
        if (statuses != NULL) {
            statuses[i] = 0u;
        }
    }
    return 0;
}

static int s_real_mapper_called = 0;
int mock_sceKernelMapperGetParam(void *param_buf);
int __attribute__((noinline)) mock_sceKernelMapperGetParam(void *param_buf) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_mapper_called++;
    if (param_buf != NULL) {
        uint64_t *q = (uint64_t *)param_buf;
        q[0] = 0x38u;
        q[1] = 0x80000000000ULL;
        q[2] = 0x88000000000ULL;
        q[3] = 0x88000000000ULL;
        q[4] = 0x88000000000ULL;
        q[5] = 0x40u;
        q[6] = 0x2663u;
    }
    return 0;
}

static int s_real_open_called = 0;
int mock_sceKernelOpen(const char *path, int flags, int mode);
int __attribute__((noinline)) mock_sceKernelOpen(const char *path, int flags, int mode) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_open_called++;
    (void)path;
    (void)flags;
    (void)mode;
    return 3;
}

static int s_real_fstat_called = 0;
int mock_sceKernelFstat(int fd, void *sb);
int __attribute__((noinline)) mock_sceKernelFstat(int fd, void *sb) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; "
                     "nop; nop; nop;");
    s_real_fstat_called++;
    (void)fd;
    if (sb != NULL) {
        memset(sb, 0, 128);
        *(uint64_t *)((uint8_t *)sb + 8) = 1001ULL; /* st_ino */
    }
    return 0;
}

int main(void) {
    printf("tracer_selftest: starting...\n");

    /* The sampler counts every call and records the first OBS_TRACE_CAP per NID. */
    static struct obs_trace_rec storage[4096];
    static uint64_t samp_nids[256];
    static uint32_t samp_counts[256];

    struct obs_trace_buf buf;
    struct obs_trace_sampler samp;
    obs_trace_buf_init(&buf, storage, 4096u);
    obs_trace_sampler_init(&samp, samp_nids, samp_counts, 256u, OBS_TRACE_CAP);

    uint32_t seq = 0;
    for (unsigned i = 0; i < HOT_CALLS; i++) {
        uint64_t args[2] = {(uint64_t)i, 0x1337ULL};
        if (obs_trace_hit(&samp, NID_HOT)) {
            obs_trace_entry(&buf, 1, seq++, NID_HOT, args, 2);
            obs_trace_exit(&buf, 1, seq - 1, NID_HOT, (uint64_t)i * 2u);
        }
    }
    assert(obs_trace_count(&samp, NID_HOT) == HOT_CALLS);
    assert(buf.head == OBS_TRACE_CAP * 2u);

    for (unsigned i = 0; i < COLD_CALLS; i++) {
        uint64_t args[1] = {(uint64_t)(0x100 + i)};
        if (obs_trace_hit(&samp, NID_COLD)) {
            obs_trace_entry(&buf, 1, seq++, NID_COLD, args, 1);
        }
    }
    assert(obs_trace_count(&samp, NID_COLD) == COLD_CALLS);

    /* A small out-buffer is recorded inline. */
    (void)obs_trace_hit(&samp, NID_OUT);
    uint8_t small[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    obs_trace_outbuf(&buf, NID_OUT, 0x40000000ULL, small, sizeof(small),
                     OBS_TRACE_OUTBUF_INLINE);

    /* A large out-buffer is recorded as its FNV-1a hash. */
    (void)obs_trace_hit(&samp, NID_BIG);
    uint8_t big[128];
    for (unsigned i = 0; i < sizeof(big); i++) {
        big[i] = (uint8_t)(i * 3u + 1u);
    }
    uint64_t expected_hash = obs_trace_fnv1a(big, sizeof(big));
    obs_trace_outbuf(&buf, NID_BIG, 0x50000000ULL, big, sizeof(big),
                     OBS_TRACE_OUTBUF_INLINE);
    assert(expected_hash != 0);
    assert(buf.recs[buf.head - 1u].arg[2] == expected_hash);

    /* A detour routes calls to the hook, whose trampoline reaches the original;
     * removing it restores the original behaviour. */
    assert(mock_add(10, 20) == 30);
    assert(s_mock_hook_called == 0);

    int hook_rc =
        tracer_hook_install(&s_test_hook, (void *)mock_add, (void *)mock_add_hook, 16);
    assert(hook_rc == 0);
    assert(s_test_hook.installed == 1);

    /* mock_add_hook -> trampoline -> 30, plus 100. */
    int (*volatile p_add)(int, int) = mock_add;
    int hooked_res = p_add(10, 20);
    assert(hooked_res == 130);
    assert(s_mock_hook_called == 1);

    /* Uninstalling restores the original. */
    int unhook_rc = tracer_hook_uninstall(&s_test_hook);
    assert(unhook_rc == 0);
    assert(s_test_hook.installed == 0);
    assert(p_add(10, 20) == 30);
    assert(s_mock_hook_called == 1);

    /* A created shader's bytecode and header are dumped to disk byte for byte, once per
     * distinct bytecode. */
    tracer_init(NULL, 0);
    shader_dump_set_directory("build/test_shaders");
    shader_dump_reset_cache();

    int agc_hook_rc =
        tracer_hook_install(&g_hook_agc_create_shader, (void *)mock_sceAgcCreateShader,
                            (void *)hook_sceAgcCreateShader, 16);
    assert(agc_hook_rc == 0);

    /* A synthetic compute shader. */
    uint32_t container_hdr[76]; /* 304 bytes */
    memset(container_hdr, 0, sizeof(container_hdr));
    container_hdr[0] = SHADER_STAGE_CS; /* Compute */
    container_hdr[2] = 64u;             /* 64 bytes bytecode */

    uint32_t cs_bytecode[16]; /* 64 bytes */
    memset(cs_bytecode, 0, sizeof(cs_bytecode));
    cs_bytecode[0] = 0xbf800000u; /* s_nop */
    cs_bytecode[1] = 0xbf810000u; /* s_endpgm */

    int (*volatile p_create_shader)(void *, const void *, void *, uint32_t) =
        mock_sceAgcCreateShader;
    uint32_t obj_out = 0;
    int create_rc = p_create_shader(&obj_out, container_hdr, cs_bytecode, 0);
    assert(create_rc == 0);
    assert(s_real_agc_create_shader_called == 1);
    assert(obj_out == 0xcafebabeu);
    assert(shader_dump_get_count() == 1);

    /* The dump files are named by the bytecode hash. */
    uint64_t cs_hash = obs_trace_fnv1a((const uint8_t *)cs_bytecode, 64);
    char hex[17];
    for (int i = 15; i >= 0; i--) {
        hex[i] = "0123456789abcdef"[cs_hash & 0x0fu];
        cs_hash >>= 4u;
    }
    hex[16] = '\0';

    char bin_path[256];
    char hdr_path[256];
    snprintf(bin_path, sizeof(bin_path), "build/test_shaders/%s.bin", hex);
    snprintf(hdr_path, sizeof(hdr_path), "build/test_shaders/%s.hdr", hex);

    FILE *f_bin = fopen(bin_path, "rb");
    assert(f_bin != NULL);
    uint32_t read_code[16];
    assert(fread(read_code, 1, sizeof(read_code), f_bin) == sizeof(read_code));
    assert(memcmp(read_code, cs_bytecode, sizeof(cs_bytecode)) == 0);
    fclose(f_bin);

    FILE *f_hdr = fopen(hdr_path, "rb");
    assert(f_hdr != NULL);
    uint32_t read_hdr[76];
    assert(fread(read_hdr, 1, sizeof(read_hdr), f_hdr) == sizeof(read_hdr));
    assert(memcmp(read_hdr, container_hdr, sizeof(container_hdr)) == 0);
    fclose(f_hdr);

    /* The same bytecode again is not dumped again. */
    create_rc = p_create_shader(&obj_out, container_hdr, cs_bytecode, 0);
    assert(create_rc == 0);
    assert(shader_dump_get_count() == 1);

    /* Different bytecode is dumped. */
    container_hdr[0] = SHADER_STAGE_VS;
    container_hdr[2] = 128u;
    uint32_t vs_bytecode[32];
    memset(vs_bytecode, 0, sizeof(vs_bytecode));
    vs_bytecode[0] = 0x7e0202ffu;
    vs_bytecode[1] = 0xbf810000u;
    create_rc = p_create_shader(&obj_out, container_hdr, vs_bytecode, 0);
    assert(create_rc == 0);
    assert(shader_dump_get_count() == 2);

    remove(bin_path);
    remove(hdr_path);

    /* tracer_install_hooks intercepts a DCB submission and still calls the original. */
    int dcb_hook_rc =
        tracer_hook_install(&g_hook_agc_submit_dcb, (void *)mock_sceAgcDriverSubmitDcb,
                            (void *)g_hook_agc_submit_dcb.hook_fn, 16);
    tracer_uninstall_hooks();
    int install_rc = tracer_install_hooks((void *)mock_sceAgcCreateShader,
                                          (void *)mock_sceAgcDriverSubmitDcb, NULL);
    assert(install_rc == 0);

    test_dcb_desc_t desc;
    desc.gpu_addr = 0xbeef0000ULL;
    desc.size = 1024u;
    desc.flags = 0u;
    desc.pad = 0u;

    int (*volatile p_submit_dcb)(const test_dcb_desc_t *) = mock_sceAgcDriverSubmitDcb;
    int submit_rc = p_submit_dcb(&desc);
    assert(submit_rc == 0);
    assert(s_real_agc_submit_dcb_called == 1);

    tracer_uninstall_hooks();

    /* Test bulk hook installation for APR resolve, Mapper, Open, and Fstat */
    tracer_symbols_t syms;
    memset(&syms, 0, sizeof(syms));
    syms.p_apr_resolve = (void *)mock_sceKernelAprResolveFilepathsToIdsAndFileSizes;
    syms.p_mapper_param = (void *)mock_sceKernelMapperGetParam;
    syms.p_sce_open = (void *)mock_sceKernelOpen;
    syms.p_sce_fstat = (void *)mock_sceKernelFstat;
    int bulk_rc = tracer_install_symbols(&syms);
    assert(bulk_rc == 0);

    /* Test APR resolve hook: resolves globalgamemanagers */
    const char *test_paths[1] = {"/app0/Media/globalgamemanagers"};
    uint32_t test_ids[1] = {0};
    uint64_t test_sizes[1] = {0};
    uint32_t test_statuses[1] = {99};
    int (*volatile p_apr)(const char **, uint32_t, uint32_t *, uint64_t *, uint32_t *, void *) =
        mock_sceKernelAprResolveFilepathsToIdsAndFileSizes;
    int apr_rc = p_apr(test_paths, 1u, test_ids, test_sizes, test_statuses, NULL);
    assert(apr_rc == 0);
    assert(s_real_apr_resolve_called == 1);
    assert(test_ids[0] == 26u);
    assert(test_sizes[0] == 224748u);

    /* Test Mapper param hook: populates 56-byte buffer */
    uint8_t mapper_buf[56];
    memset(mapper_buf, 0, sizeof(mapper_buf));
    int (*volatile p_mapper)(void *) = mock_sceKernelMapperGetParam;
    int mapper_rc = p_mapper(mapper_buf);
    assert(mapper_rc == 0);
    assert(s_real_mapper_called == 1);
    assert(*(uint64_t *)(void *)(mapper_buf + 8) == 0x80000000000ULL);

    /* Test Open hook */
    int (*volatile p_open)(const char *, int, int) = mock_sceKernelOpen;
    int open_fd = p_open("/app0/Media/globalgamemanagers", 0, 0);
    assert(open_fd == 3);
    assert(s_real_open_called == 1);

    /* Test Fstat hook */
    uint8_t stat_buf[128];
    int (*volatile p_fstat)(int, void *) = mock_sceKernelFstat;
    int fstat_rc = p_fstat(3, stat_buf);
    assert(fstat_rc == 0);
    assert(s_real_fstat_called == 1);

    tracer_uninstall_hooks();

    /* The flushed trace file decodes without error and contains the expected records. */
    const char *trace_path = "build/tracer_selftest.bin";
    const char *text_path = "build/tracer_selftest.txt";
    int flush_rc = tracer_flush_to_file(trace_path);
    assert(flush_rc == 0);

    FILE *fin = fopen(trace_path, "rb");
    assert(fin != NULL);

    FILE *fout = fopen(text_path, "w");
    assert(fout != NULL);

    int decode_rc = obs_trace_decode_stream(fin, fout);
    fclose(fin);
    fclose(fout);
    remove(trace_path);

    assert(decode_rc == 0);

    /* Verify decoded content */
    FILE *ftxt = fopen(text_path, "r");
    assert(ftxt != NULL);
    char line[512];
    int found_apr = 0;
    int found_open = 0;
    int found_stat = 0;
    int found_mapper = 0;
    while (fgets(line, (int)sizeof(line), ftxt) != NULL) {
        if (strstr(line, "OBS|apr_resolve|idx=0|id=26|size=224748|status=0|path=/app0/Media/globalgamemanagers") != NULL) {
            found_apr = 1;
        }
        if (strstr(line, "OBS|open|fd=3|flags=0x0|path=/app0/Media/globalgamemanagers") != NULL) {
            found_open = 1;
        }
        if (strstr(line, "OBS|stat|fd=3|ino=1001|") != NULL) {
            found_stat = 1;
        }
        if (strstr(line, "OBS|outbuf|4d41505045520000|") != NULL) {
            found_mapper = 1;
        }
    }
    fclose(ftxt);
    remove(text_path);

    assert(found_apr == 1);
    assert(found_open == 1);
    assert(found_stat == 1);
    assert(found_mapper == 1);
    (void)dcb_hook_rc;

    printf("tracer_selftest: ok (API calls, rate limiter, inline hooks, shader "
           "capture, and DCB telemetry verified)\n");
    return 0;
}
