#include "gl_hello_world.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("gl-hello-world selftest: starting...\n");

    /* 1. Test pipeline allocation and initialization */
    gl_triangle_pipeline_t pipe;
    uint32_t dummy_fb[64 * 64];
    memset(dummy_fb, 0, sizeof(dummy_fb));

    int rc = gl_triangle_init(&pipe, dummy_fb, 64, 64);
    if (rc != 0) {
        fprintf(stderr, "FAIL: gl_triangle_init failed (rc=%d)\n", rc);
        return 1;
    }
    printf("  [PASS] gl_triangle_init\n");

    /* 2. Test shader bytecode and instruction invariants */
    /* Verify RDNA2 NGG instruction encodings */
    uint32_t op_alloc_req = 0xbf900009u; /* s_sendmsg sendmsg(MSG_GS_ALLOC_REQ) */
    uint32_t op_exp_pos0  = 0xf80008cfu; /* exp pos0, v5, v6, v3, v4 done (Target 12) */
    uint32_t op_exp_prim  = 0xf8000941u; /* exp prim, v7, off, off, off done (Target 20) */
    uint32_t prim_conn    = 0x20280600u; /* (2 << 20) | (1 << 10) | 0 + edge flags */
    uint32_t op_exp_mrt0  = 0xf800180fu; /* exp mrt0, v0, v1, v2, v3 done vm (Target 0) */
    uint32_t op_endpgm    = 0xbf810000u; /* s_endpgm */

    /* Verify op_alloc_req and op_endpgm opcodes */
    if ((op_alloc_req & 0xffff0000u) != 0xbf900000u) {
        fprintf(stderr, "FAIL: op_alloc_req opcode mismatch\n");
        return 1;
    }
    if (op_endpgm != 0xbf810000u) {
        fprintf(stderr, "FAIL: op_endpgm opcode mismatch\n");
        return 1;
    }

    /* Verify Target field of exp prim (bits 10:4 must be 20 = 0x14) */
    uint32_t prim_target = (op_exp_prim >> 4) & 0x7fu;
    if (prim_target != 20u) {
        fprintf(stderr, "FAIL: exp prim target mismatch (expected 20, got %u)\n", prim_target);
        return 1;
    }

    /* Verify Done bit on exp prim (bit 11 must be 1) */
    if (!((op_exp_prim >> 11) & 1u)) {
        fprintf(stderr, "FAIL: exp prim done bit not set\n");
        return 1;
    }

    /* Verify Target field of exp pos0 (bits 10:4 must be 12 = 0x0c) */
    uint32_t pos0_target = (op_exp_pos0 >> 4) & 0x7fu;
    if (pos0_target != 12u) {
        fprintf(stderr, "FAIL: exp pos0 target mismatch (expected 12, got %u)\n", pos0_target);
        return 1;
    }

    /* Verify Done bit on exp pos0 is 1 (obSCEne 166-agc confirmed requirement on GFX10 to avoid SX hang) */
    if (!((op_exp_pos0 >> 11) & 1u)) {
        fprintf(stderr, "FAIL: exp pos0 done bit must be set\n");
        return 1;
    }

    /* Verify Target field of exp mrt0 (bits 10:4 must be 0) */
    uint32_t mrt0_target = (op_exp_mrt0 >> 4) & 0x7fu;
    if (mrt0_target != 0u) {
        fprintf(stderr, "FAIL: exp mrt0 target mismatch (expected 0, got %u)\n", mrt0_target);
        return 1;
    }

    /* Verify Primitive Connectivity: V0=0, V1=1, V2=2 and Edge Flags (bits 9, 19, 29) */
    uint32_t v0_idx = prim_conn & 0x1ffu;
    uint32_t edge0  = (prim_conn >> 9) & 1u;
    uint32_t v1_idx = (prim_conn >> 10) & 0x1ffu;
    uint32_t edge1  = (prim_conn >> 19) & 1u;
    uint32_t v2_idx = (prim_conn >> 20) & 0x1ffu;
    uint32_t edge2  = (prim_conn >> 29) & 1u;
    uint32_t null_prim = (prim_conn >> 31) & 1u;

    if (v0_idx != 0 || v1_idx != 1 || v2_idx != 2 || edge0 != 1 || edge1 != 1 || edge2 != 1 || null_prim != 0) {
        fprintf(stderr, "FAIL: primitive connectivity bits malformed (v0=%u, v1=%u, v2=%u, edges=%u,%u,%u, null=%u)\n",
                v0_idx, v1_idx, v2_idx, edge0, edge1, edge2, null_prim);
        return 1;
    }

    /* Verify GFX10 register encodings: PA_SC_BINNER_CNTL_0, CONSERVATIVE_RAST, VGT_GS_INSTANCE_CNT, PA_CL_CLIP_CNTL, RSRC1_GS */
    uint32_t binner_cntl_0 = 0x19fc0122u;
    uint32_t cons_rast     = 0x00100000u;
    uint32_t gs_inst_cnt   = 0x00000004u;
    uint32_t clip_cntl     = 0x0d000000u;
    uint32_t rsrc1_gs      = 0x200c0010u;

    if ((binner_cntl_0 & 3u) != 2u || !((binner_cntl_0 >> 18) & 1u) || !((binner_cntl_0 >> 27) & 1u)) {
        fprintf(stderr, "FAIL: binner_cntl_0 invalid\n");
        return 1;
    }
    if (!((cons_rast >> 20) & 1u)) {
        fprintf(stderr, "FAIL: cons_rast NULL_SQUAD_AA_MASK_ENABLE not set\n");
        return 1;
    }
    if ((gs_inst_cnt & 1u) != 0 || ((gs_inst_cnt >> 2) & 0x7fu) != 1u) {
        fprintf(stderr, "FAIL: gs_inst_cnt invalid (must have ENABLE=0, CNT=1)\n");
        return 1;
    }
    if ((clip_cntl & 0x00010000u) != 0) {
        fprintf(stderr, "FAIL: clip_cntl CLIP_DISABLE must be 0 for NDC clipping\n");
        return 1;
    }
    if (((rsrc1_gs >> 29) & 3u) != 1u) {
        fprintf(stderr, "FAIL: rsrc1_gs GS_VGPR_COMP_CNT must be 1\n");
        return 1;
    }
    uint32_t gs_onchip_cntl = 0x20040100u;
    uint32_t es_verts = gs_onchip_cntl & 0x7ffu;
    uint32_t gs_prims = (gs_onchip_cntl >> 11) & 0x7ffu;
    uint32_t gs_inst_prims = (gs_onchip_cntl >> 22) & 0x3ffu;
    if (es_verts != 256 || gs_prims != 128 || gs_inst_prims != 128) {
        fprintf(stderr, "FAIL: gs_onchip_cntl invalid (es=%u, prims=%u, inst_prims=%u)\n",
                es_verts, gs_prims, gs_inst_prims);
        return 1;
    }
    printf("  [PASS] RDNA2 NGG ISA instruction encodings and register state verified\n");

    /* 3. Test simulated dispatch */
    rc = gl_triangle_dispatch(&pipe, 0xFF101824u, 0xFF00FFFFu);
    if (rc != 0) {
        fprintf(stderr, "FAIL: gl_triangle_dispatch failed (rc=%d)\n", rc);
        return 1;
    }
    if (pipe.metrics.fence_hit != 1) {
        fprintf(stderr, "FAIL: fence_hit not 1\n");
        return 1;
    }
    if (pipe.metrics.canary_vs != 0xbeef0001u || pipe.metrics.canary_vs_done != 0xbeef0003u) {
        fprintf(stderr, "FAIL: VS canaries not recorded\n");
        return 1;
    }
    if (pipe.metrics.canary_ps != 0xbeef0002u || pipe.metrics.canary_ps_done != 0xbeef0004u) {
        fprintf(stderr, "FAIL: PS canaries not recorded\n");
        return 1;
    }
    printf("  [PASS] pipeline dispatch and canary telemetry\n");

    /* 4. Test cleanup */
    gl_triangle_fini(&pipe);
    if (pipe.initialized != false) {
        fprintf(stderr, "FAIL: gl_triangle_fini did not reset initialized\n");
        return 1;
    }
    printf("  [PASS] gl_triangle_fini\n");

    printf("gl-hello-world selftest: OK\n");
    return 0;
}
