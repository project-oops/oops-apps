#ifndef OOPS_TRACER_H
#define OOPS_TRACER_H

#include <stdint.h>
#include <stddef.h>
#include "trace_format.h"
#include "trace_encode.h"
#include "hook.h"
#include "shader_dump.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the in-process tracer with caller-provided buffer */
void tracer_init(struct obs_trace_rec *storage, uint32_t cap);

/* Record an API function call entry */
void tracer_record_entry(uint16_t tid, uint32_t seq, uint64_t nid, const uint64_t *args,
                         uint8_t argc);

/* Record an API function return */
void tracer_record_exit(uint16_t tid, uint32_t seq, uint64_t nid, uint64_t ret);

/* Record out-parameter buffer */
void tracer_record_outbuf(uint64_t nid, uint64_t addr, const uint8_t *buf,
                          uint32_t len);

/* Record a GPU shader registration event */
void tracer_record_shader(uint64_t addr, uint32_t size, uint32_t stage,
                          const uint8_t *code);

/* Record an AGC command buffer submission event */
void tracer_record_dcb(uint64_t addr, uint32_t dwords, uint32_t queue);

/* Flush the current buffer to a binary trace file */
int tracer_flush_to_file(const char *path);

/* Global hook structures */
extern tracer_hook_t g_hook_agc_submit_dcb;
extern tracer_hook_t g_hook_video_out_flip;

/* Install hooks on AGC and video presentation */
int tracer_install_hooks(void *p_create_shader, void *p_submit_dcb,
                         void *p_submit_flip);

/* Uninstall all installed tracer hooks */
void tracer_uninstall_hooks(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_TRACER_H */
