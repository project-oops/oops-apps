/*
 * In-process trace encoder.
 * Freestanding, header-only, zero dynamic allocations.
 */
#ifndef OOPS_TRACE_ENCODE_H
#define OOPS_TRACE_ENCODE_H

#include <stdint.h>
#include "trace_format.h"

#ifndef OBS_TRACE_CAP
#define OBS_TRACE_CAP 64u
#endif

/* Linear append buffer */
struct obs_trace_buf {
    struct obs_trace_rec *recs;
    uint32_t cap;
    uint32_t head;
    uint32_t dropped;
};

static inline void obs_trace_buf_init(struct obs_trace_buf *b,
                                      struct obs_trace_rec *storage, uint32_t cap) {
    b->recs = storage;
    b->cap = cap;
    b->head = 0;
    b->dropped = 0;
}

static inline int obs_trace_buf_push(struct obs_trace_buf *b,
                                     const struct obs_trace_rec *r) {
    if (b->head >= b->cap) {
        b->dropped++;
        return 0;
    }
    b->recs[b->head] = *r;
    b->head++;
    return 1;
}

/* Open-addressed hash table sampler for per-NID rate limiting */
struct obs_trace_sampler {
    uint64_t *nids;   /* Power-of-two size */
    uint32_t *counts;
    uint32_t cap;
    uint32_t cap_n;
    uint32_t full;
};

static inline void obs_trace_sampler_init(struct obs_trace_sampler *s, uint64_t *nids,
                                          uint32_t *counts, uint32_t cap,
                                          uint32_t cap_n) {
    s->nids = nids;
    s->counts = counts;
    s->cap = cap;
    s->cap_n = cap_n;
    s->full = 0;
    for (uint32_t i = 0; i < cap; i++) {
        s->nids[i] = 0;
        s->counts[i] = 0;
    }
}

static inline uint64_t obs_trace_mix(uint64_t x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return x;
}

static inline uint32_t obs_trace_count(const struct obs_trace_sampler *s, uint64_t nid) {
    uint32_t mask = s->cap - 1u;
    uint32_t i = (uint32_t)obs_trace_mix(nid) & mask;
    for (uint32_t probe = 0; probe < s->cap; probe++) {
        uint32_t at = (i + probe) & mask;
        if (s->nids[at] == 0) {
            return 0;
        }
        if (s->nids[at] == nid) {
            return s->counts[at];
        }
    }
    return 0;
}

static inline int obs_trace_hit(struct obs_trace_sampler *s, uint64_t nid) {
    uint32_t mask = s->cap - 1u;
    uint32_t i = (uint32_t)obs_trace_mix(nid) & mask;
    for (uint32_t probe = 0; probe < s->cap; probe++) {
        uint32_t at = (i + probe) & mask;
        if (s->nids[at] == nid) {
            s->counts[at]++;
            return s->counts[at] <= s->cap_n;
        }
        if (s->nids[at] == 0) {
            s->nids[at] = nid;
            s->counts[at] = 1;
            return 1;
        }
    }
    s->full++;
    return 0;
}

/* Fast 64-bit FNV-1a hash */
static inline uint64_t obs_trace_fnv1a(const uint8_t *p, uint32_t len) {
    uint64_t h = 1469598103934665603ULL;
    for (uint32_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static inline void obs_trace_entry(struct obs_trace_buf *b, uint16_t tid, uint32_t seq,
                                   uint64_t nid, const uint64_t *args, uint8_t argc) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_ENTRY;
    r.argc = argc > OBS_TRACE_ARGS ? (uint8_t)OBS_TRACE_ARGS : argc;
    r.tid = tid;
    r.seq = seq;
    r.nid = nid;
    for (uint32_t i = 0; i < OBS_TRACE_ARGS; i++) {
        r.arg[i] = i < r.argc ? args[i] : 0;
    }
    (void)obs_trace_buf_push(b, &r);
}

static inline void obs_trace_exit(struct obs_trace_buf *b, uint16_t tid, uint32_t seq,
                                  uint64_t nid, uint64_t ret) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_EXIT;
    r.argc = 0;
    r.tid = tid;
    r.seq = seq;
    r.nid = nid;
    r.arg[0] = ret;
    for (uint32_t i = 1; i < OBS_TRACE_ARGS; i++) {
        r.arg[i] = 0;
    }
    (void)obs_trace_buf_push(b, &r);
}

static inline void obs_trace_outbuf(struct obs_trace_buf *b, uint64_t nid,
                                    uint64_t addr, const uint8_t *buf, uint32_t len,
                                    uint32_t threshold) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_OUTBUF;
    r.tid = 0;
    r.seq = 0;
    r.nid = nid;
    if (threshold > OBS_TRACE_OUTBUF_INLINE) {
        threshold = OBS_TRACE_OUTBUF_INLINE;
    }
    if (len <= threshold) {
        r.argc = (uint8_t)len;
        r.arg[0] = addr;
        r.arg[1] = len;
        uint8_t *payload = (uint8_t *)&r.arg[2];
        for (uint32_t i = 0; i < OBS_TRACE_OUTBUF_INLINE; i++) {
            payload[i] = i < len ? buf[i] : 0;
        }
    } else {
        r.argc = 0;
        r.arg[0] = addr;
        r.arg[1] = len;
        r.arg[2] = obs_trace_fnv1a(buf, len);
        r.arg[3] = 0;
        r.arg[4] = 0;
        r.arg[5] = 0;
    }
    (void)obs_trace_buf_push(b, &r);
}

static inline void obs_trace_name(struct obs_trace_buf *b, uint64_t nid,
                                  const char *name) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_NAME;
    r.argc = 0;
    r.tid = 0;
    r.seq = 0;
    r.nid = nid;
    uint8_t *dst = (uint8_t *)&r.arg[0];
    uint32_t i = 0;
    for (; i < OBS_TRACE_ARGS * 8u - 1u && name[i] != '\0'; i++) {
        dst[i] = (uint8_t)name[i];
    }
    for (; i < OBS_TRACE_ARGS * 8u; i++) {
        dst[i] = 0;
    }
    (void)obs_trace_buf_push(b, &r);
}

static inline void obs_trace_count_rec(struct obs_trace_buf *b, uint64_t nid,
                                       uint32_t total) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_COUNT;
    r.argc = 0;
    r.tid = 0;
    r.seq = 0;
    r.nid = nid;
    r.arg[0] = total;
    for (uint32_t i = 1; i < OBS_TRACE_ARGS; i++) {
        r.arg[i] = 0;
    }
    (void)obs_trace_buf_push(b, &r);
}

/* GPU Shader Registration Record */
static inline void obs_trace_shader_rec(struct obs_trace_buf *b, uint64_t addr,
                                        uint32_t size, uint32_t stage, uint64_t hash) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_SHADER;
    r.argc = 0;
    r.tid = 0;
    r.seq = 0;
    r.nid = hash;
    r.arg[0] = addr;
    r.arg[1] = (uint64_t)size;
    r.arg[2] = hash;
    r.arg[3] = (uint64_t)stage;
    r.arg[4] = 0;
    r.arg[5] = 0;
    (void)obs_trace_buf_push(b, &r);
}

/* GPU Command Buffer Submission Record */
static inline void obs_trace_dcb_rec(struct obs_trace_buf *b, uint64_t addr,
                                     uint32_t dwords, uint32_t queue) {
    struct obs_trace_rec r;
    r.kind = (uint8_t)OBS_TRACE_DCB;
    r.argc = 0;
    r.tid = 0;
    r.seq = 0;
    r.nid = addr;
    r.arg[0] = addr;
    r.arg[1] = (uint64_t)dwords;
    r.arg[2] = (uint64_t)queue;
    r.arg[3] = 0;
    r.arg[4] = 0;
    r.arg[5] = 0;
    (void)obs_trace_buf_push(b, &r);
}

#endif /* OOPS_TRACE_ENCODE_H */

