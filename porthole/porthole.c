/*
 * Porthole - the target-side payload, scaffolded.
 *
 * This is the shape of the on-console payload from prosperous/docs/VIDEO.md "Part
 * three: Porthole", laid out so that filling it in is an afternoon rather than a
 * fortnight of deciding.
 * **Almost nothing here is implemented on purpose.** The three things Porthole actually
 * does - reach the encoder, capture-and-encode a frame, apply a pad - are stubs that
 * report PORTHOLE_UNIMPLEMENTED, each with a note on what it will do and where its
 * ground truth comes from. The one exception is porthole_pad_decode, which is the wire
 * contract and is real, because a contract nobody can test is not a contract.
 *
 * The single question the whole payload rests on is porthole_encoder_open below: **can
 * an unsigned payload reach the hardware encoder?** That is not answered by guessing
 * here - it is answered by the obSCEne probes (src/sections/record.c and the
 * encoder-reachability section), and this stub returns PORTHOLE_NO_ENCODER until they
 * say otherwise. If they say it cannot, Porthole is not built and the fallback is the
 * raw frame-grabber (VIDEO.md part two).
 *
 * Freestanding, and it stays that way: no libc, no allocation, no float, no variadics.
 * Platform calls are weak externs guarded before use, exactly as the probe's are, so a
 * loader that does not resolve one gets a clean status rather than a jump to zero.
 */
#if defined(PORTHOLE_HOST_BUILD)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif
#include "porthole.h"

/* The sockets, from the SDK, on both sides of the build. Outside the target-only block below
 * because the network section near the bottom is now one code path rather than two. */
#include "oops/net.h"

#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
/* The host build's only remaining platform call is the frame-interval wait. */
#include <time.h>
#endif

/* ---- the wire contract, which is real
 * ------------------------------------------------------ */

porthole_status porthole_pad_decode(const uint8_t bytes[PORTHOLE_PAD_BYTES],
                                    porthole_pad *out) {
    if (bytes[0] != PORTHOLE_PAD_MAGIC0 || bytes[1] != PORTHOLE_PAD_MAGIC1 ||
        bytes[2] != PORTHOLE_PAD_MAGIC2 || bytes[3] != PORTHOLE_PAD_MAGIC3) {
        return PORTHOLE_BAD_RECORD;
    }
    /* Assembled a byte at a time rather than read through a cast: the buffer is off the
     * wire with no alignment guarantee, and a misaligned load is undefined even where
     * it happens to work. */
    for (unsigned int i = 0; i < 4; i++) {
        out->magic[i] = bytes[i];
    }
    out->version = (uint16_t)((uint16_t)bytes[4] | (uint16_t)((uint16_t)bytes[5] << 8));
    out->slot = bytes[6];
    out->reserved0 = bytes[7];
    out->buttons = (uint32_t)bytes[8] | ((uint32_t)bytes[9] << 8) |
                   ((uint32_t)bytes[10] << 16) | ((uint32_t)bytes[11] << 24);
    for (unsigned int i = 0; i < 4; i++) {
        out->sticks[i] = bytes[12 + i];
    }
    out->triggers[0] = bytes[16];
    out->triggers[1] = bytes[17];
    out->reserved1 =
        (uint16_t)((uint16_t)bytes[18] | (uint16_t)((uint16_t)bytes[19] << 8));
    out->sequence = (uint32_t)bytes[20] | ((uint32_t)bytes[21] << 8) |
                    ((uint32_t)bytes[22] << 16) | ((uint32_t)bytes[23] << 24);

    /* A record that fails its own invariants is a sender believing something untrue;
     * acting on it puts one person's input on another's pad, or presses a button nobody
     * pressed. Refused, not clamped. The reserved bytes are checked so a version bump
     * into that room is a clean upgrade rather than a silent reinterpretation of old
     * fields. */
    if (out->version != PORTHOLE_PAD_VERSION || out->slot > PORTHOLE_PAD_MAX_SLOT ||
        out->reserved0 != 0 || out->reserved1 != 0) {
        return PORTHOLE_BAD_RECORD;
    }
    return PORTHOLE_OK;
}

#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
#include "oops/freestd.h"
#include "oops/krw.h"
#include "oops/syscall.h"
#include "oops/display.h"
#include "oops/memory.h"
#include "oops/time.h"
#include "agc/display.h"
#include "agc/tiler.h"

#define SCE_SYSMODULE_VENC 0x00A0u

#ifndef OBS_WEAK
#define OBS_WEAK __attribute__((weak))
#endif

OBS_WEAK int sceSysmoduleLoadModule(uint16_t id);
OBS_WEAK int sceKernelDlsym(int handle, const char *symbol, void **address_out);
/* No libSceNet declarations. The export table an elfldr payload is handed carries none of it -
 * measured, see the socket layer below - so the sockets are the platform's POSIX ones and are
 * resolved by name rather than linked against. */
#endif

static const void *s_porthole_args = NULL;
static porthole_encoder_api s_encoder_api;
static int s_encoder_opened = 0;
static porthole_encoder_session s_encoder_session = {
    .handle = -1,
    .work_mem = NULL,
    .work_mem_phys = 0,
    .work_mem_size = 0,
    .session_active = 0
};

void porthole_set_payload_args(const void *args) {
    s_porthole_args = args;
}

const porthole_encoder_api *porthole_encoder_get_api(void) {
    if (!s_encoder_opened) {
        return NULL;
    }
    return &s_encoder_api;
}

const porthole_encoder_session *porthole_encoder_get_session(void) {
    return &s_encoder_session;
}

int porthole_encoder_is_active(void) {
    return s_encoder_session.session_active;
}

int porthole_encoder_session_enabled(void) {
#if defined(PORTHOLE_ENCODER_SESSION)
    return 1;
#else
    return 0;
#endif
}

void porthole_encoder_config_default(porthole_encoder_config *cfg) {
    if (cfg == NULL) return;
    for (size_t i = 0; i < sizeof(porthole_encoder_config); i++) {
        ((uint8_t *)cfg)[i] = 0;
    }
    cfg->size = (uint32_t)sizeof(porthole_encoder_config);
    cfg->codec = (uint32_t)PORTHOLE_CODEC_AVC;
    cfg->width = 1920;
    cfg->height = 1080;
    cfg->fps_num = 60;
    cfg->fps_den = 1;
    cfg->bitrate = PORTHOLE_DEFAULT_BITRATE; /* 10 Mbps; PORTHOLE_FRAME_BYTES follows it */
    cfg->profile = (uint32_t)PORTHOLE_PROFILE_AVC_HIGH;
    cfg->level = 42;
    cfg->rc_mode = (uint32_t)PORTHOLE_RC_CBR;
    cfg->gop_size = 60;
}

porthole_status porthole_encoder_config_validate(const porthole_encoder_config *cfg) {
    if (cfg == NULL) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->size != (uint32_t)sizeof(porthole_encoder_config)) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->codec != (uint32_t)PORTHOLE_CODEC_AVC && cfg->codec != (uint32_t)PORTHOLE_CODEC_HEVC) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->width == 0 || cfg->width > 3840 || (cfg->width & 1) != 0) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->height == 0 || cfg->height > 2160 || (cfg->height & 1) != 0) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->fps_num == 0 || cfg->fps_den == 0) {
        return PORTHOLE_BAD_RECORD;
    }
    if (cfg->bitrate == 0 || cfg->bitrate > 100000000) {
        return PORTHOLE_BAD_RECORD;
    }
    return PORTHOLE_OK;
}

#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
porthole_status porthole_encoder_open(void) {
    /* No hardware encoder on host. */
    return PORTHOLE_NO_ENCODER;
}

porthole_status porthole_encoder_query_memory(const porthole_encoder_config *cfg, size_t *out_size) {
    if (out_size != NULL) *out_size = 0;
    if (cfg == NULL || porthole_encoder_config_validate(cfg) != PORTHOLE_OK) {
        return PORTHOLE_BAD_RECORD;
    }
    return PORTHOLE_NO_ENCODER;
}

porthole_status porthole_encoder_session_create(const porthole_encoder_config *cfg) {
    if (cfg == NULL || porthole_encoder_config_validate(cfg) != PORTHOLE_OK) {
        return PORTHOLE_BAD_RECORD;
    }
    return PORTHOLE_NO_ENCODER;
}

porthole_status porthole_encoder_session_destroy(void) {
    s_encoder_session.handle = -1;
    s_encoder_session.work_mem = NULL;
    s_encoder_session.work_mem_phys = 0;
    s_encoder_session.work_mem_size = 0;
    s_encoder_session.session_active = 0;
    return PORTHOLE_OK;
}
#else

static void *porthole_resolve_symbol(pid_t pid, const char *name) {
    if (name == NULL) {
        return NULL;
    }
    char nid[12];
    obs_compute_nid(name, nid);

    /* 1. Try kernel dispatch table walk (D277/D278) */
    if (krw_is_ready()) {
        uintptr_t addr = krw_dynlib_resolve_any(pid, name);
        if (addr >= 0x10000UL) {
            return (void *)addr;
        }
    }

    /* 2. Try pre-dumped kexport_table if available in payload_args */
    if (s_porthole_args != NULL) {
        const payload_args_t *pargs = (const payload_args_t *)s_porthole_args;
        if (pargs->kexport_table != NULL) {
            const void *addr = obs_kexport_lookup(
                (const obs_kexport_table_t *)pargs->kexport_table, nid);
            if (addr != NULL && (uintptr_t)addr >= 0x10000UL) {
                return (void *)addr;
            }
        }
    }

    /* 3. Try sceKernelDlsym if available */
    if (&sceKernelDlsym != NULL) {
        void *addr = NULL;
        if (sceKernelDlsym(0x2001, nid, &addr) == 0 && (uintptr_t)addr >= 0x10000UL) {
            return addr;
        }
        if (sceKernelDlsym(0x2001, name, &addr) == 0 && (uintptr_t)addr >= 0x10000UL) {
            return addr;
        }
        if (sceKernelDlsym(0x2, nid, &addr) == 0 && (uintptr_t)addr >= 0x10000UL) {
            return addr;
        }
        if (sceKernelDlsym(0x2, name, &addr) == 0 && (uintptr_t)addr >= 0x10000UL) {
            return addr;
        }
    }

    return NULL;
}

porthole_status porthole_encoder_query_memory(const porthole_encoder_config *cfg, size_t *out_size) {
    if (out_size == NULL) {
        return PORTHOLE_BAD_RECORD;
    }
    *out_size = 0;

    porthole_status vrc = porthole_encoder_config_validate(cfg);
    if (vrc != PORTHOLE_OK) {
        return vrc;
    }

    if (!s_encoder_opened || s_encoder_api.query_memory_size == NULL) {
        return PORTHOLE_NO_ENCODER;
    }

#if !defined(PORTHOLE_ENCODER_SESSION)
    /* Gated off: the call below passes a parameter layout not yet confirmed (D300, M2).
     * See porthole_encoder_session_enabled in porthole.h. */
    return PORTHOLE_UNIMPLEMENTED;
#else
    /* Out parameter poisoning (D303): initialized to poisoned value to detect honest assignment */
    size_t mem_needed = (size_t)0xDEADBEEFULL;
    int (*fn_query)(const void *, size_t *) =
        (int (*)(const void *, size_t *))s_encoder_api.query_memory_size;
    int rc = fn_query(cfg, &mem_needed);
    if (rc != 0 || mem_needed == 0 || mem_needed == (size_t)0xDEADBEEFULL) {
        /* Fallback: default working buffer 32 MB if platform query refused */
        *out_size = 0x2000000u;
    } else {
        *out_size = mem_needed;
    }
    return PORTHOLE_OK;
#endif
}

porthole_status porthole_encoder_session_create(const porthole_encoder_config *cfg) {
    if (s_encoder_session.session_active) {
        return PORTHOLE_OK;
    }

    porthole_status vrc = porthole_encoder_config_validate(cfg);
    if (vrc != PORTHOLE_OK) {
        return vrc;
    }

    if (!s_encoder_opened || s_encoder_api.create_encoder == NULL) {
        return PORTHOLE_NO_ENCODER;
    }

#if !defined(PORTHOLE_ENCODER_SESSION)
    /* Gated off: the calls below pass parameter layouts not yet confirmed (D300, M2).
     * See porthole_encoder_session_enabled in porthole.h. */
    return PORTHOLE_UNIMPLEMENTED;
#else
    size_t required_mem = 0;
    porthole_status qrc = porthole_encoder_query_memory(cfg, &required_mem);
    if (qrc != PORTHOLE_OK) {
        return qrc;
    }

    /* Align required_mem to 2MB large page boundary */
    size_t page_align = 0x200000u;
    required_mem = (required_mem + page_align - 1) & ~(page_align - 1);
    s_encoder_session.work_mem_size = required_mem;

    /* Allocate direct memory GPU write-combined working buffer */
    int64_t phys = 0;
    int arc = oops_mem_alloc_direct(required_mem, page_align, OOPS_MEM_WC_GARLIC, &phys);
    if (arc != 0 || phys <= 0) {
        return PORTHOLE_NO_ENCODER;
    }
    s_encoder_session.work_mem_phys = phys;

    /* Map working buffer with CPU and GPU read/write permissions */
    void *vaddr = (void *)0x4200000000ULL;
    int mrc = oops_mem_batch_map(vaddr, phys, required_mem, page_align,
                                 OOPS_PROT_CPU_RW | OOPS_PROT_GPU_RW);
    if (mrc != 0) {
        void *mapped_vaddr = NULL;
        if (oops_mem_map_direct(&mapped_vaddr, required_mem,
                                OOPS_PROT_CPU_RW | OOPS_PROT_GPU_RW,
                                0, phys, page_align) == 0 && mapped_vaddr != NULL) {
            vaddr = mapped_vaddr;
        } else {
            oops_mem_free_direct(phys, required_mem);
            s_encoder_session.work_mem_phys = 0;
            return PORTHOLE_NO_ENCODER;
        }
    }
    s_encoder_session.work_mem = vaddr;
    s_encoder_session.config = *cfg;

    /* Create encoder instance: poisoned handle */
    int handle = (int)0xDEADBEEF;
    int (*fn_create)(int *, const void *, void *, size_t) =
        (int (*)(int *, const void *, void *, size_t))s_encoder_api.create_encoder;

    int crc = fn_create(&handle, cfg, s_encoder_session.work_mem, s_encoder_session.work_mem_size);
    if (crc != 0 || handle == (int)0xDEADBEEF || handle < 0) {
        /* Cleanup allocated memory on creation failure */
        oops_mem_unmap(s_encoder_session.work_mem, s_encoder_session.work_mem_size);
        oops_mem_free_direct(s_encoder_session.work_mem_phys, s_encoder_session.work_mem_size);
        s_encoder_session.work_mem = NULL;
        s_encoder_session.work_mem_phys = 0;
        return PORTHOLE_NO_ENCODER;
    }

    s_encoder_session.handle = handle;

    /* Start sequence if start_sequence is available */
    if (s_encoder_api.start_sequence != NULL) {
        int (*fn_start)(int) = (int (*)(int))s_encoder_api.start_sequence;
        (void)fn_start(s_encoder_session.handle);
    }

    s_encoder_session.session_active = 1;
    return PORTHOLE_OK;
#endif
}

porthole_status porthole_encoder_session_destroy(void) {
    if (!s_encoder_session.session_active) {
        return PORTHOLE_OK;
    }

    if (s_encoder_api.stop_sequence != NULL && s_encoder_session.handle >= 0) {
        int (*fn_stop)(int) = (int (*)(int))s_encoder_api.stop_sequence;
        (void)fn_stop(s_encoder_session.handle);
    }

    if (s_encoder_api.delete_encoder != NULL && s_encoder_session.handle >= 0) {
        int (*fn_del)(int) = (int (*)(int))s_encoder_api.delete_encoder;
        (void)fn_del(s_encoder_session.handle);
    }

    if (s_encoder_session.work_mem != NULL && s_encoder_session.work_mem_size > 0) {
        oops_mem_unmap(s_encoder_session.work_mem, s_encoder_session.work_mem_size);
    }
    if (s_encoder_session.work_mem_phys > 0 && s_encoder_session.work_mem_size > 0) {
        oops_mem_free_direct(s_encoder_session.work_mem_phys, s_encoder_session.work_mem_size);
    }

    s_encoder_session.handle = -1;
    s_encoder_session.work_mem = NULL;
    s_encoder_session.work_mem_phys = 0;
    s_encoder_session.work_mem_size = 0;
    s_encoder_session.session_active = 0;
    return PORTHOLE_OK;
}

porthole_status porthole_encoder_open(void) {
    if (s_encoder_opened) {
        return PORTHOLE_OK;
    }

#if !defined(PORTHOLE_ENCODER_SESSION)
    /* **The load itself is refused, and refused loudly.** obSCEne resolved
     * `sceSysmoduleLoadModule` from the payload's own export table at 0x8002740d0 and called it
     * with VENC (0x00A0) from an unsigned elfldr payload: the kernel answered 0xa0020101, a
     * privilege refusal raised as a signal rather than returned as a code
     * (REQ-20260909T0840Z-2d17). All seven encoder entry points stayed null afterwards, by every
     * route.
     *
     * So a gated build does not make the call at all. This is not caution about a value that
     * might be wrong - it is a measured refusal, and a payload that trips it risks dying at
     * startup before it has opened a socket, which is precisely what the gate exists to
     * prevent. */
    klog_write("encoder not attempted: the sysmodule load is refused for unsigned payloads "
               "(measured 0xa0020101); build with PORTHOLE_ENCODER_SESSION to try anyway");
    return PORTHOLE_NO_ENCODER;
#else
    pid_t mypid = 0;
#if defined(SYS_getpid)
    mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
#endif

    /* Initialize kernel R/W if payload args were supplied and krw is not yet initialized */
    if (!krw_is_ready() && s_porthole_args != NULL) {
        const payload_args_t *pargs = (const payload_args_t *)s_porthole_args;
        (void)krw_init(pargs);
    }

    /* Step 1: Resolve sceSysmoduleLoadModule */
    int (*fn_sysmodule_load)(uint16_t) = NULL;
    if (&sceSysmoduleLoadModule != NULL) {
        fn_sysmodule_load = &sceSysmoduleLoadModule;
    } else {
        fn_sysmodule_load =
            (int (*)(uint16_t))porthole_resolve_symbol(mypid, "sceSysmoduleLoadModule");
    }

    if (fn_sysmodule_load == NULL) {
        return PORTHOLE_NO_ENCODER;
    }

    /* Step 2: Load the hardware video encoder sysmodule (VENC = 0x00A0) */
    int rc = fn_sysmodule_load(SCE_SYSMODULE_VENC);
    /* 0x0 = success, 0x80540001 = SCE_SYSMODULE_LOADED (already loaded) */
    if (rc != 0 && rc != (int)0x80540001) {
        return PORTHOLE_NO_ENCODER;
    }

    /* Step 3: Self-resolve libSceVencCore symbols from loaded module's export table (D277) */
    s_encoder_api.query_memory_size =
        porthole_resolve_symbol(mypid, "sceVencCoreQueryMemorySize");
    s_encoder_api.create_encoder =
        porthole_resolve_symbol(mypid, "sceVencCoreCreateEncoder");
    s_encoder_api.delete_encoder =
        porthole_resolve_symbol(mypid, "sceVencCoreDeleteEncoder");
    s_encoder_api.get_au_data =
        porthole_resolve_symbol(mypid, "sceVencCoreGetAuData");
    s_encoder_api.set_input_frame =
        porthole_resolve_symbol(mypid, "sceVencCoreSetInputFrame");
    s_encoder_api.start_sequence =
        porthole_resolve_symbol(mypid, "sceVencCoreStartSequence");
    s_encoder_api.stop_sequence =
        porthole_resolve_symbol(mypid, "sceVencCoreStopSequence");
    s_encoder_api.sync_encode =
        porthole_resolve_symbol(mypid, "sceVencCoreSyncEncode");

    /* Essential check: core encoder entry points must resolve to callable addresses */
    if (s_encoder_api.query_memory_size == NULL ||
        s_encoder_api.create_encoder == NULL ||
        s_encoder_api.get_au_data == NULL) {
        return PORTHOLE_NO_ENCODER;
    }

    s_encoder_opened = 1;

    /* Milestone M2: bring up an encoder session with the default configuration. Only a gated-on
     * build reaches this line at all now - see the refusal at the top of this function. */
    porthole_encoder_config cfg;
    porthole_encoder_config_default(&cfg);
    (void)porthole_encoder_session_create(&cfg);

    return PORTHOLE_OK;
#endif
}
#endif

/* ---- Controller Input Subsystem (Ghostpad VDI) ---------------------------- */

typedef struct porthole_slot_state {
    int active;
    int handle;
    uint32_t last_sequence;
} porthole_slot_state;

static porthole_slot_state s_slots[PORTHOLE_PAD_MAX_SLOT + 1];
static porthole_pad_api s_pad_api;
static int s_pad_opened = 0;
static volatile int s_porthole_running = 1;

void porthole_stop(void) {
    s_porthole_running = 0;
}

const porthole_pad_api *porthole_pad_get_api(void) {
    if (!s_pad_opened) {
        return NULL;
    }
    return &s_pad_api;
}

void porthole_pad_reset(void) {
    for (size_t i = 0; i <= PORTHOLE_PAD_MAX_SLOT; i++) {
        s_slots[i].active = 0;
        s_slots[i].handle = -1;
        s_slots[i].last_sequence = 0;
    }
}

void porthole_pad_resequence(void) {
    /* Only the count. A slot's virtual device stays where it is, so a host that reconnects
     * drives the pad it was driving rather than a second one added beside it. */
    for (size_t i = 0; i <= PORTHOLE_PAD_MAX_SLOT; i++) {
        s_slots[i].last_sequence = 0;
    }
}

#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
porthole_status porthole_pad_open(void) {
    porthole_pad_reset();
    s_pad_opened = 1;
    return PORTHOLE_OK;
}
#else
porthole_status porthole_pad_open(void) {
    if (s_pad_opened) {
        return PORTHOLE_OK;
    }
    porthole_pad_reset();

    pid_t mypid = 0;
#if defined(SYS_getpid)
    mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
#endif

    if (!krw_is_ready() && s_porthole_args != NULL) {
        const payload_args_t *pargs = (const payload_args_t *)s_porthole_args;
        (void)krw_init(pargs);
    }

    s_pad_api.pad_init = porthole_resolve_symbol(mypid, "scePadInit");
    s_pad_api.virtual_device_add =
        porthole_resolve_symbol(mypid, "scePadVirtualDeviceAddDevice");
    s_pad_api.virtual_device_delete =
        porthole_resolve_symbol(mypid, "scePadVirtualDeviceDeleteDevice");
    s_pad_api.virtual_device_insert =
        porthole_resolve_symbol(mypid, "scePadVirtualDeviceInsertData");

    if (s_pad_api.pad_init != NULL) {
        int (*fn_pad_init)(void) = (int (*)(void))s_pad_api.pad_init;
        (void)fn_pad_init();
    }

    /* Marked opened either way, so a record arriving sixty times a second does not send this
     * back through resolution on every one of them. */
    s_pad_opened = 1;

    /* **Tried is not the same as reached.** None of these four is in the export table a payload
     * is handed - checked against sweep 20260909-083918 - so this normally comes back with four
     * nulls, and returning OK for that would put a success in the caller's hands that the
     * addresses underneath it contradict. Injection needs the add and the insert; without
     * either there is no input path at all, whatever the other two did. */
    if (s_pad_api.virtual_device_add == NULL || s_pad_api.virtual_device_insert == NULL) {
        return PORTHOLE_NO_PAD;
    }
    return PORTHOLE_OK;
}
#endif

/*
 * Apply one decoded controller record to the target's pad state.
 *
 * Checks sequence freshness: input is state, not an event. A record no newer than the
 * last applied to its slot is not applied, and says so with PORTHOLE_STALE, so old
 * sticks and buttons never replay and a caller can tell "applied" from "superseded".
 * A slot's count starts over at porthole_pad_resequence - once per new input
 * connection, because a new connection is a new sender.
 */
porthole_status porthole_pad_apply(const porthole_pad *pad) {
    if (pad == NULL || pad->slot > PORTHOLE_PAD_MAX_SLOT) {
        return PORTHOLE_BAD_RECORD;
    }

    if (!s_pad_opened) {
        (void)porthole_pad_open();
    }

    porthole_slot_state *slot = &s_slots[pad->slot];

    /* Input freshness check: drop older or duplicate sequence numbers per slot */
    if (pad->sequence != 0 && slot->active && pad->sequence <= slot->last_sequence) {
        return PORTHOLE_STALE;
    }

    slot->last_sequence = pad->sequence;

#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
    slot->active = 1;
    return PORTHOLE_OK;
#else
    if (!slot->active) {
        if (s_pad_api.virtual_device_add != NULL) {
            int (*fn_add)(uint32_t) = (int (*)(uint32_t))s_pad_api.virtual_device_add;
            int h = fn_add(3); /* Type 3 = DualSense virtual device */
            slot->handle = h;
        } else {
            slot->handle = -1;
        }
        slot->active = 1;
    }

    if (slot->handle >= 0 && s_pad_api.virtual_device_insert != NULL) {
        /* Ghostpad pad packet format (16 bytes) matching target pad structure */
        struct {
            uint32_t buttons;
            uint8_t sticks[4];
            uint8_t triggers[2];
            uint16_t reserved;
            uint32_t timestamp;
        } __attribute__((packed)) gpad_pkt;

        gpad_pkt.buttons = pad->buttons;
        gpad_pkt.sticks[0] = pad->sticks[0];
        gpad_pkt.sticks[1] = pad->sticks[1];
        gpad_pkt.sticks[2] = pad->sticks[2];
        gpad_pkt.sticks[3] = pad->sticks[3];
        gpad_pkt.triggers[0] = pad->triggers[0];
        gpad_pkt.triggers[1] = pad->triggers[1];
        gpad_pkt.reserved = 0;
        gpad_pkt.timestamp = pad->sequence;

        int (*fn_insert)(int, const void *) =
            (int (*)(int, const void *))s_pad_api.virtual_device_insert;
        (void)fn_insert(slot->handle, &gpad_pkt);
    }

    return PORTHOLE_OK;
#endif
}

/* ---- Display Subsystem (reusing oops-sdk) --------------------------------- */

#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
static oops_display_t *s_porthole_disp = NULL;

porthole_status porthole_display_open(void) {
    if (s_porthole_disp != NULL && oops_display_is_ready(s_porthole_disp)) {
        return PORTHOLE_OK;
    }
    s_porthole_disp = oops_display_open(OOPS_DISPLAY_BACKEND_AUTO, 1920, 1080);
    if (s_porthole_disp == NULL || !oops_display_is_ready(s_porthole_disp)) {
        return PORTHOLE_NO_DISPLAY;
    }
    return PORTHOLE_OK;
}

uint32_t *porthole_display_get_framebuffer(void) {
    if (s_porthole_disp == NULL) {
        return NULL;
    }
    return oops_display_get_framebuffer(s_porthole_disp);
}

int porthole_display_flip(void) {
    if (s_porthole_disp == NULL) {
        return -1;
    }
    return oops_display_flip(s_porthole_disp);
}

void porthole_display_close(void) {
    if (s_porthole_disp != NULL) {
        oops_display_close(s_porthole_disp);
        s_porthole_disp = NULL;
    }
}
#else
static uint32_t s_host_fb[1920 * 1080];
static int s_host_disp_opened = 0;

porthole_status porthole_display_open(void) {
    s_host_disp_opened = 1;
    return PORTHOLE_OK;
}

uint32_t *porthole_display_get_framebuffer(void) {
    return s_host_disp_opened ? s_host_fb : NULL;
}

int porthole_display_flip(void) {
    return s_host_disp_opened ? 0 : -1;
}

void porthole_display_close(void) {
    s_host_disp_opened = 0;
}
#endif

/*
 * Draw SMPTE color bars and a running progress tick into the 1920x1080 framebuffer.
 */
void porthole_display_draw_test_pattern(uint32_t frame_index) {
    uint32_t *fb = porthole_display_get_framebuffer();
    if (fb == NULL) return;

    static const uint32_t s_colors[8] = {
        0xFFFFFFFFu, /* White */
        0xFFFFFF00u, /* Yellow */
        0xFF00FFFFu, /* Cyan */
        0xFF00FF00u, /* Green */
        0xFFFF00FFu, /* Magenta */
        0xFFFF0000u, /* Red */
        0xFF0000FFu, /* Blue */
        0xFF000000u  /* Black */
    };

    /* Top 960 rows: 8 equal-width color bars (240 px each) */
    for (uint32_t y = 0; y < 960; y++) {
        uint32_t *row = fb + (size_t)y * 1920u;
        for (uint32_t bar = 0; bar < 8; bar++) {
            uint32_t color = s_colors[bar];
            for (uint32_t x = 0; x < 240; x++) {
                row[bar * 240 + x] = color;
            }
        }
    }

    /* Bottom 120 rows: dark gray baseline with running activity marker */
    uint32_t marker_pos = (frame_index * 8u) % 1920u;
    for (uint32_t y = 960; y < 1080; y++) {
        uint32_t *row = fb + (size_t)y * 1920u;
        for (uint32_t x = 0; x < 1920; x++) {
            if (x >= marker_pos && x < marker_pos + 40u) {
                row[x] = 0xFFFFFFFFu;
            } else {
                row[x] = 0xFF202020u;
            }
        }
    }
}

/* ---- Encoder Session & Frame Capture (M2 & M3) ---------------------------- */

static uint32_t s_encoded_frames = 0;

/* Where one access unit is built, sized by PORTHOLE_FRAME_BYTES. File scope because a
 * payload's stack will not hold it, and because porthole_run is its only caller and is not
 * reentrant - the payload serves one video connection at a time. */
static uint8_t s_frame_buf[PORTHOLE_FRAME_BYTES];

/* Standard Annex-B H.264 NAL templates for 1080p60 stream */
static const uint8_t s_sps_nal[14] = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xC0, 0x28, 0xD9, 0x00, 0x78, 0x02, 0x27, 0xE2
};
static const uint8_t s_pps_nal[8] = {
    0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80
};
static const uint8_t s_idr_nal[16] = {
    0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00
};
static const uint8_t s_slice_nal[16] = {
    0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x84, 0x00, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00
};

/*
 * Capture the composited output and hand back one encoded access unit (Annex-B H.264).
 */
porthole_status porthole_capture_encode(uint8_t *out, size_t cap, size_t *len) {
    if (out == NULL || len == NULL) {
        return PORTHOLE_BAD_RECORD;
    }
    *len = 0;
#if defined(PORTHOLE_ENCODER_SESSION)
    /* A build asked for real video has nothing to give without the encoder. Gated off, the
     * template stream below needs none - and refusing here was what stopped a gated payload
     * serving anything at all. See porthole_encoder_session_enabled in porthole.h. */
    if (!s_encoder_opened) {
        return PORTHOLE_NO_ENCODER;
    }
#endif

#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__)) && \
    defined(PORTHOLE_ENCODER_SESSION)
    /* Milestone M3: feed the input frame and take the encoded AU from the active session.
     * Compiled in only when the gate is on - both calls pass layouts not yet confirmed, and
     * the second writes into a stack buffer. Without it the template stream below serves. */
    if (s_encoder_session.session_active && s_encoder_session.handle >= 0 &&
        s_encoder_api.get_au_data != NULL) {
        if (s_encoder_api.set_input_frame != NULL) {
            uint32_t *fb = porthole_display_get_framebuffer();
            if (fb != NULL) {
                ((void (*)(int, const void *))s_encoder_api.set_input_frame)(s_encoder_session.handle, fb);
            }
        }

        int (*gfn)(int, void *, size_t, size_t *) =
            (int (*)(int, void *, size_t, size_t *))s_encoder_api.get_au_data;
        size_t au_len = (size_t)0xDEADBEEFULL;
        if (gfn(s_encoder_session.handle, out, cap, &au_len) == 0 && au_len > 0 &&
            au_len != (size_t)0xDEADBEEFULL) {
            if (au_len <= cap) {
                *len = au_len;
                s_encoded_frames++;
                return PORTHOLE_OK;
            }
            /* **Too large must not look like working video.** Falling through to the template
             * puts a test pattern on the wire, and a host watching that sees a stream which
             * frames and keyframes perfectly well, so nothing downstream can tell. Said once
             * rather than per frame, because this runs sixty times a second. */
            static int said = 0;
            if (!said) {
                said = 1;
                klog_write_hex("access unit too large for the frame buffer, bytes: ",
                               (uint64_t)au_len);
                klog_write_hex("  the buffer holds: ", (uint64_t)cap);
            }
        }
    }
#endif

    /* Emit Annex-B H.264 stream packet sequence (SPS/PPS/IDR on keyframe, P-slice on intra) */
    size_t total = 0;
    int is_keyframe = (s_encoded_frames % 60 == 0);

    if (is_keyframe) {
        if (cap < sizeof(s_sps_nal) + sizeof(s_pps_nal) + sizeof(s_idr_nal)) {
            return PORTHOLE_BAD_RECORD;
        }
        for (size_t i = 0; i < sizeof(s_sps_nal); i++) out[total++] = s_sps_nal[i];
        for (size_t i = 0; i < sizeof(s_pps_nal); i++) out[total++] = s_pps_nal[i];
        for (size_t i = 0; i < sizeof(s_idr_nal); i++) out[total++] = s_idr_nal[i];
    } else {
        if (cap < sizeof(s_slice_nal)) {
            return PORTHOLE_BAD_RECORD;
        }
        for (size_t i = 0; i < sizeof(s_slice_nal); i++) out[total++] = s_slice_nal[i];
    }

    *len = total;
    s_encoded_frames++;
    return PORTHOLE_OK;
}

/* ---- Networking & Dual-Socket Server -------------------------------------- */

/*
 * The sockets come from oops-sdk, and one code path serves both builds.
 *
 * **This was a private socket layer until 2026-09-09.** It had to be, because the SDK's was built
 * on `sceNet*` and a payload resolves none of that (obSCEne REQ-20260908T1400Z-0001). The SDK has
 * since been rebuilt on the POSIX exports a payload *can* reach, so the duplication goes: the
 * measurements Porthole gathered - the socket-creation route, the option that sets non-blocking
 * mode, the errno behaviour - now live in `oops/net.h` where the next payload gets them for free.
 *
 * Neither socket is waited on. The loop runs at the frame rate and, each time round, accepts
 * whoever has arrived, reads whatever input has arrived, and sends one frame - so a read or an
 * accept that waited would hold the frame with it. That is not hypothetical: the host goes quiet
 * on the input socket whenever a pad is at rest, so a receive that waited stalled the video every
 * time somebody let go of the keys, and an accept that waited meant no video at all until
 * something also connected for input.
 */

/* Set when a listener could not be made non-blocking, so porthole_run can say so. The loop still
 * runs, but each accept then waits, and video needs an input client connected too. */
static int s_net_accept_waits = 0;

static int porthole_set_blocking(int s, int blocking) {
    return oops_set_nonblocking(s, blocking ? 0 : 1);
}

/* Whether a negative return meant "nothing yet" rather than "broken". The SDK answers for both
 * worlds - errno through `__error()` on the POSIX path, and the `sceNet` encoding otherwise - so
 * Porthole no longer decides which it is looking at. */
static int porthole_would_block(long rc) {
    return oops_net_would_block(rc);
}

static int porthole_listen_port(uint16_t port) {
    int s = oops_socket(OOPS_AF_INET, OOPS_SOCK_STREAM, OOPS_IPPROTO_TCP);
    if (s < 0) {
        return -1;
    }
    /* A null address is INADDR_ANY: this listens on whatever interface reaches the host. */
    if (oops_bind(s, (const char *)0, port) < 0 || oops_listen(s, 1) < 0) {
        oops_close(s);
        return -1;
    }
    if (porthole_set_blocking(s, 0) < 0) {
        s_net_accept_waits = 1;
    }
    return s;
}

static int porthole_accept_conn(int listener) {
    return oops_accept(listener, (char *)0, 0, (uint16_t *)0);
}

static long porthole_recv_bytes(int conn, void *buf, size_t len) {
    /* Take only what has already arrived. The flag's value was measured on this console before it
     * had a name; it has one now, and it lives in the SDK where the next payload finds it. */
    return oops_recv(conn, buf, len, OOPS_MSG_DONTWAIT);
}

static long porthole_send_bytes(int conn, const void *buf, size_t len) {
    return oops_send(conn, buf, len, 0);
}

static void porthole_close_conn(int conn) {
    if (conn >= 0) {
        oops_close(conn);
    }
}

/* Wait, on whichever side of the build this is. One helper rather than the same guarded pair
 * written out at each of the two places the loop waits. */
static void porthole_sleep_ms(unsigned int ms) {
#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
    oops_time_sleep_ms(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    nanosleep(&ts, NULL);
#endif
}

/*
 * The payload's main server loop:
 * Opens listening sockets on PORTHOLE_PORT_VIDEO (9805) and PORTHOLE_PORT_INPUT (9806).
 * Serves video out and accepts input in, waiting on neither.
 */
porthole_status porthole_run(void) {
    /* The encoder is attempted, not required.
     *
     * With the session gated off the video path is the template stream, which needs no
     * encoder at all, so requiring one here meant the build designed to exercise the sockets
     * could never reach them. That is not hypothetical: obSCEne measured on 2026-09-08 that
     * an elfldr payload resolves neither the sysmodule loader nor any sceVencCore entry
     * point, so a payload that insisted on the encoder would exit before opening a port. */
    porthole_status enc_status = porthole_encoder_open();
    if (enc_status != PORTHOLE_OK) {
#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
        /* A host build has no target to serve and would bind real ports if it tried, so it
         * stops here rather than starting a server inside a test binary. */
        return enc_status;
#elif defined(PORTHOLE_ENCODER_SESSION)
        /* This build was asked for real video and has no way to produce any. */
        return enc_status;
#else
        klog_write_num("encoder unavailable, serving the template stream anyway, status: ",
                       (int64_t)enc_status);
#endif
    }

    (void)porthole_pad_open();

    /* **The display will not open in a payload, and the log says so rather than implying it did.**
     * obSCEne measured `oops_display_*` as `supported: 0x0` there - no GPU library is mapped in an
     * unsigned payload - and every route to the composited pixels came back null
     * (REQ-20260909T1021Z-af02). The call is still made, because a future route may change that
     * and the failure is harmless: the framebuffer stays null, the test pattern draws nothing and
     * the flip refuses. What must not happen is a payload that quietly draws into nowhere while
     * its log reads like a working capture path. */
    porthole_status disp_status = porthole_display_open();
#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
    if (disp_status != PORTHOLE_OK) {
        klog_write_num("display unavailable, so nothing is captured; the stream is the template "
                       "only. status: ",
                       (int64_t)disp_status);
    } else if (porthole_display_get_framebuffer() == NULL) {
        klog_write("display opened but handed back no framebuffer: nothing to capture");
    }
#else
    (void)disp_status;
#endif

    int s_video = porthole_listen_port(PORTHOLE_PORT_VIDEO);
    int s_input = porthole_listen_port(PORTHOLE_PORT_INPUT);

    if (s_video < 0 || s_input < 0) {
        porthole_close_conn(s_video);
        porthole_close_conn(s_input);
        return PORTHOLE_NET;
    }
#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
    if (s_net_accept_waits) {
        klog_write("listeners could not be made non-blocking: each accept waits, so video "
                   "needs an input client connected too");
    }
    /* Which route the descriptor came by is the SDK's business now, not this payload's - it owns
     * the choice between the export and the system call, and both were measured working. What is
     * still worth saying here is that two listeners exist at all, because everything downstream
     * depends on it and a first run has no other way to know. */
    klog_write_num("video listening on 9805, descriptor: ", (int64_t)s_video);
    klog_write_num("input listening on 9806, descriptor: ", (int64_t)s_input);
#endif

    s_porthole_running = 1;
    int conn_input = -1;
    int conn_video = -1;

    uint8_t input_buf[PORTHOLE_PAD_BYTES];
    size_t input_buf_len = 0;
    uint32_t frame_counter = 0;

    while (s_porthole_running) {
        /* Whoever has arrived. A watcher with no feed open, or a feed with no watcher, is
         * an ordinary state, and the half that is connected is served. */
        if (conn_input < 0) {
            conn_input = porthole_accept_conn(s_input);
            if (conn_input >= 0) {
                /* A new connection is a new sender, whose count starts over. Judged
                 * against the previous sender's last sequence, a host that restarted would
                 * have every record dropped as stale until its count climbed past - input
                 * that silently does nothing. */
                porthole_pad_resequence();
                input_buf_len = 0;
            }
        }
        if (conn_video < 0) {
            conn_video = porthole_accept_conn(s_video);
            if (conn_video >= 0) {
                /* Explicitly, because an accepted socket can inherit the listener's
                 * non-blocking mode, and the send below treats "try later" as gone. */
                (void)porthole_set_blocking(conn_video, 1);
            }
        }

        /* Everything that has arrived on the input socket, and no waiting for more. Reading
         * until the socket is empty also keeps a burst of records from queuing behind a
         * one-record-a-frame reader. */
        while (conn_input >= 0) {
            long n = porthole_recv_bytes(conn_input, input_buf + input_buf_len,
                                         PORTHOLE_PAD_BYTES - input_buf_len);
            if (n > 0) {
                input_buf_len += (size_t)n;
                if (input_buf_len == PORTHOLE_PAD_BYTES) {
                    porthole_pad pad;
                    if (porthole_pad_decode(input_buf, &pad) == PORTHOLE_OK) {
                        (void)porthole_pad_apply(&pad);
                    }
                    input_buf_len = 0;
                }
                continue;
            }
            if (n < 0 && porthole_would_block(n)) {
                break; /* nothing more this frame */
            }
#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
            if (n < 0) {
                klog_write_hex("input connection closed, receive returned: ",
                               (uint64_t)(uint32_t)n);
            }
#endif
            /* Closed by the host, or broken. Either way this connection is over, and the
             * listener takes the next. */
            porthole_close_conn(conn_input);
            conn_input = -1;
            input_buf_len = 0;
        }

        if (conn_video >= 0) {
            porthole_display_draw_test_pattern(frame_counter++);
            (void)porthole_display_flip();

            size_t frame_len = 0;
            porthole_status cap_rc =
                porthole_capture_encode(s_frame_buf, sizeof(s_frame_buf), &frame_len);
            if (cap_rc == PORTHOLE_OK && frame_len > 0) {
                /* **Until all of it has gone.** A send may take less than it was offered, and
                 * a short write drops the tail of an access unit. The host's reader
                 * resynchronises past the damage, so the loss would surface as a picture that
                 * stutters rather than as an error anywhere - the worst shape a fault can
                 * take. The template units are small enough that this cannot bite today; the
                 * real frames M3 brings are not.
                 *
                 * **A full buffer is waited out, not treated as a death.** An accepted socket
                 * inherits the listener's non-blocking mode here - measured, by reading the
                 * flags back off one (REQ-20260908T1528Z-b4a2) - so this connection is put
                 * back to blocking above. Should that ever fail, a full send buffer answers
                 * try-again, and closing on it would drop a working connection every time the
                 * network fell behind. Giving up mid-unit is not an option either: half an
                 * access unit on the wire is the corruption this loop exists to avoid. So it
                 * waits, which costs a millisecond and cannot lose the stream. */
                size_t off = 0;
                while (off < frame_len) {
                    long sent =
                        porthole_send_bytes(conn_video, s_frame_buf + off, frame_len - off);
                    if (sent > 0) {
                        off += (size_t)sent;
                        continue;
                    }
                    if (sent < 0 && porthole_would_block(sent)) {
                        porthole_sleep_ms(1);
                        continue;
                    }
                    porthole_close_conn(conn_video);
                    conn_video = -1;
                    break;
                }
            }
        }

        porthole_sleep_ms(16);
    }

    if (conn_input >= 0) porthole_close_conn(conn_input);
    if (conn_video >= 0) porthole_close_conn(conn_video);
    porthole_close_conn(s_input);
    porthole_close_conn(s_video);
    porthole_display_close();
    (void)porthole_encoder_session_destroy();

    return PORTHOLE_OK;
}
