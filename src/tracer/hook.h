#ifndef OOPS_TRACER_HOOK_H
#define OOPS_TRACER_HOOK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACER_DETOUR_SIZE 14u
#define TRACER_MAX_HOOKS 32u

typedef struct tracer_hook {
    void *target_fn;
    void *hook_fn;
    void *trampoline;
    uint8_t orig_bytes[32];
    size_t patch_size;
    int installed;
} tracer_hook_t;

/* Initialize the hooking subsystem and trampoline pool */
int tracer_hook_subsystem_init(void);

/* Install an inline detour hook on target_fn redirecting to hook_fn */
int tracer_hook_install(tracer_hook_t *hook, void *target_fn, void *hook_fn, size_t patch_size);

/* Uninstall an inline detour hook, restoring original instructions */
int tracer_hook_uninstall(tracer_hook_t *hook);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_TRACER_HOOK_H */

