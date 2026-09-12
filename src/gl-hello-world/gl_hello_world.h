#ifndef GL_HELLO_WORLD_H
#define GL_HELLO_WORLD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gl_hw_metrics {
    uint32_t fence_val;
    int      fence_hit;
    uint32_t canary_vs;
    uint32_t canary_vs_done;
    uint32_t canary_ps;
    uint32_t canary_ps_done;
    uint32_t canary_exec;
    uint32_t canary_v0;
    uint32_t canary_v1;
    uint32_t pixels_modified;
    int      rc_queue;
    int      rc_submit;
    uint32_t bytes_submitted;
    float    rotation_angle;
    uint32_t current_color;
} gl_hw_metrics_t;

typedef struct gl_triangle_pipeline {
    void               *agc_queue;
    uint8_t            *gpu_payload;
    volatile uint32_t  *fence;
    volatile uint32_t  *canary;
    uint32_t           *dcb_mem;
    uint32_t            dcb_capacity_dw;
    uint32_t           *target_buffer;
    uint32_t            target_width;
    uint32_t            target_height;
    uint64_t            frame_count;
    gl_hw_metrics_t     metrics;
    bool                initialized;
    bool                use_hardware;
} gl_triangle_pipeline_t;

/* Initialize hardware RDNA2 3D triangle pipeline */
int gl_triangle_init(gl_triangle_pipeline_t *pipe, uint32_t *target_buffer,
                     uint32_t width, uint32_t height);

/* Dispatch one frame of hardware 3D rasterization */
int gl_triangle_dispatch(gl_triangle_pipeline_t *pipe, uint32_t clear_color,
                         uint32_t prim_color);

/* Release pipeline resources */
void gl_triangle_fini(gl_triangle_pipeline_t *pipe);

#ifdef __cplusplus
}
#endif

#endif /* GL_HELLO_WORLD_H */

