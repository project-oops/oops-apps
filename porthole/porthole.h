/*
 * Porthole - the wire contract.
 *
 * Porthole is the DIY remote-play stand-in: our own video stream out and our own
 * controller state in, over our own payload, so watching and playing a jailbroken
 * target never speaks the vendor's remote-play protocol. The design is
 * prosperous/docs/VIDEO.md "Part three: Porthole"; this subtree is the target-side
 * payload, and this header is the half of it that is *decided* rather than *discovered*
 * - the bytes on the two sockets.
 *
 * Two sockets, two directions, no negotiation:
 *   9805  --->  encoded video out (raw Annex-B H.264, start-code delimited, no
 * container of ours) 9806  <---  controller state in (a fixed 24-byte PORTHOLE record,
 * one per update, up to 60/s)
 *
 * Everything here is a decision both ends of the wire honour; nothing here is measured
 * off the platform. What *is* still a question - can an unsigned payload reach the
 * hardware encoder at all - lives in porthole.c behind PORTHOLE_NO_ENCODER, and is what
 * the obSCEne probes answer.
 *
 * Freestanding: no libc. Fixed-width types only, chosen so the payload writes
 * structures it already holds in the platform's own little-endian order rather than
 * swapping bytes it might swap wrongly.
 */
#ifndef PORTHOLE_H
#define PORTHOLE_H

#include <stddef.h>
#include <stdint.h>

/* The two ports, adjacent so they are memorable together, and clear of every port the
 * chain already used as measured on 2026-08-25 (9021 loader, 9022 the frame-grabber,
 * 2121/3232/2323/ 8084, 6967 scripted input). A collision is a one-line change and a
 * note in VIDEO.md. */
#define PORTHOLE_PORT_VIDEO 9805u
#define PORTHOLE_PORT_INPUT 9806u

/* The controller record, 24 bytes, little-endian, one per update.
 *
 * Absolute state, never deltas: the newest record supersedes every older one, so a
 * receiver behind by three applies the last and drops two. The button bit layout and
 * the stick range are
 * **not ours to choose** - they are the target's own pad structure, confirmed
 * empirically by the Ghostpad project against real hardware (see obSCEne
 * ACKNOWLEDGEMENTS.md when this lands). This header fixes only the record's *shape*;
 * the meaning of individual button bits is Ghostpad's and belongs beside the code that
 * fills them, not here.
 */
#define PORTHOLE_PAD_MAGIC0 'P'
#define PORTHOLE_PAD_MAGIC1 'P'
#define PORTHOLE_PAD_MAGIC2 'A'
#define PORTHOLE_PAD_MAGIC3 'D'

/* Highest slot a record may name. A record for a fifth pad is a sender believing
 * something untrue, and is refused rather than clamped onto the fourth - delivering it
 * as pad three's input would put one person's controls on another. */
#define PORTHOLE_PAD_MAX_SLOT 3u

typedef struct porthole_pad {
    uint8_t magic[4];  /* offset 0:  "PPAD" */
    uint16_t version;  /* offset 4 */
    uint8_t slot;      /* offset 6:  0..PORTHOLE_PAD_MAX_SLOT */
    uint8_t reserved0; /* offset 7:  zero, checked */
    uint32_t buttons;  /* offset 8:  one bit each, the target's own layout (Ghostpad) */
    uint8_t sticks[4]; /* offset 12: LX, LY, RX, RY; unsigned, 128 is centre */
    uint8_t triggers[2]; /* offset 16: L2, R2 pressure; a trigger sets its bit AND its
                            pressure */
    uint16_t reserved1;  /* offset 18: zero, checked - room for gyro/touchpad/rumble via
                            version */
    uint32_t sequence;   /* offset 20: per-slot, so "behind by two" is answerable */
} porthole_pad;

/* The record is exactly 24 bytes with no padding. A parser that finds field boundaries
 * at 250 Hz is a parser that drops inputs, so the layout is fixed and checked, not
 * discovered. */
#define PORTHOLE_PAD_BYTES 24u

/* What a payload operation reports. A non-zero status means the thing did not happen,
 * and each value says why - "it did not work" and "it worked and produced nothing" must
 * never look alike. PORTHOLE_NO_ENCODER is the one that decides whether Porthole is
 * built at all (see porthole.c). */
typedef enum porthole_status {
    PORTHOLE_OK = 0,
    PORTHOLE_UNIMPLEMENTED = 1, /* scaffold: this half is not built yet */
    PORTHOLE_NO_ENCODER =
        2, /* the go/no-go: the hardware encoder could not be reached */
    PORTHOLE_NO_DISPLAY = 3, /* nothing composited to capture */
    PORTHOLE_NO_PAD = 4,     /* pad injection unavailable */
    PORTHOLE_NET = 5,        /* a socket call refused */
    PORTHOLE_BAD_RECORD =
        6 /* an input record failed its own checks (magic/slot/reserved) */
} porthole_status;

/* Reads a 24-byte controller record, checking its magic, slot and reserved bytes, into
 * `out`. Pure and freestanding, so both ends can share it and it can be unit-tested
 * off-hardware. Returns PORTHOLE_OK, or PORTHOLE_BAD_RECORD for a record the payload
 * must not act on. */
porthole_status porthole_pad_decode(const uint8_t bytes[PORTHOLE_PAD_BYTES],
                                    porthole_pad *out);

/* The three things Porthole does, and the loop that drives them. Scaffolded in
 * porthole.c: each reports a status until the real implementation lands, all gated on
 * the encoder go/no-go.
 *
 *   porthole_encoder_open    reach the hardware encoder and hold a session (the
 * go/no-go) porthole_capture_encode  one encoded access unit from the composited
 * output, into the buffer porthole_pad_apply       apply one already-decoded controller
 * record to the target's pads porthole_run             open the two sockets and serve
 * video/apply input until stopped
 */
porthole_status porthole_encoder_open(void);
porthole_status porthole_capture_encode(uint8_t *out, size_t cap, size_t *len);
porthole_status porthole_pad_apply(const porthole_pad *pad);
porthole_status porthole_run(void);
void porthole_stop(void);

/*
 * Display interface (backed by oops-sdk).
 */
porthole_status porthole_display_open(void);
uint32_t *porthole_display_get_framebuffer(void);
void porthole_display_draw_test_pattern(uint32_t frame_index);
int porthole_display_flip(void);
void porthole_display_close(void);

/*
 * Hardware video encoder interface (libSceVencCore).
 * Resolved dynamically via export table walk on loaded sysmodule (D277/D300).
 */
typedef struct porthole_encoder_api {
    void *query_memory_size;    /* sceVencCoreQueryMemorySize */
    void *create_encoder;        /* sceVencCoreCreateEncoder */
    void *delete_encoder;        /* sceVencCoreDeleteEncoder */
    void *get_au_data;           /* sceVencCoreGetAuData */
    void *set_input_frame;       /* sceVencCoreSetInputFrame */
    void *start_sequence;        /* sceVencCoreStartSequence */
    void *stop_sequence;         /* sceVencCoreStopSequence */
    void *sync_encode;           /* sceVencCoreSyncEncode */
} porthole_encoder_api;

/* Returns a pointer to the resolved encoder API functions if opened, or NULL. */
const porthole_encoder_api *porthole_encoder_get_api(void);

/*
 * Video Encoder Configuration (libSceVencCore).
 * Represents encoder session parameters for 1080p60 H.264 / AVC stream.
 */
typedef enum porthole_codec_type {
    PORTHOLE_CODEC_AVC  = 0, /* H.264 / AVC */
    PORTHOLE_CODEC_HEVC = 1  /* H.265 / HEVC */
} porthole_codec_type;

typedef enum porthole_encoder_profile {
    PORTHOLE_PROFILE_AVC_BASELINE = 66,
    PORTHOLE_PROFILE_AVC_MAIN     = 77,
    PORTHOLE_PROFILE_AVC_HIGH     = 100
} porthole_encoder_profile;

typedef enum porthole_rate_control {
    PORTHOLE_RC_CBR = 0,
    PORTHOLE_RC_VBR = 1
} porthole_rate_control;

typedef struct porthole_encoder_config {
    uint32_t size;            /* Structure size for API version check */
    uint32_t codec;           /* PORTHOLE_CODEC_AVC (0) */
    uint32_t width;           /* Horizontal resolution in pixels (e.g. 1920) */
    uint32_t height;          /* Vertical resolution in pixels (e.g. 1080) */
    uint32_t fps_num;         /* Framerate numerator (e.g. 60) */
    uint32_t fps_den;         /* Framerate denominator (e.g. 1) */
    uint32_t bitrate;         /* Target bitrate in bps (e.g. 10000000 = 10 Mbps) */
    uint32_t profile;         /* Profile (e.g. 100 for High Profile) */
    uint32_t level;           /* Level (e.g. 42 for Level 4.2) */
    uint32_t rc_mode;         /* Rate control mode (0=CBR, 1=VBR) */
    uint32_t gop_size;        /* GOP / IDR keyframe interval (e.g. 60) */
    uint32_t reserved[5];     /* Zeroed padding / extension space */
} porthole_encoder_config;

/*
 * Encoder Session State.
 * Holds active encoder handle and allocated working direct memory.
 */
typedef struct porthole_encoder_session {
    int handle;                     /* Encoder instance handle (>= 0 when active) */
    void *work_mem;                 /* Direct memory working buffer virtual pointer */
    int64_t work_mem_phys;          /* Physical base address of working buffer */
    size_t work_mem_size;           /* Size in bytes queried from QueryMemorySize */
    porthole_encoder_config config; /* Active session configuration */
    int session_active;             /* 1 if encoder instance created and running */
} porthole_encoder_session;

/*
 * Encoder AU Information (Access Unit returned by sceVencCoreGetAuData).
 */
typedef struct porthole_au_info {
    void *data;               /* Pointer to encoded bitstream bytes */
    size_t size;              /* Size in bytes of encoded AU */
    uint64_t timestamp;       /* Timestamp or PTS */
    uint32_t picture_type;    /* 1 = I-frame / IDR, 2 = P-frame, 3 = B-frame */
    uint32_t flags;           /* Bitstream flags */
} porthole_au_info;

/*
 * Video Encoder Session Lifecycle (M2).
 */
void porthole_encoder_config_default(porthole_encoder_config *cfg);
porthole_status porthole_encoder_config_validate(const porthole_encoder_config *cfg);
porthole_status porthole_encoder_query_memory(const porthole_encoder_config *cfg, size_t *out_size);
porthole_status porthole_encoder_session_create(const porthole_encoder_config *cfg);
porthole_status porthole_encoder_session_destroy(void);
const porthole_encoder_session *porthole_encoder_get_session(void);
int porthole_encoder_is_active(void);


/*
 * Controller pad interface (libScePad virtual device / Ghostpad path).
 * Resolved dynamically via export table walk on libScePad.
 */
typedef struct porthole_pad_api {
    void *pad_init;              /* scePadInit */
    void *virtual_device_add;    /* scePadVirtualDeviceAddDevice */
    void *virtual_device_delete; /* scePadVirtualDeviceDeleteDevice */
    void *virtual_device_insert; /* scePadVirtualDeviceInsertData */
} porthole_pad_api;

/* Returns a pointer to the resolved pad API functions if opened, or NULL. */
const porthole_pad_api *porthole_pad_get_api(void);

/* Initialize pad subsystem and virtual controller support. */
porthole_status porthole_pad_open(void);

/* Reset pad slots and sequence tracking state. */
void porthole_pad_reset(void);

/* Pass payload arguments (kernel R/W pipes/base) from the loader or session. */
void porthole_set_payload_args(const void *args);

/* Logging utilities */
void klog_write(const char *msg);
void klog_write_hex(const char *prefix, uint64_t hex);
void klog_write_num(const char *prefix, int64_t num);

#endif /* PORTHOLE_H */
