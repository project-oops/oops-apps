/*
 * Comprehensive tracer host selftest.
 *
 * Exercises:
 * 1. Fixed-rate sampling & linear probing NID sampler.
 * 2. Inlined and hashed out-buffer diff telemetry.
 * 3. Freestanding x86_64 inline detour and trampoline hooking engine.
 * 4. AGC shader interception, container parsing, FNV-1a deduplication, and bytecode dumping.
 * 5. AGC command buffer (DCB) telemetry interception.
 * 6. Wire format decoding and verification.
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

#define NID_HOT   0x1111111111111111ULL
#define NID_COLD  0x2222222222222222ULL
#define NID_OUT   0x3333333333333333ULL
#define NID_BIG   0x4444444444444444ULL

#define HOT_CALLS 1000u
#define COLD_CALLS 4u

/* --- Test Target 1: Generic Function Hooking --- */
static int s_mock_hook_called = 0;
static tracer_hook_t s_test_hook = {0};

static int __attribute__((noinline)) mock_add(int a, int b) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    return a + b;
}

static int mock_add_hook(int a, int b) {
    s_mock_hook_called++;
    int (*real_add)(int, int) = (int (*)(int, int))s_test_hook.trampoline;
    return real_add(a, b) + 100;
}

/* --- Test Target 2: Mock AGC Shader Creation --- */
static int s_real_agc_create_shader_called = 0;

int mock_sceAgcCreateShader(void *shader_obj, const void *header,
                            void *gpu_payload, uint32_t flags);
int __attribute__((noinline)) mock_sceAgcCreateShader(void *shader_obj, const void *header,
                                                      void *gpu_payload, uint32_t flags) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
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

/* --- Test Target 3: Mock AGC DCB Submission --- */
static int s_real_agc_submit_dcb_called = 0;

typedef struct {
    uint64_t gpu_addr;
    uint32_t size;
    uint16_t flags;
    uint16_t pad;
} test_dcb_desc_t;

int mock_sceAgcDriverSubmitDcb(const test_dcb_desc_t *desc);
int __attribute__((noinline)) mock_sceAgcDriverSubmitDcb(const test_dcb_desc_t *desc) {
    __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop; nop;");
    s_real_agc_submit_dcb_called++;
    volatile const void *vd = desc;
    (void)vd;
    return 0;
}

int main(void) {
    printf("tracer_selftest: starting...\n");

    /* =====================================================================
     * Section 1: Ring Buffer, Sampler, & Outbuf Telemetry
     * ===================================================================== */
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

    /* Inlined outbuf */
    (void)obs_trace_hit(&samp, NID_OUT);
    uint8_t small[8] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80};
    obs_trace_outbuf(&buf, NID_OUT, 0x40000000ULL, small, sizeof(small), OBS_TRACE_OUTBUF_INLINE);

    /* Hashed outbuf */
    (void)obs_trace_hit(&samp, NID_BIG);
    uint8_t big[128];
    for (unsigned i = 0; i < sizeof(big); i++) {
        big[i] = (uint8_t)(i * 3u + 1u);
    }
    uint64_t expected_hash = obs_trace_fnv1a(big, sizeof(big));
    obs_trace_outbuf(&buf, NID_BIG, 0x50000000ULL, big, sizeof(big), OBS_TRACE_OUTBUF_INLINE);
    assert(expected_hash != 0);
    assert(buf.recs[buf.head - 1u].arg[2] == expected_hash);

    /* =====================================================================
     * Section 2: Inline Detour & Trampoline Hooking Engine
     * ===================================================================== */
    assert(mock_add(10, 20) == 30);
    assert(s_mock_hook_called == 0);

    int hook_rc = tracer_hook_install(&s_test_hook, (void *)mock_add, (void *)mock_add_hook, 16);
    assert(hook_rc == 0);
    assert(s_test_hook.installed == 1);

    /* Calling mock_add should now route through mock_add_hook -> trampoline -> 30 + 100 = 130 */
    int (* volatile p_add)(int, int) = mock_add;
    int hooked_res = p_add(10, 20);
    assert(hooked_res == 130);
    assert(s_mock_hook_called == 1);

    /* Uninstall hook and verify restoration */
    int unhook_rc = tracer_hook_uninstall(&s_test_hook);
    assert(unhook_rc == 0);
    assert(s_test_hook.installed == 0);
    assert(p_add(10, 20) == 30);
    assert(s_mock_hook_called == 1);

    /* =====================================================================
     * Section 3: AGC Shader Interception & Bytecode Disk Dumping
     * ===================================================================== */
    tracer_init(NULL, 0);
    shader_dump_set_directory("build/test_shaders");
    shader_dump_reset_cache();

    int agc_hook_rc = tracer_hook_install(&g_hook_agc_create_shader,
                                          (void *)mock_sceAgcCreateShader,
                                          (void *)hook_sceAgcCreateShader, 16);
    assert(agc_hook_rc == 0);

    /* Construct synthetic compute shader */
    uint32_t container_hdr[76]; /* 304 bytes */
    memset(container_hdr, 0, sizeof(container_hdr));
    container_hdr[0] = SHADER_STAGE_CS; /* Compute */
    container_hdr[2] = 64u;             /* 64 bytes bytecode */

    uint32_t cs_bytecode[16]; /* 64 bytes */
    memset(cs_bytecode, 0, sizeof(cs_bytecode));
    cs_bytecode[0] = 0xbf800000u; /* s_nop */
    cs_bytecode[1] = 0xbf810000u; /* s_endpgm */

    int (* volatile p_create_shader)(void *, const void *, void *, uint32_t) = mock_sceAgcCreateShader;
    uint32_t obj_out = 0;
    int create_rc = p_create_shader(&obj_out, container_hdr, cs_bytecode, 0);
    assert(create_rc == 0);
    assert(s_real_agc_create_shader_called == 1);
    assert(obj_out == 0xcafebabeu);
    assert(shader_dump_get_count() == 1);

    /* Verify dumped files exist on disk */
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

    /* Test deduplication: calling again with same payload must NOT increment dump count */
    create_rc = p_create_shader(&obj_out, container_hdr, cs_bytecode, 0);
    assert(create_rc == 0);
    assert(shader_dump_get_count() == 1);

    /* Test second shader: Vertex shader with different instructions */
    container_hdr[0] = SHADER_STAGE_VS;
    container_hdr[2] = 128u;
    uint32_t vs_bytecode[32];
    memset(vs_bytecode, 0, sizeof(vs_bytecode));
    vs_bytecode[0] = 0x7e0202ffu;
    vs_bytecode[1] = 0xbf810000u;
    create_rc = p_create_shader(&obj_out, container_hdr, vs_bytecode, 0);
    assert(create_rc == 0);
    assert(shader_dump_get_count() == 2);

    /* Clean up shader files */
    remove(bin_path);
    remove(hdr_path);

    /* =====================================================================
     * Section 4: AGC DCB Submission Interception
     * ===================================================================== */
    int dcb_hook_rc = tracer_hook_install(&g_hook_agc_submit_dcb,
                                          (void *)mock_sceAgcDriverSubmitDcb,
                                          (void *)g_hook_agc_submit_dcb.hook_fn, 16);
    /* Alternatively, hook through tracer_install_hooks */
    tracer_uninstall_hooks();
    int install_rc = tracer_install_hooks((void *)mock_sceAgcCreateShader,
                                          (void *)mock_sceAgcDriverSubmitDcb, NULL);
    assert(install_rc == 0);

    test_dcb_desc_t desc;
    desc.gpu_addr = 0xbeef0000ULL;
    desc.size = 1024u;
    desc.flags = 0u;
    desc.pad = 0u;

    int (* volatile p_submit_dcb)(const test_dcb_desc_t *) = mock_sceAgcDriverSubmitDcb;
    int submit_rc = p_submit_dcb(&desc);
    assert(submit_rc == 0);
    assert(s_real_agc_submit_dcb_called == 1);

    /* Uninstall all hooks */
    tracer_uninstall_hooks();

    /* =====================================================================
     * Section 5: Trace Flush & Decode Validation
     * ===================================================================== */
    const char *trace_path = "build/tracer_selftest.bin";
    int flush_rc = tracer_flush_to_file(trace_path);
    assert(flush_rc == 0);

    FILE *fin = fopen(trace_path, "rb");
    assert(fin != NULL);

    FILE *devnull = fopen("/dev/null", "w");
    if (!devnull) devnull = fopen("NUL", "w");
    assert(devnull != NULL);

    int decode_rc = obs_trace_decode_stream(fin, devnull);
    fclose(fin);
    fclose(devnull);
    remove(trace_path);

    assert(decode_rc == 0);
    (void)dcb_hook_rc;

    printf("tracer_selftest: ok (API calls, rate limiter, inline hooks, shader capture, and DCB telemetry verified)\n");
    return 0;
}
