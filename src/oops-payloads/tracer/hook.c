#include "hook.h"

#if defined(OOPS_HOST_BUILD)
#include <sys/mman.h>
#include <string.h>
#else
#include "oops/freestd.h"
#include "oops/syscall.h"
#include "oops/krw.h"
#endif

#define TRAMPOLINE_SLOT_SIZE 64u
#define TRAMPOLINE_POOL_PAGES 1u
#define PAGE_SIZE 0x1000u

static uint8_t *s_trampoline_pool = NULL;
static size_t s_trampoline_used = 0;

#if defined(OOPS_HOST_BUILD)
static void *allocate_rwx_pool(size_t size) {
    void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? NULL : ptr;
}

static int set_memory_rwx(void *addr, size_t len) {
    uintptr_t start = (uintptr_t)addr & ~((uintptr_t)PAGE_SIZE - 1u);
    uintptr_t end =
        ((uintptr_t)addr + len + PAGE_SIZE - 1u) & ~((uintptr_t)PAGE_SIZE - 1u);
    size_t size = end - start;
    return mprotect((void *)start, size, PROT_READ | PROT_WRITE | PROT_EXEC);
}
#else
static void *allocate_rwx_pool(size_t size) {
    /* SYS_mmap 477, PROT_READ|PROT_WRITE|PROT_EXEC = 7, MAP_PRIVATE|MAP_ANON = 0x1002
     */
    long ret = sys_call(SYS_mmap, 0, (long)size, 7, 0x1002, -1, 0);
    if (ret <= 0 || (unsigned long)ret >= 0xFFFFFFFFFFFFF000ULL) {
        return NULL;
    }
    return (void *)(uintptr_t)ret;
}

static int set_memory_rwx(void *addr, size_t len) {
    uintptr_t start = (uintptr_t)addr & ~((uintptr_t)PAGE_SIZE - 1u);
    uintptr_t end =
        ((uintptr_t)addr + len + PAGE_SIZE - 1u) & ~((uintptr_t)PAGE_SIZE - 1u);
    size_t size = end - start;
    long ret = sys_call(SYS_mprotect, (long)start, (long)size, 7, 0, 0, 0);
    return (ret == 0) ? 0 : -1;
}
#endif

static void flush_icache(const void *addr, size_t len) {
#if defined(__x86_64__)
    const char *p = (const char *)addr;
    for (size_t i = 0; i < len; i += 64) {
        __builtin_ia32_clflush(p + i);
    }
    if (len > 0) {
        __builtin_ia32_clflush(p + len - 1);
    }
    __asm__ volatile("mfence" ::: "memory");
#else
    (void)addr;
    (void)len;
#endif
}

int tracer_hook_subsystem_init(void) {
    if (s_trampoline_pool != NULL) {
        return 0;
    }
    s_trampoline_pool = (uint8_t *)allocate_rwx_pool(PAGE_SIZE * TRAMPOLINE_POOL_PAGES);
    if (s_trampoline_pool == NULL) {
        return -1;
    }
    s_trampoline_used = 0;
    return 0;
}

static void *alloc_trampoline_slot(void) {
    if (s_trampoline_pool == NULL && tracer_hook_subsystem_init() != 0) {
        return NULL;
    }
    if (s_trampoline_used + TRAMPOLINE_SLOT_SIZE > PAGE_SIZE * TRAMPOLINE_POOL_PAGES) {
        return NULL;
    }
    void *slot = s_trampoline_pool + s_trampoline_used;
    s_trampoline_used += TRAMPOLINE_SLOT_SIZE;
    return slot;
}

/* JMP [RIP+0]: ff 25 00 00 00 00 <64-bit absolute target> */
static void emit_abs_jump(uint8_t *dst, void *target) {
    dst[0] = 0xff;
    dst[1] = 0x25;
    dst[2] = 0x00;
    dst[3] = 0x00;
    dst[4] = 0x00;
    dst[5] = 0x00;
    uint64_t addr = (uint64_t)(uintptr_t)target;
    for (size_t i = 0; i < 8; i++) {
        dst[6 + i] = (uint8_t)((addr >> (i * 8u)) & 0xFFu);
    }
}

int tracer_hook_install(tracer_hook_t *hook, void *target_fn, void *hook_fn,
                        size_t patch_size) {
    if (hook == NULL || target_fn == NULL || hook_fn == NULL) {
        return -1;
    }
    if (patch_size < TRACER_DETOUR_SIZE) {
        patch_size = TRACER_DETOUR_SIZE;
    }
    if (patch_size > sizeof(hook->orig_bytes)) {
        patch_size = sizeof(hook->orig_bytes);
    }

    uint8_t *trampoline = (uint8_t *)alloc_trampoline_slot();
    if (trampoline == NULL) {
        return -3;
    }

    /* Save original bytes */
    const uint8_t *src = (const uint8_t *)target_fn;
    for (size_t i = 0; i < patch_size; i++) {
        hook->orig_bytes[i] = src[i];
    }
    hook->target_fn = target_fn;
    hook->hook_fn = hook_fn;
    hook->patch_size = patch_size;

    /* Build trampoline: original instructions + jump to (target_fn + patch_size) */
    for (size_t i = 0; i < patch_size; i++) {
        trampoline[i] = hook->orig_bytes[i];
    }
    emit_abs_jump(trampoline + patch_size, (void *)((uintptr_t)target_fn + patch_size));
    flush_icache(trampoline, patch_size + TRACER_DETOUR_SIZE);
    hook->trampoline = trampoline;

    /* Make target function page writable */
    int prot_rc = set_memory_rwx(target_fn, patch_size);
    if (prot_rc != 0) {
#if !defined(OOPS_HOST_BUILD)
        /* Fallback: write through kernel DMAP mapping */
        uint8_t detour[TRACER_DETOUR_SIZE];
        emit_abs_jump(detour, hook_fn);
        for (size_t i = 0; i < TRACER_DETOUR_SIZE; i++) {
            if (krw_write8((uintptr_t)target_fn + i, detour[i]) != 0) {
                return -4;
            }
        }
        flush_icache(target_fn, TRACER_DETOUR_SIZE);
        hook->installed = 1;
        return 0;
#else
        return -4;
#endif
    }

    /* Overwrite target function with detour jump */
    uint8_t *dst = (uint8_t *)target_fn;
    emit_abs_jump(dst, hook_fn);
    for (size_t i = TRACER_DETOUR_SIZE; i < patch_size; i++) {
        dst[i] = 0x90; /* nop */
    }

    flush_icache(target_fn, patch_size);
    hook->installed = 1;
    return 0;
}

int tracer_hook_uninstall(tracer_hook_t *hook) {
    if (hook == NULL || !hook->installed || hook->target_fn == NULL) {
        return -1;
    }
    if (set_memory_rwx(hook->target_fn, hook->patch_size) != 0) {
#if !defined(OOPS_HOST_BUILD)
        for (size_t i = 0; i < hook->patch_size; i++) {
            (void)krw_write8((uintptr_t)hook->target_fn + i, hook->orig_bytes[i]);
        }
        flush_icache(hook->target_fn, hook->patch_size);
        hook->installed = 0;
        return 0;
#else
        return -1;
#endif
    }
    uint8_t *dst = (uint8_t *)hook->target_fn;
    for (size_t i = 0; i < hook->patch_size; i++) {
        dst[i] = hook->orig_bytes[i];
    }
    flush_icache(hook->target_fn, hook->patch_size);
    hook->installed = 0;
    return 0;
}
