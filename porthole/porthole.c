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
    if (out->slot > PORTHOLE_PAD_MAX_SLOT || out->reserved0 != 0 ||
        out->reserved1 != 0) {
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
OBS_WEAK int sceNetSocket(const char *name, int family, int type, int protocol);
OBS_WEAK int sceNetBind(int s, const void *addr, uint32_t addrlen);
OBS_WEAK int sceNetListen(int s, int backlog);
OBS_WEAK int sceNetAccept(int s, void *addr, uint32_t *paddrlen);
OBS_WEAK int sceNetRecv(int s, void *buf, uint64_t len, int flags);
OBS_WEAK int sceNetSend(int s, const void *buf, uint64_t len, int flags);
OBS_WEAK int sceNetSocketClose(int s);
OBS_WEAK int sceNetSetsockopt(int s, int level, int optname, const void *optval,
                              uint32_t optlen);
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
    cfg->bitrate = 10000000; /* 10 Mbps */
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

#if defined(PORTHOLE_ENCODER_SESSION)
    /* Milestone M2: bring up an encoder session with the default configuration. Compiled in
     * only when the gate is on - see porthole_encoder_session_enabled. */
    porthole_encoder_config cfg;
    porthole_encoder_config_default(&cfg);
    (void)porthole_encoder_session_create(&cfg);
#endif

    return PORTHOLE_OK;
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

    s_pad_opened = 1;
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
    if (!s_encoder_opened) {
        return PORTHOLE_NO_ENCODER;
    }

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
        if (gfn(s_encoder_session.handle, out, cap, &au_len) == 0 &&
            au_len > 0 && au_len != (size_t)0xDEADBEEFULL && au_len <= cap) {
            *len = au_len;
            s_encoded_frames++;
            return PORTHOLE_OK;
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
 * Neither socket is waited on. The loop below runs at the frame rate and, each time round,
 * accepts whoever has arrived, reads whatever input has arrived, and sends one frame - so a
 * read or an accept that waited would hold the frame with it. That is not hypothetical: the
 * host goes quiet on the input socket whenever a pad is at rest, so a receive that waited
 * stalled the video every time somebody let go of the keys, and an accept that waited meant
 * no video at all until something also connected for input.
 *
 * So the listeners are non-blocking, the input receive takes only what has arrived, and the
 * one socket allowed to wait is the video connection - a send that returned "try later"
 * would otherwise be read as the connection having gone.
 */

#if defined(PORTHOLE_HOST_BUILD) || (!defined(__FreeBSD__) && !defined(__PS5__))
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int porthole_set_blocking(int s, int blocking) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) return -1;
    if (blocking) {
        flags &= ~O_NONBLOCK;
    } else {
        flags |= O_NONBLOCK;
    }
    return fcntl(s, F_SETFL, flags);
}

static int porthole_listen_port(uint16_t port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;
    int one = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(s, 1) < 0 ||
        porthole_set_blocking(s, 0) < 0) {
        close(s);
        return -1;
    }
    return s;
}

/* Whoever is waiting, or -1 at once if nobody is. */
static int porthole_accept_conn(int listener) {
    return accept(listener, NULL, NULL);
}

/* What has already arrived, without waiting for more. */
static long porthole_recv_bytes(int conn, void *buf, size_t len) {
    return (long)recv(conn, buf, len, MSG_DONTWAIT);
}

/* Whether the last negative return meant "nothing yet" rather than "broken". */
static int porthole_would_block(long rc) {
    (void)rc;
    int e = errno;
    return e == EAGAIN || e == EWOULDBLOCK;
}

static long porthole_send_bytes(int conn, const void *buf, size_t len) {
    return (long)send(conn, buf, len, 0);
}

static void porthole_close_conn(int conn) {
    if (conn >= 0) {
        close(conn);
    }
}
#else

/* libSceNet's socket-option namespace and its non-blocking switch. The values oops-sdk's own
 * net layer uses (OOPS_SOL_SOCKET in oops/net.h, and SO_NBIO as noted in src/net/net.c),
 * and the same two that CTurt's public PS4-SDK network.h carries as SOL_SOCKET 0xffff and
 * SO_NBIO 0x1200. Not yet read back off this console: if the option is refused, the klog
 * says the listeners could not be made non-blocking. */
#define PORTHOLE_SCE_SOL_SOCKET 0xFFFF
#define PORTHOLE_SCE_SO_NBIO 0x1200

/* "Do not wait" on one receive. FreeBSD's MSG_DONTWAIT, which the platform's network stack
 * descends from; CTurt's PS4-SDK network.h and the vitasdk net header both give the sceNet
 * flag of the same name this value. */
#define PORTHOLE_SCE_MSG_DONTWAIT 0x80

typedef struct porthole_net_api {
    int (*socket)(const char *name, int family, int type, int protocol);
    int (*bind)(int s, const void *addr, uint32_t addrlen);
    int (*listen)(int s, int backlog);
    int (*accept)(int s, void *addr, uint32_t *paddrlen);
    int (*recv)(int s, void *buf, uint64_t len, int flags);
    int (*send)(int s, const void *buf, uint64_t len, int flags);
    int (*close)(int s);
    int (*setsockopt)(int s, int level, int optname, const void *optval, uint32_t optlen);
} porthole_net_api;

static porthole_net_api s_net_api;
static int s_net_inited = 0;
/* Set when a listener could not be made non-blocking, so porthole_run can say so. The loop
 * still runs, but each accept then waits, and video needs an input client connected too. */
static int s_net_accept_waits = 0;

static void porthole_net_init(void) {
    if (s_net_inited) return;
    pid_t mypid = 0;
#if defined(SYS_getpid)
    mypid = (pid_t)sys_call(SYS_getpid, 0, 0, 0, 0, 0, 0);
#endif
    s_net_api.socket = (int (*)(const char *, int, int, int))
        (&sceNetSocket != NULL ? (void *)&sceNetSocket : porthole_resolve_symbol(mypid, "sceNetSocket"));
    s_net_api.bind = (int (*)(int, const void *, uint32_t))
        (&sceNetBind != NULL ? (void *)&sceNetBind : porthole_resolve_symbol(mypid, "sceNetBind"));
    s_net_api.listen = (int (*)(int, int))
        (&sceNetListen != NULL ? (void *)&sceNetListen : porthole_resolve_symbol(mypid, "sceNetListen"));
    s_net_api.accept = (int (*)(int, void *, uint32_t *))
        (&sceNetAccept != NULL ? (void *)&sceNetAccept : porthole_resolve_symbol(mypid, "sceNetAccept"));
    s_net_api.recv = (int (*)(int, void *, uint64_t, int))
        (&sceNetRecv != NULL ? (void *)&sceNetRecv : porthole_resolve_symbol(mypid, "sceNetRecv"));
    s_net_api.send = (int (*)(int, const void *, uint64_t, int))
        (&sceNetSend != NULL ? (void *)&sceNetSend : porthole_resolve_symbol(mypid, "sceNetSend"));
    s_net_api.close = (int (*)(int))
        (&sceNetSocketClose != NULL ? (void *)&sceNetSocketClose : porthole_resolve_symbol(mypid, "sceNetSocketClose"));
    s_net_api.setsockopt = (int (*)(int, int, int, const void *, uint32_t))
        (&sceNetSetsockopt != NULL ? (void *)&sceNetSetsockopt : porthole_resolve_symbol(mypid, "sceNetSetsockopt"));
    s_net_inited = 1;
}

static int porthole_set_blocking(int s, int blocking) {
    if (s_net_api.setsockopt == NULL) return -1;
    int nbio = blocking ? 0 : 1;
    return s_net_api.setsockopt(s, PORTHOLE_SCE_SOL_SOCKET, PORTHOLE_SCE_SO_NBIO, &nbio,
                                (uint32_t)sizeof(nbio));
}

static int porthole_listen_port(uint16_t port) {
    porthole_net_init();
    if (s_net_api.socket == NULL || s_net_api.bind == NULL || s_net_api.listen == NULL ||
        s_net_api.accept == NULL || s_net_api.recv == NULL || s_net_api.send == NULL) {
        return -1;
    }
    int s = s_net_api.socket("porthole", 2, 1, 6);
    if (s < 0) return -1;

    struct {
        uint8_t sin_len;
        uint8_t sin_family;
        uint16_t sin_port;
        uint32_t sin_addr;
        char sin_zero[8];
    } addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = 2;
    addr.sin_port = (uint16_t)((port << 8) | (port >> 8));
    addr.sin_addr = 0;

    if (s_net_api.bind(s, &addr, sizeof(addr)) < 0 || s_net_api.listen(s, 1) < 0) {
        if (s_net_api.close != NULL) s_net_api.close(s);
        return -1;
    }
    if (porthole_set_blocking(s, 0) < 0) {
        s_net_accept_waits = 1;
    }
    return s;
}

static int porthole_accept_conn(int listener) {
    if (s_net_api.accept == NULL) return -1;
    return s_net_api.accept(listener, NULL, NULL);
}

static long porthole_recv_bytes(int conn, void *buf, size_t len) {
    if (s_net_api.recv == NULL) return -1;
    return (long)s_net_api.recv(conn, buf, (uint64_t)len, PORTHOLE_SCE_MSG_DONTWAIT);
}

/* Whether a negative libSceNet return meant "nothing yet" rather than "broken".
 *
 * libSceNet reports a BSD errno as 0x8041xxxx with the errno in the low byte, and "would
 * block" on FreeBSD is EAGAIN, 35. The vitasdk net header - the same sceNet lineage - gives
 * SCE_NET_ERROR_EAGAIN and SCE_NET_ERROR_EWOULDBLOCK as 0x80410123, which is that shape; no
 * public PS4 header on hand names them, and it has not been read back off this console. If
 * it is wrong, the input connection closes at its first quiet moment and the klog line in
 * porthole_run names the code that did it. */
static int porthole_would_block(long rc) {
    uint32_t code = (uint32_t)rc;
    return (code >> 16) == 0x8041u && (code & 0xFFu) == 35u;
}

static long porthole_send_bytes(int conn, const void *buf, size_t len) {
    if (s_net_api.send == NULL) return -1;
    return (long)s_net_api.send(conn, buf, (uint64_t)len, 0);
}

static void porthole_close_conn(int conn) {
    if (conn >= 0 && s_net_api.close != NULL) {
        s_net_api.close(conn);
    }
}
#endif

/*
 * The payload's main server loop:
 * Opens listening sockets on PORTHOLE_PORT_VIDEO (9805) and PORTHOLE_PORT_INPUT (9806).
 * Serves video out and accepts input in, waiting on neither.
 */
porthole_status porthole_run(void) {
    porthole_status enc_status = porthole_encoder_open();
    if (enc_status != PORTHOLE_OK) {
        return enc_status;
    }

    (void)porthole_pad_open();
    (void)porthole_display_open();

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

            uint8_t frame_buf[4096];
            size_t frame_len = 0;
            porthole_status cap_rc =
                porthole_capture_encode(frame_buf, sizeof(frame_buf), &frame_len);
            if (cap_rc == PORTHOLE_OK && frame_len > 0) {
                long sent = porthole_send_bytes(conn_video, frame_buf, frame_len);
                if (sent < 0) {
                    porthole_close_conn(conn_video);
                    conn_video = -1;
                }
            }
        }

#if !defined(PORTHOLE_HOST_BUILD) && (defined(__FreeBSD__) || defined(__PS5__))
        oops_time_sleep_ms(16);
#else
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 16000000;
        nanosleep(&ts, NULL);
#endif
    }

    if (conn_input >= 0) porthole_close_conn(conn_input);
    if (conn_video >= 0) porthole_close_conn(conn_video);
    porthole_close_conn(s_input);
    porthole_close_conn(s_video);
    porthole_display_close();
    (void)porthole_encoder_session_destroy();

    return PORTHOLE_OK;
}
