#ifndef OOPS_TRACER_SHADER_DUMP_H
#define OOPS_TRACER_SHADER_DUMP_H

#include <stdint.h>
#include <stddef.h>
#include "hook.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SHADER_STAGE_UNKNOWN 0u
#define SHADER_STAGE_CS      1u
#define SHADER_STAGE_VS      2u
#define SHADER_STAGE_PS      3u
#define SHADER_STAGE_GS      4u
#define SHADER_STAGE_HS      5u

#define AGC_CONTAINER_HEADER_SIZE 304u

/* Configure output directory for shader dumps (e.g. "/data/shaders" or "build/test_shaders") */
void shader_dump_set_directory(const char *dir);

/* Get the current dump directory */
const char *shader_dump_get_directory(void);

/* Number of distinct shaders dumped to disk */
uint32_t shader_dump_get_count(void);

/* Clear deduplication cache (useful for testing) */
void shader_dump_reset_cache(void);

/* Interceptor signature matching sceAgcCreateShader:
 * int sceAgcCreateShader(void *shader_obj, const void *header, void *gpu_payload, uint32_t flags);
 */
int hook_sceAgcCreateShader(void *shader_obj, const void *header, void *gpu_payload, uint32_t flags);

/* Global hook structure for sceAgcCreateShader */
extern tracer_hook_t g_hook_agc_create_shader;

#ifdef __cplusplus
}
#endif

#endif /* OOPS_TRACER_SHADER_DUMP_H */

