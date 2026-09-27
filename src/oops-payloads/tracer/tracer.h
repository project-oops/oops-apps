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
extern tracer_hook_t g_hook_apr_resolve;
extern tracer_hook_t g_hook_mapper_param;
extern tracer_hook_t g_hook_sce_open;
extern tracer_hook_t g_hook_posix_open;
extern tracer_hook_t g_hook_sce_close;
extern tracer_hook_t g_hook_posix_close;
extern tracer_hook_t g_hook_sce_fstat;
extern tracer_hook_t g_hook_posix_fstat;
extern tracer_hook_t g_hook_sysmodule_load;
extern tracer_hook_t g_hook_alloc_direct_mem;
extern tracer_hook_t g_hook_map_direct_mem;

/* Target symbol bundle for bulk installation */
typedef struct tracer_symbols {
    void *p_create_shader;
    void *p_submit_dcb;
    void *p_submit_flip;
    void *p_apr_resolve;
    void *p_mapper_param;
    void *p_sce_open;
    void *p_posix_open;
    void *p_sce_close;
    void *p_posix_close;
    void *p_sce_fstat;
    void *p_posix_fstat;
    void *p_sysmodule_load;
    void *p_alloc_direct_mem;
    void *p_map_direct_mem;
} tracer_symbols_t;

/* Install hooks using an explicit symbol bundle */
int tracer_install_symbols(const tracer_symbols_t *syms);

/* Install hooks on AGC and video presentation (backward compatible) */
int tracer_install_hooks(void *p_create_shader, void *p_submit_dcb,
                         void *p_submit_flip);

/* Interceptor hook functions */
int hook_sceKernelAprResolveFilepathsToIdsAndFileSizes(const char **paths,
                                                       uint32_t count, uint32_t *ids,
                                                       uint64_t *sizes,
                                                       uint32_t *statuses, void *arg5);
int hook_sceKernelMapperGetParam(void *param_buf);
int hook_sceKernelOpen(const char *path, int flags, int mode);
int hook_posix_open(const char *path, int flags, int mode);
int hook_sceKernelClose(int fd);
int hook_posix_close(int fd);
int hook_sceKernelFstat(int fd, void *sb);
int hook_posix_fstat(int fd, void *sb);
int hook_sceSysmoduleLoadModule(uint16_t id);
int hook_sceKernelAllocateDirectMemory(int64_t search_low, int64_t search_high,
                                       size_t len, size_t alignment, int mem_type,
                                       int64_t *phys_out);
int hook_sceKernelMapDirectMemory(void **addr_out, size_t len, int prot, int flags,
                                  int64_t direct_mem, size_t alignment);

/* Uninstall all installed tracer hooks */
void tracer_uninstall_hooks(void);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_TRACER_H */
