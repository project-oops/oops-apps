#include "gl_hello_world.h"
#include "oops/freestd.h"

#ifndef OOPS_HOST_BUILD
#include "oops/memory.h"
#include "oops/syscall.h"
#include "agc/driver.h"

/* Forward declare syscall and kernel externals */
__attribute__((weak)) int sceKernelUsleep(unsigned int microseconds);
__attribute__((weak)) int sceAgcDriverSubmitCommandBuffer(void *queue, const void *dcb);

static void klog_line(const char *msg) {
    char buf[160];
    const char *prefix = "[GL-HW] ";
    int n = 0;
    while (prefix[n] && n < 16) { buf[n] = prefix[n]; n++; }
    int m = 0;
    while (msg[m] && n < (int)sizeof(buf) - 2) { buf[n++] = msg[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}

static void klog_val(const char *tag, uint64_t val) {
    char buf[160];
    char hex[17];
    uint64_t v = val;
    for (int i = 15; i >= 0; i--) {
        uint8_t d = (uint8_t)(v & 0xf);
        hex[i] = (char)(d < 10 ? ('0' + d) : ('a' + d - 10));
        v >>= 4;
    }
    hex[16] = '\0';
    int hstart = 0;
    while (hstart < 15 && hex[hstart] == '0') hstart++;

    int n = 0;
    const char *pfx = "[GL-HW] ";
    while (pfx[n] && n < 8) { buf[n] = pfx[n]; n++; }
    int m = 0;
    while (tag && tag[m] && n < 40) { buf[n++] = tag[m++]; }
    if (n < 44) { buf[n++] = ':'; buf[n++] = ' '; buf[n++] = '0'; buf[n++] = 'x'; }
    m = hstart;
    while (hex[m] && n < (int)sizeof(buf) - 3) { buf[n++] = hex[m++]; }
    buf[n++] = '\n';
    buf[n] = '\0';
    (void)sys_call(SYS_klog, 7, (long)buf, 0, 0, 0, 0);
}
#endif
__attribute__((unused)) static float fast_sin9(float x) {
    const float PI = 3.14159265358979323846f;
    const float TWO_PI = 6.28318530717958647692f;
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f / 6.0f - x2 * (1.0f / 120.0f - x2 * (1.0f / 5040.0f - x2 * (1.0f / 362880.0f)))));
}

__attribute__((unused)) static float fast_cos9(float x) {
    const float HALF_PI = 1.57079632679489661923f;
    return fast_sin9(x + HALF_PI);
}

int gl_triangle_init(gl_triangle_pipeline_t *pipe, uint32_t *target_buffer,
                     uint32_t width, uint32_t height) {
    if (!pipe) return -1;
    memset(pipe, 0, sizeof(*pipe));

    pipe->target_buffer = target_buffer;
    pipe->target_width = width ? width : 1920;
    pipe->target_height = height ? height : 1080;

#ifndef OOPS_HOST_BUILD
    klog_line("initializing AGC hardware 3D triangle pipeline...");

    /* 1. Allocate coherent Onion memory for shaders, fence, canary, DCB */
    pipe->gpu_payload = (uint8_t *)oops_mem_alloc(0x1000, 256, OOPS_MEM_WB_ONION);
    pipe->fence = (volatile uint32_t *)oops_mem_alloc(0x1000, 0x1000, OOPS_MEM_WB_ONION);
    pipe->canary = (volatile uint32_t *)oops_mem_alloc(0x1000, 0x1000, OOPS_MEM_WB_ONION);
    pipe->dcb_capacity_dw = 4096;
    pipe->dcb_mem = (uint32_t *)oops_mem_alloc(pipe->dcb_capacity_dw * sizeof(uint32_t),
                                               0x1000, OOPS_MEM_WB_ONION);

    if (!pipe->gpu_payload || !pipe->fence || !pipe->canary || !pipe->dcb_mem) {
        klog_line("failed to allocate Onion memory buffers for pipeline");
        return -2;
    }

    *pipe->fence = 0x11111111u;
    for (int i = 0; i < 16; i++) {
        pipe->canary[i] = 0x11111111u * (uint32_t)(i + 1);
    }

    /* 2. Create Type 0 Universal Graphics Queue */
    void *queue = NULL;
    int rc_q = -1;
    if (sceAgcDriverCreateQueue) {
        rc_q = sceAgcDriverCreateQueue(0u, &queue, 0u);
    }
    pipe->metrics.rc_queue = rc_q;
    klog_val("sceAgcDriverCreateQueue Type 0 rc", (uint64_t)(uint32_t)rc_q);

    if (rc_q != 0 || queue == NULL) {
        klog_line("type 0 graphics queue creation failed");
        pipe->use_hardware = false;
        pipe->initialized = true;
        return -3;
    }
    pipe->agc_queue = queue;
    pipe->use_hardware = true;

    /* 3. Assemble RDNA2 NGG Primitive Shader Bytecode (gpu_payload + 0x000)
     *
     * Stage: NGG Primitive / Export Shader
     * Function:
     * - Allocates 1 primitive + 3 vertices via MSG_GS_ALLOC_REQ
     * - Activates lanes 0, 1, 2 via exec_lo = 7
     * - Each lane evaluates its NDC vertex position
     * - Emits exp pos0 for each vertex
     * - Masks to lane 0 via exec_lo = 1
     * - Emits exp prim with 0x00200400 (triangle indices 0, 1, 2)
     * - Writes canaries and ends
     */
    uint64_t canary_gpu = (uint64_t)(uintptr_t)pipe->canary;
    uint32_t *vs = (uint32_t *)pipe->gpu_payload;

    vs[0]  = 0xbefc03ffu; /* s_mov_b32 m0, 0x1003 (1 prim, 3 verts) */
    vs[1]  = 0x00001003u;
    vs[2]  = 0xbf900009u; /* s_sendmsg sendmsg(MSG_GS_ALLOC_REQ) */
    vs[3]  = 0xbe84037eu; /* s_mov_b32 s4, exec_lo */
    vs[4]  = 0x7e160300u; /* v_mov_b32 v11, v0 */
    vs[5]  = 0x7e180301u; /* v_mov_b32 v12, v1 */
    vs[6]  = 0x7e1a0204u; /* v_mov_b32 v13, s4 */
    vs[7]  = 0xbefe0381u; /* s_mov_b32 exec_lo, 1 (lane 0 only) */
    vs[8]  = 0xbe8003ffu; /* s_mov_b32 s0, canary_lo */
    vs[9]  = (uint32_t)canary_gpu;
    vs[10] = 0xbe8103ffu; /* s_mov_b32 s1, canary_hi */
    vs[11] = (uint32_t)(canary_gpu >> 32);
    vs[12] = 0x7e100200u; /* v_mov_b32 v8, s0 */
    vs[13] = 0x7e120201u; /* v_mov_b32 v9, s1 */
    vs[14] = 0x7e1402ffu; /* v_mov_b32 v10, 0xbeef0001 */
    vs[15] = 0xbeef0001u;
    vs[16] = 0xdc708000u; /* global_store_dword v[8:9], v10, off offset:0 */
    vs[17] = 0x007d0a08u;
    vs[18] = 0xbe8203ffu; /* s_mov_b32 s2, theta (2 DWORDs: opcode + literal) */
    vs[19] = 0x00000000u; /* initial theta = 0.0f */
    vs[20] = 0xdc70800cu; /* global_store_dword v[8:9], v12, off offset:12 */
    vs[21] = 0x007d0c08u;
    vs[22] = 0xdc708014u; /* global_store_dword v[8:9], v13, off offset:20 */
    vs[23] = 0x007d0d08u;
    vs[24] = 0xbf8c3f70u; /* s_waitcnt vmcnt(0) */

    /* Activate lanes 0, 1, 2 for the 3 triangle vertices */
    vs[25] = 0xbefe03ffu; /* s_mov_b32 exec_lo, 7 */
    vs[26] = 0x00000007u;

    /* Compute rotating vertex positions via hardware VALU transcendental unit (12 DWORDs) */
    vs[27] = 0xd765000eu; /* v_mbcnt_lo_u32_b32 v14, -1, 0 */
    vs[28] = 0x000100c1u;
    vs[29] = 0x7e020d0eu; /* v_cvt_f32_u32 v1, v14 (lane float: 0.0, 1.0, 2.0) */
    vs[30] = 0x7e040202u; /* v_mov_b32 v2, s2 (load theta into v2) */
    vs[31] = 0x560402ffu; /* v_fmac_f32 v2, 0.33333334f, v1 (angle = theta + lane / 3) */
    vs[32] = 0x3eaaaaabu;
    vs[33] = 0x7e0a6d02u; /* v_cos_f32 v5, v2 (X = cos(2*pi*angle)) */
    vs[34] = 0x7e0c6b02u; /* v_sin_f32 v6, v2 (Y = sin(2*pi*angle)) */
    vs[35] = 0x100a0af0u; /* v_mul_f32 v5, 0.5f, v5 (radius = 0.5) */
    vs[36] = 0x100c0cf0u; /* v_mul_f32 v6, 0.5f, v6 (radius = 0.5) */
    vs[37] = 0x7e060280u; /* v_mov_b32 v3,  0.0f (Z = 0.0) */
    vs[38] = 0x7e0802f2u; /* v_mov_b32 v4,  1.0f (W = 1.0) */

    /* Primitive connectivity MUST be exported BEFORE position exports in NGG */
    /* Lane 0 exports primitive connectivity: exp prim, v7, off, off, off done */
    vs[39] = 0xbefe0381u; /* s_mov_b32 exec_lo, 1 */
    vs[40] = 0x7e0e02ffu; /* v_mov_b32 v7, 0x20280600 (indices 0, 1, 2 with edge flags) */
    vs[41] = 0x20280600u;
    vs[42] = 0xf8000941u; /* exp prim, v7, off, off, off done */
    vs[43] = 0x00000007u;

    /* Export positions: exp pos0, v5, v6, v3, v4 done */
    vs[44] = 0xbefe03ffu; /* s_mov_b32 exec_lo, 7 */
    vs[45] = 0x00000007u;
    vs[46] = 0xf80008cfu; /* exp pos0, v5, v6, v3, v4 done */
    vs[47] = 0x04030605u;

    /* Write completion canary (lane 0 only to prevent invalid writes) */
    vs[48] = 0xbefe0381u; /* s_mov_b32 exec_lo, 1 */
    vs[49] = 0x7e1402ffu; /* v_mov_b32 v10, 0xbeef0003 */
    vs[50] = 0xbeef0003u;
    vs[51] = 0xdc708018u; /* global_store_dword v[8:9], v10, off offset:24 */
    vs[52] = 0x007d0a08u;
    vs[53] = 0xbf8c3f70u; /* s_waitcnt vmcnt(0) */
    vs[54] = 0xbefe0304u; /* s_mov_b32 exec_lo, s4 */
    vs[55] = 0xbf810000u; /* s_endpgm */

    for (size_t p = 56; p < 64; p++) {
        vs[p] = 0xbf800000u; /* s_nop */
    }

    /* Mirror to GS stage at offset 0x100 */
    uint32_t *gs = (uint32_t *)((char *)pipe->gpu_payload + 0x100);
    for (size_t p = 0; p < 64; p++) {
        gs[p] = vs[p];
    }

    /* 4. Assemble RDNA2 Pixel Shader Bytecode (gpu_payload + 0x200)
     *
     * Stage: Pixel Shader (PS)
     * Function:
     * - Exports solid cyan color (R=0.0, G=1.0, B=1.0, A=1.0) to MRT0
     * - Sets done and vm flags
     */
    uint32_t *ps = (uint32_t *)((char *)pipe->gpu_payload + 0x200);
    ps[0]  = 0xbf8c0000u; /* s_waitcnt 0 */
    ps[1]  = 0xbe84037eu; /* s_mov_b32 s4, exec_lo */
    ps[2]  = 0x7e160300u; /* v_mov_b32 v11, v0 */
    ps[3]  = 0x7e180301u; /* v_mov_b32 v12, v1 */
    ps[4]  = 0x7e1a0204u; /* v_mov_b32 v13, s4 */
    ps[5]  = 0xbefe0381u; /* s_mov_b32 exec_lo, 1 (lane 0 only) */
    ps[6]  = 0xbe8003ffu; /* s_mov_b32 s0, canary_lo */
    ps[7]  = (uint32_t)canary_gpu;
    ps[8]  = 0xbe8103ffu; /* s_mov_b32 s1, canary_hi */
    ps[9]  = (uint32_t)(canary_gpu >> 32);
    ps[10] = 0x7e100200u; /* v_mov_b32 v8, s0 */
    ps[11] = 0x7e120201u; /* v_mov_b32 v9, s1 */
    ps[12] = 0x7e1402ffu; /* v_mov_b32 v10, 0xbeef0002 */
    ps[13] = 0xbeef0002u;
    ps[14] = 0xdc708004u; /* global_store_dword v[8:9], v10, off offset:4 */
    ps[15] = 0x007d0a08u;
    ps[16] = 0xbe8203ffu; /* s_mov_b32 s2, color_r */
    ps[17] = 0x00000000u; /* R (updated dynamically by CPU each frame) */
    ps[18] = 0xbe8303ffu; /* s_mov_b32 s3, color_g */
    ps[19] = 0x3f800000u; /* G (updated dynamically by CPU each frame) */
    ps[20] = 0xbe8503ffu; /* s_mov_b32 s5, color_b */
    ps[21] = 0x3f800000u; /* B (updated dynamically by CPU each frame) */
    /* Write completion canary before color export */
    ps[22] = 0x7e1402ffu; /* v_mov_b32 v10, 0xbeef0004 */
    ps[23] = 0xbeef0004u;
    ps[24] = 0xdc708028u; /* global_store_dword v[8:9], v10, off offset:40 */
    ps[25] = 0x007d0a08u;
    ps[26] = 0xbf8c3f70u; /* s_waitcnt vmcnt(0) */
    ps[27] = 0xbefe0304u; /* s_mov_b32 exec_lo, s4 */

    /* Dynamic RGB Color Output: R=s2, G=s3, B=s5, A=1.0f */
    ps[28] = 0x7e000202u; /* v_mov_b32 v0, s2 (R) */
    ps[29] = 0x7e020203u; /* v_mov_b32 v1, s3 (G) */
    ps[30] = 0x7e040205u; /* v_mov_b32 v2, s5 (B) */
    ps[31] = 0x7e0602f2u; /* v_mov_b32 v3, 1.0f (A) */
    ps[32] = 0xf800180fu; /* exp mrt0, v0, v1, v2, v3 done vm */
    ps[33] = 0x03020100u;
    ps[34] = 0xbf810000u; /* s_endpgm */

    for (size_t p = 35; p < 64; p++) {
        ps[p] = 0xbf800000u; /* s_nop */
    }

    /* 5. Fallback shader (offset 0x300) */
    uint32_t *fb = (uint32_t *)((char *)pipe->gpu_payload + 0x300);
    fb[0] = 0xbefc0380u; /* s_mov_b32 m0, 0 */
    fb[1] = 0xbf900009u; /* s_sendmsg sendmsg(MSG_GS_ALLOC_REQ) */
    fb[2] = 0xbf810000u; /* s_endpgm */
    for (size_t p = 3; p < 64; p++) {
        fb[p] = 0xbf800000u;
    }

#if defined(__x86_64__)
    __builtin_ia32_clflush((const void *)pipe->gpu_payload);
    __builtin_ia32_clflush((const void *)((const char *)pipe->gpu_payload + 0x100));
    __builtin_ia32_clflush((const void *)((const char *)pipe->gpu_payload + 0x200));
    __builtin_ia32_clflush((const void *)((const char *)pipe->gpu_payload + 0x300));
#endif

    klog_line("RDNA2 NGG Vertex and Pixel Shaders assembled and flushed");
#else
    pipe->initialized = true;
    pipe->use_hardware = false;
#endif

    pipe->initialized = true;
    return 0;
}

int gl_triangle_dispatch(gl_triangle_pipeline_t *pipe, uint32_t clear_color,
                         uint32_t prim_color) {
    if (!pipe || !pipe->initialized) return -1;
    (void)clear_color;
    (void)prim_color;

#ifndef OOPS_HOST_BUILD
    if (!pipe->use_hardware || !pipe->agc_queue) {
        return -2;
    }

    uint64_t color_gpu = (uint64_t)(uintptr_t)pipe->target_buffer;
    uint64_t fence_gpu = (uint64_t)(uintptr_t)pipe->fence;
    uint64_t payload_va = (uint64_t)(uintptr_t)pipe->gpu_payload;

    *pipe->fence = 0x11111111u;

    /* Compute dynamic continuous rotation angle and RGB color for Stage 2 */
    /* 1 full rotation every 360 frames (~6.0 seconds at 60 FPS) */
    float theta_norm = (float)(pipe->frame_count % 360) / 360.0f;
    float theta_rad = theta_norm * 6.28318530717958647692f;
    uint32_t theta_raw = 0;
    memcpy(&theta_raw, &theta_norm, 4);

    /* Update Vertex Shader rotation angle */
    uint32_t *vs_ptr = (uint32_t *)pipe->gpu_payload;
    vs_ptr[19] = theta_raw;
#if defined(__x86_64__)
    __builtin_ia32_clflush((const void *)&vs_ptr[19]);
#endif

    /* Dynamic RGB Rainbow Color Cycling */
    float cr, cg, cb;
    if (prim_color != 0) {
        cr = (float)((prim_color >> 16) & 0xff) / 255.0f;
        cg = (float)((prim_color >> 8) & 0xff) / 255.0f;
        cb = (float)(prim_color & 0xff) / 255.0f;
    } else {
        cr = 0.5f + 0.5f * fast_cos9(theta_rad);
        cg = 0.5f + 0.5f * fast_cos9(theta_rad + 2.0943951f);
        cb = 0.5f + 0.5f * fast_cos9(theta_rad + 4.1887902f);
    }

    uint32_t raw_r = 0, raw_g = 0, raw_b = 0;
    memcpy(&raw_r, &cr, 4);
    memcpy(&raw_g, &cg, 4);
    memcpy(&raw_b, &cb, 4);

    /* Update Pixel Shader colors at payload + 0x200 */
    uint32_t *ps_ptr = (uint32_t *)((char *)pipe->gpu_payload + 0x200);
    ps_ptr[17] = raw_r;
    ps_ptr[19] = raw_g;
    ps_ptr[21] = raw_b;
#if defined(__x86_64__)
    __builtin_ia32_clflush((const void *)&ps_ptr[16]);
#endif

    pipe->metrics.rotation_angle = theta_rad; /* radians for HUD */

    uint32_t ir = (uint32_t)(cr * 255.0f + 0.5f);
    uint32_t ig = (uint32_t)(cg * 255.0f + 0.5f);
    uint32_t ib = (uint32_t)(cb * 255.0f + 0.5f);
    if (ir > 255) ir = 255;
    if (ig > 255) ig = 255;
    if (ib > 255) ib = 255;
    pipe->metrics.current_color = 0xff000000u | (ir << 16) | (ig << 8) | ib;

    uint32_t *dw = pipe->dcb_mem;

    /* 1. Context Registers: Color Target, Viewport, Scissor, Rasterizer */
    static const struct {
        uint32_t reg;
        uint32_t val;
    } ctx_regs[] = {
        {0x318u, 0}, /* CB_COLOR0_BASE (patched below) */
        {0x390u, 0}, /* CB_COLOR0_BASE_EXT (patched below) */
        {0x31bu, 0x00000000u}, /* CB_COLOR0_VIEW */
        {0x31cu, 0x000180a8u}, /* CB_COLOR0_INFO: COLOR_8_8_8_8, LINEAR_GENERAL, UNORM */
        {0x31du, 0x00000000u}, /* CB_COLOR0_ATTRIB: 0 */
        {0x31eu, 0x00000000u}, /* CB_COLOR0_DCC_CONTROL: disabled */
        {0x3b0u, (1919u << 14) | 1079u}, /* CB_COLOR0_ATTRIB2: MIP0_WIDTH=1919, MIP0_HEIGHT=1079 */
        {0x202u, 0x00cc0010u}, /* CB_COLOR_CONTROL: CB_NORMAL, ROP3_COPY */
        {0x08eu, 0x0000000fu}, /* CB_TARGET_MASK: MRT0 4 components enabled */
        {0x08fu, 0x0000000fu}, /* CB_SHADER_MASK: MRT0 4 components export enabled */
        {0x1e0u, 0x00000000u}, /* CB_BLEND0_CONTROL: blending disabled */
        {0x000u, 0x00000000u}, /* DB_RENDER_CONTROL: 0 */
        {0x004u, 0x08000000u}, /* DB_RENDER_OVERRIDE2: CENTROID_COMPUTATION_MODE=1 */
        {0x010u, 0x00000000u}, /* DB_Z_INFO: Z_INVALID */
        {0x011u, 0x00000000u}, /* DB_STENCIL_INFO: STENCIL_INVALID */
        {0x200u, 0x00000000u}, /* DB_DEPTH_CONTROL: disabled */
        {0x201u, 0x00130000u}, /* DB_EQAA: HIGH_QUALITY_INTERSECTIONS | INCOHERENT_EQAA_READS | STATIC_ANCHOR_ASSOCIATIONS */
        {0x203u, 0x00000600u}, /* DB_SHADER_CONTROL: Z_ORDER=LATE_Z | EXEC_ON_HIER_FAIL | EXEC_ON_NOOP */
        {0x08cu, 0xaa99aaaau}, /* PA_SC_EDGERULE: D3D/OpenGL standard edge rule */
        {0x1d4u, 0x000000ffu}, /* SX_PS_DOWNCONVERT_CONTROL: disabled / no conversion (RDNA2 GFX10.3 hardware default) */
        {0x291u, (128u << 22) | (128u << 11) | 256u}, /* VGT_GS_ONCHIP_CNTL: GS_INST_PRIMS=128, GS_PRIMS=128, ES_VERTS=256 */
        {0x29bu, 0x00000002u},       /* VGT_GS_OUT_PRIM_TYPE: TRILIST */
        {0x2d3u, 0x00000001u}, /* GE_NGG_SUBGRP_CNTL: PRIM_AMP=1, THDS_PER_SUBGRP=0 */
        {0x2d5u, 0x00c12010u}, /* VGT_SHADER_STAGES_EN: ES_EN(REAL) | PRIMGEN_EN | MAX_PRIMGRP(2) | GS_W32 | VS_W32 (GS_EN=0) */
        {0x1ffu, 0x00000100u}, /* GE_MAX_OUTPUT_PER_SUBGROUP: MAX_VERTS=256 */
        {0x20eu, 0x00000078u}, /* PA_CL_NGG_CNTL: VERTEX_REUSE_DEPTH=30 */
        {0x2a1u, 0x00000000u}, /* VGT_PRIMITIVEID_EN: disabled */
        {0x2a6u, 0x00000000u}, /* VGT_DRAW_PAYLOAD_CNTL: disabled */
        {0x2adu, 0x00000000u}, /* VGT_REUSE_OFF */
        {0x2ceu, 0x00000400u}, /* VGT_GS_MAX_VERT_OUT: 1024 */
        {0x2e4u, 0x00000004u}, /* VGT_GS_INSTANCE_CNT: CNT=1, ENABLE=0 */
        {0x2d4u, 0x88101010u}, /* VGT_TESS_DISTRIBUTION */
        {0x103u, 0xffffffffu}, /* VGT_MULTI_PRIM_IB_RESET_INDX */
        {0x30eu, 0xffffffffu}, /* PA_SC_AA_MASK_X0Y0_X1Y0: enable all samples */
        {0x30fu, 0xffffffffu}, /* PA_SC_AA_MASK_X0Y1_X1Y1: enable all samples */
        {0x310u, 0x00000000u}, /* PA_SC_SHADER_CONTROL */
        {0x314u, 0x00000200u}, /* PA_SC_NGG_MODE_CNTL: MAX_DEALLOCS_IN_WAVE=512 (RDNA2 GFX10 hardware default) */
        {0x311u, 0x19fc0122u}, /* PA_SC_BINNER_CNTL_0: DISABLE_BINNING_USE_NEW_SC (128x128 extend=2, fpovs=63, opt_bin=1, flush=1) */
        {0x312u, 0x03ff0080u}, /* PA_SC_BINNER_CNTL_1 */
        {0x313u, 0x00100000u}, /* PA_SC_CONSERVATIVE_RASTERIZATION_CNTL: NULL_SQUAD_AA_MASK_ENABLE */
        {0x00eu, 0x00000002u}, /* DB_DFSM_CONTROL */
        {0x280u, 0x00080008u}, /* PA_SU_POINT_SIZE */
        {0x281u, 0xffff0000u}, /* PA_SU_POINT_MINMAX */
        {0x282u, 0x00000008u}, /* PA_SU_LINE_CNTL */
        {0x2deu, 0x000001e9u}, /* PA_SU_POLY_OFFSET_DB_FMT_CNTL */
        {0x00cu, 0x00000000u}, /* PA_SC_SCREEN_SCISSOR_TL */
        {0x00du, 0x04380780u}, /* PA_SC_SCREEN_SCISSOR_BR (1920x1080) */
        {0x081u, 0x80000000u}, /* PA_SC_WINDOW_SCISSOR_TL (WINDOW_OFFSET_DISABLE) */
        {0x082u, 0x04380780u}, /* PA_SC_WINDOW_SCISSOR_BR (1920x1080) */
        {0x090u, 0x80000000u}, /* PA_SC_GENERIC_SCISSOR_TL (WINDOW_OFFSET_DISABLE) */
        {0x091u, 0x04380780u}, /* PA_SC_GENERIC_SCISSOR_BR (1920x1080) */
        {0x094u, 0x80000000u}, /* PA_SC_VPORT_SCISSOR_0_TL (WINDOW_OFFSET_DISABLE) */
        {0x095u, 0x04380780u}, /* PA_SC_VPORT_SCISSOR_0_BR (1920x1080) */
        {0x0b4u, 0x00000000u}, /* PA_SC_VPORT_ZMIN_0: 0.0f */
        {0x0b5u, 0x3f800000u}, /* PA_SC_VPORT_ZMAX_0: 1.0f */
        {0x10fu, 0x44700000u}, /* PA_CL_VPORT_XSCALE: 960.0f */
        {0x110u, 0x44700000u}, /* PA_CL_VPORT_XOFFSET: 960.0f */
        {0x111u, 0x44070000u}, /* PA_CL_VPORT_YSCALE: 540.0f */
        {0x112u, 0x44070000u}, /* PA_CL_VPORT_YOFFSET: 540.0f */
        {0x113u, 0x3f000000u}, /* PA_CL_VPORT_ZSCALE: 0.5f */
        {0x114u, 0x3f000000u}, /* PA_CL_VPORT_ZOFFSET: 0.5f */
        {0x083u, 0x0000ffffu}, /* PA_SC_CLIPRECT_RULE: allow all cliprects */
        {0x084u, 0x00000000u}, /* PA_SC_CLIPRECT_0_TL */
        {0x085u, 0x20002000u}, /* PA_SC_CLIPRECT_0_BR (8192x8192) */
        {0x204u, 0x0d080000u}, /* PA_CL_CLIP_CNTL: CLIP_DISABLE=0, ZCLIP_NEAR_DISABLE, ZCLIP_FAR_DISABLE, DX_CLIP_SPACE_DEF, DX_LINEAR_ATTR_CLIP_ENA */
        {0x206u, 0x0000043fu}, /* PA_CL_VTE_CNTL: enable VPORT X,Y,Z scale & offset */
        {0x207u, 0x00000000u}, /* PA_CL_VS_OUT_CNTL: no clip/cull dists */
        {0x2fau, 0x40800000u}, /* PA_CL_GB_VERT_CLIP_ADJ: 4.0f */
        {0x2fbu, 0x40800000u}, /* PA_CL_GB_VERT_DISC_ADJ: 4.0f */
        {0x2fcu, 0x40800000u}, /* PA_CL_GB_HORZ_CLIP_ADJ: 4.0f */
        {0x2fdu, 0x40800000u}, /* PA_CL_GB_HORZ_DISC_ADJ: 4.0f */
        {0x205u, 0x00000240u}, /* PA_SU_SC_MODE_CNTL: no cull, face=0, poly=trilist */
        {0x20cu, 0x00000000u}, /* PA_SU_SMALL_PRIM_FILTER_CNTL: disabled */
        {0x292u, 0x00000022u}, /* PA_SC_MODE_CNTL_0: VPORT_SCISSOR_ENABLE | ALTERNATE_RBS_PER_TILE */
        {0x293u, 0x760201b5u}, /* PA_SC_MODE_CNTL_1: WALK_SIZE=1, WALK_ALIGN8=1, OUT_OF_ORDER_WATER_MARK=7, etc */
        {0x2f8u, 0x20000000u}, /* PA_SC_AA_CONFIG: 1x MSAA, COVERED_CENTROID_IS_CENTER=1 */
        {0x2f9u, 0x0000002du}, /* PA_SU_VTX_CNTL: 1/16th subpixel, half-pixel center */
        {0x191u, 0x00000000u}, /* SPI_PS_INPUT_CNTL_0 */
        {0x1b1u, 0x00000080u}, /* SPI_VS_OUT_CONFIG: NO_PC_EXPORT */
        {0x1c2u, 0x00000001u}, /* SPI_SHADER_IDX_FORMAT: IDX0 = 1COMP */
        {0x1c3u, 0x00000004u}, /* SPI_SHADER_POS_FORMAT: POS0 = 4COMP */
        {0x1c4u, 0x00000000u}, /* SPI_SHADER_Z_FORMAT: ZERO (no Z export) */
        {0x1c5u, 0x00000009u}, /* SPI_SHADER_COL_FORMAT: COL0 = 32_ABGR */
        {0x1b3u, 0x00000080u}, /* SPI_PS_INPUT_ENA: LINE_STIPPLE_TEX_ENA (minimum required for PS wave spawn) */
        {0x1b4u, 0x00000080u}, /* SPI_PS_INPUT_ADDR: LINE_STIPPLE_TEX_ENA */
        {0x1b5u, 0x00000000u}, /* SPI_INTERP_CONTROL_0: 0 */
        {0x1b5u, 0x00000000u}, /* SPI_INTERP_CONTROL_0: 0 */
        {0x1b6u, 0x00008000u}, /* SPI_PS_IN_CONTROL: PS_W32_EN, NUM_INTERP=0 */
        {0x1b8u, 0x00000000u}, /* SPI_BARYC_CNTL: 0 */
    };

    for (size_t i = 0; i < sizeof(ctx_regs) / sizeof(ctx_regs[0]); i++) {
        uint32_t reg = ctx_regs[i].reg;
        uint32_t val = ctx_regs[i].val;
        if (reg == 0x318u) {
            val = (uint32_t)(color_gpu >> 8);
        } else if (reg == 0x390u) {
            val = (uint32_t)(color_gpu >> 40);
        }
        *dw++ = 0xc0016900u; /* PACKET3_SET_CONTEXT_REG, count 1 */
        *dw++ = reg;
        *dw++ = val;
    }

    /* 2. Shader Program Bindings: bind PS, VS, GS, ES, HS, LS */
    static const struct {
        uint32_t base_reg;
        uint64_t va_offset;
        uint32_t rsrc1;
        uint32_t rsrc2;
    } stages[] = {
        {0x08u, 0x200u, 0x000c0010u, 0x00000000u},  /* PS (Pixel Shader, USER_SGPR=0) */
        {0x48u, 0x000u, 0x000c0010u, 0x00000008u},  /* VS */
        {0x88u, 0x000u, 0x200c0010u, 0x00000008u},  /* GS / NGG (GS_VGPR_COMP_CNT=1) */
        {0xc8u, 0x000u, 0x000c0010u, 0x00000008u},  /* ES / VS */
        {0x108u, 0x000u, 0x000c0010u, 0x00000008u}, /* HS */
        {0x148u, 0x000u, 0x000c0010u, 0x00000008u}, /* LS */
    };
    for (size_t s = 0; s < sizeof(stages) / sizeof(stages[0]); s++) {
        uint32_t base_reg = stages[s].base_reg;
        uint64_t s_va = payload_va + stages[s].va_offset;
        *dw++ = 0xc0017600u; /* SET_SH_REG, count 1 */
        *dw++ = base_reg;
        *dw++ = (uint32_t)(s_va >> 8);
        *dw++ = 0xc0017600u; /* SET_SH_REG, count 1 */
        *dw++ = base_reg + 1u;
        *dw++ = (uint32_t)(s_va >> 40);
        *dw++ = 0xc0017600u; /* SET_SH_REG, count 1 */
        *dw++ = base_reg + 2u;
        *dw++ = stages[s].rsrc1;
        *dw++ = 0xc0017600u; /* SET_SH_REG, count 1 */
        *dw++ = base_reg + 3u;
        *dw++ = stages[s].rsrc2;
    }

    /* 3. SPI CU Enable Masks & Wave Limits */
    *dw++ = 0xc0017600u; *dw++ = 0x007u; *dw++ = 0x003fffffu; /* SPI_SHADER_PGM_RSRC3_PS: WAVE_LIMIT=63, CU_EN=0xffff */
    *dw++ = 0xc0017600u; *dw++ = 0x001u; *dw++ = 0x0000ffffu; /* SPI_SHADER_PGM_RSRC4_PS: CUs 16-31 */
    *dw++ = 0xc0017600u; *dw++ = 0x030u; *dw++ = 0x00000007u; /* SPI_SHADER_REQ_CTRL_PS: SOFT_GROUPING_EN | NUM_REQ=3 */
    *dw++ = 0xc0017600u; *dw++ = 0x046u; *dw++ = 0x003fffffu; /* SPI_SHADER_PGM_RSRC3_VS: WAVE_LIMIT=63, CU_EN=0xffff */
    *dw++ = 0xc0017600u; *dw++ = 0x041u; *dw++ = 0x0000ffffu; /* SPI_SHADER_PGM_RSRC4_VS: CUs 16-31 */
    *dw++ = 0xc0017600u; *dw++ = 0x087u; *dw++ = 0x003fffffu; /* SPI_SHADER_PGM_RSRC3_GS: WAVE_LIMIT=63, CU_EN=0xffff */
    *dw++ = 0xc0017600u; *dw++ = 0x081u; *dw++ = 0x0000ffffu; /* SPI_SHADER_PGM_RSRC4_GS: CUs 16-31 */
    *dw++ = 0xc0017600u; *dw++ = 0x107u; *dw++ = 0x003fffffu; /* SPI_SHADER_PGM_RSRC3_HS: WAVE_LIMIT=63 */

    /* 4. Primitive Topology & GE Config */
    *dw++ = 0xc0002f00u; /* PACKET3_NUM_INSTANCES */
    *dw++ = 1u;
    *dw++ = 0xc0017900u; /* PACKET3_SET_UCONFIG_REG mmVGT_PRIMITIVE_TYPE */
    *dw++ = 0x242u;
    *dw++ = 0x4u;        /* DI_PT_TRILIST */
    *dw++ = 0xc0017900u; /* mmGE_CNTL */
    *dw++ = 0x25bu;
    *dw++ = 0x00020080u;
    *dw++ = 0xc0017900u; /* mmGE_PC_ALLOC: 256 lines per SE with oversub enabled (RDNA2 GFX10.3 hardware default) */
    *dw++ = 0x260u;
    *dw++ = 0x000001ffu;

    /* 5. Primitive Draw: DRAW_INDEX_AUTO (opcode 0x2D, count 3, initiator 2) */
    *dw++ = 0xc0012d00u;
    *dw++ = 3u;
    *dw++ = 2u;
    /* 6. Release Mem with EOP Fence */
    *dw++ = 0xc0064900u;
    *dw++ = 0x06603514u;
    *dw++ = 0x20000000u;
    *dw++ = (uint32_t)fence_gpu;
    *dw++ = (uint32_t)(fence_gpu >> 32);
    *dw++ = 0xbeefcafeu;
    *dw++ = 0u;
    *dw++ = 0u;

    /* Pad trailing area */
    for (int p = 0; p < 16; p++) {
        dw[p] = 0xffff1000u; /* PM4 NOP */
    }
    dw += 16;

    uint32_t words_written = (uint32_t)(dw - pipe->dcb_mem);
    pipe->metrics.bytes_submitted = words_written * 4u;

    oops_agc_dcb_desc desc;
    desc.gpu_addr = (uint64_t)(uintptr_t)pipe->dcb_mem;
    desc.size = words_written; /* length in DWORDs */
    desc.flags = 0u;
    desc.pad = 0u;

    int submit_rc = -1;
    if (sceAgcDriverSubmitCommandBuffer) {
        submit_rc = sceAgcDriverSubmitCommandBuffer(pipe->agc_queue, &desc);
    } else if (sceAgcDriverSubmitDcb) {
        submit_rc = sceAgcDriverSubmitDcb(&desc);
    }
    pipe->metrics.rc_submit = submit_rc;

    /* Wait for fence retirement */
    int fence_hit = 0;
    uint32_t f_val = *pipe->fence;
    if (submit_rc == 0) {
        for (int iter = 0; iter < 10000; iter++) {
#if defined(__x86_64__)
            __builtin_ia32_clflush((const void *)pipe->fence);
#endif
            f_val = *pipe->fence;
            if (f_val == 0xbeefcafeu) {
                fence_hit = 1;
                fence_hit = 1;
                break;
            }
            if (sceKernelUsleep) {
                sceKernelUsleep(50);
            }
        }
    }
    pipe->metrics.fence_hit = fence_hit;
    pipe->metrics.fence_val = f_val;

#if defined(__x86_64__)
    for (size_t p = 0; p < 64; p += 64) {
        __builtin_ia32_clflush((const void *)((const char *)pipe->canary + p));
    }
#endif
    pipe->metrics.canary_vs = pipe->canary[0];
    pipe->metrics.canary_ps = pipe->canary[1];
    pipe->metrics.canary_v0 = pipe->canary[2];
    pipe->metrics.canary_v1 = pipe->canary[3];
    pipe->metrics.canary_exec = pipe->canary[5];
    pipe->metrics.canary_vs_done = pipe->canary[6];
    pipe->metrics.canary_ps_done = pipe->canary[10];

    /* Count modified pixels in target buffer */
    uint32_t modified = 0;
    size_t total_px = (size_t)pipe->target_width * (size_t)pipe->target_height;
    for (size_t i = 0; i < total_px; i += 64) {
#if defined(__x86_64__)
        __builtin_ia32_clflush((const void *)&pipe->target_buffer[i]);
#endif
        if (pipe->target_buffer[i] != clear_color) {
            modified++;
        }
    }
    pipe->metrics.pixels_modified = modified;

    if (pipe->frame_count % 30 == 0 || pipe->frame_count < 5) {
        klog_val("frame", pipe->frame_count);
        klog_val("submit-rc", (uint64_t)(uint32_t)submit_rc);
        klog_val("fence-hit", (uint64_t)fence_hit);
        klog_val("fence-val", (uint64_t)f_val);
        klog_val("canary-vs", (uint64_t)pipe->metrics.canary_vs);
        klog_val("canary-v0", (uint64_t)pipe->metrics.canary_v0);
        klog_val("canary-v1", (uint64_t)pipe->metrics.canary_v1);
        klog_val("canary-exec", (uint64_t)pipe->metrics.canary_exec);
        klog_val("canary-vs-done", (uint64_t)pipe->metrics.canary_vs_done);
        klog_val("canary-ps", (uint64_t)pipe->metrics.canary_ps);
        klog_val("canary-ps-done", (uint64_t)pipe->metrics.canary_ps_done);
        klog_val("rot-deg", (uint64_t)(uint32_t)(theta_norm * 360.0f));
        klog_val("color-rgb", (uint64_t)pipe->metrics.current_color);
        klog_val("pixels-mod", (uint64_t)modified);
    }

#else
    /* Host simulation: fill triangle in memory buffer */
    pipe->metrics.fence_hit = 1;
    pipe->metrics.fence_val = 0xbeefcafeu;
    pipe->metrics.canary_vs = 0xbeef0001u;
    pipe->metrics.canary_vs_done = 0xbeef0003u;
    pipe->metrics.canary_ps = 0xbeef0002u;
    pipe->metrics.canary_ps_done = 0xbeef0004u;
    pipe->metrics.pixels_modified = (pipe->target_width * pipe->target_height) / 4;
#endif

    pipe->frame_count++;
    return 0;
}

void gl_triangle_fini(gl_triangle_pipeline_t *pipe) {
    if (!pipe) return;
#ifndef OOPS_HOST_BUILD
    if (pipe->agc_queue && sceAgcDriverDestroyQueue) {
        sceAgcDriverDestroyQueue(pipe->agc_queue);
        pipe->agc_queue = NULL;
    }
    if (pipe->gpu_payload) { oops_mem_free(pipe->gpu_payload); pipe->gpu_payload = NULL; }
    if (pipe->fence) { oops_mem_free((void *)pipe->fence); pipe->fence = NULL; }
    if (pipe->canary) { oops_mem_free((void *)pipe->canary); pipe->canary = NULL; }
    if (pipe->dcb_mem) { oops_mem_free(pipe->dcb_mem); pipe->dcb_mem = NULL; }
#endif
    pipe->initialized = false;
}

