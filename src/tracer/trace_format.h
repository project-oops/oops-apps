/*
 * The tracer wire format - interface between the in-process tracer and host tools.
 *
 * Designed for low-overhead binary capture inside target processes:
 * 1. Fixed 64-byte records (cache-line sized).
 * 2. Zero allocations and zero formatting on the hot path.
 * 3. Supports SysV function call/return tracing, outbuf diffing, and GPU
 *    command/shader telemetry.
 */
#ifndef OOPS_TRACE_FORMAT_H
#define OOPS_TRACE_FORMAT_H

#include <stdint.h>

#define OBS_TRACE_MAGIC "OBSTRACE"
#define OBS_TRACE_VERSION 2u
#define OBS_TRACE_REC_SIZE 64u
#define OBS_TRACE_ENDIAN 0x01020304u

/* How many integer arguments a record can carry (rdi, rsi, rdx, rcx, r8, r9). */
#define OBS_TRACE_ARGS 6u

/* Inline capacity for an out-parameter buffer chunk in bytes. */
#define OBS_TRACE_OUTBUF_INLINE 32u

enum obs_trace_kind {
    OBS_TRACE_ENTRY   = 1u, /* function call: nid, seq, argc values in arg[] */
    OBS_TRACE_EXIT    = 2u, /* function return: arg[0] is return value */
    OBS_TRACE_OUTBUF  = 3u, /* outbuf: arg[0]=addr, arg[1]=len, inline bytes or hash */
    OBS_TRACE_NAME    = 4u, /* resolved name hint: 48 bytes of symbol name in arg[] */
    OBS_TRACE_COUNT   = 5u, /* total call count for capped nid: arg[0]=count */
    OBS_TRACE_SHADER  = 6u, /* shader capture: arg[0]=addr, arg[1]=size, arg[2]=hash, arg[3]=stage */
    OBS_TRACE_DCB     = 7u, /* PM4 DCB submission: arg[0]=addr, arg[1]=dwords, arg[2]=queue */
};

/* Exactly 64 bytes, cache-line sized, no padding. */
struct obs_trace_rec {
    uint8_t kind; /* enum obs_trace_kind */
    uint8_t argc; /* ENTRY: valid args; OUTBUF: payload len */
    uint16_t tid; /* thread id */
    uint32_t seq; /* call sequence / correlation id */
    uint64_t nid; /* function identity (import NID or symbol hash) */
    uint64_t arg[OBS_TRACE_ARGS];
};

/* 32-byte stream header at the front of a trace file. */
struct obs_trace_hdr {
    char magic[8];        /* OBS_TRACE_MAGIC */
    uint32_t version;     /* OBS_TRACE_VERSION */
    uint32_t rec_size;    /* OBS_TRACE_REC_SIZE */
    uint32_t endian;      /* OBS_TRACE_ENDIAN */
    uint32_t reserved[3]; /* zero */
};

#ifdef __STDC_VERSION__
#if __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(struct obs_trace_rec) == OBS_TRACE_REC_SIZE,
               "trace record must be exactly 64 bytes");
_Static_assert(sizeof(struct obs_trace_hdr) == 32u,
               "trace header must be exactly 32 bytes");
#endif
#endif

#endif /* OOPS_TRACE_FORMAT_H */

