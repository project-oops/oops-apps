/*
 * Trace decoder - turns raw binary trace stream into OBS| formatted text.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "trace_format.h"
#include "trace_decode.h"

static void print_nid(FILE *out, uint64_t nid) {
    fprintf(out, "%016llx", (unsigned long long)nid);
}

void obs_trace_decode_record(const struct obs_trace_rec *r, FILE *out) {
    if (r == NULL || out == NULL) {
        return;
    }
    switch (r->kind) {
    case OBS_TRACE_ENTRY: {
        fprintf(out, "OBS|call|?|");
        print_nid(out, r->nid);
        fprintf(out, "|%u|%u", (unsigned)r->tid, (unsigned)r->seq);
        for (unsigned i = 0; i < r->argc && i < OBS_TRACE_ARGS; i++) {
            fprintf(out, "|%llx", (unsigned long long)r->arg[i]);
        }
        fprintf(out, "\n");
        break;
    }
    case OBS_TRACE_EXIT: {
        fprintf(out, "OBS|ret|");
        print_nid(out, r->nid);
        fprintf(out, "|%u|%llx\n", (unsigned)r->seq, (unsigned long long)r->arg[0]);
        break;
    }
    case OBS_TRACE_OUTBUF: {
        uint64_t addr = r->arg[0];
        uint64_t len = r->arg[1];
        int hashed = (r->argc == 0 && len != 0);
        if (hashed) {
            fprintf(out, "OBS|outbuf|");
            print_nid(out, r->nid);
            fprintf(out, "|%llx|%llu|hash=%016llx\n", (unsigned long long)addr,
                    (unsigned long long)len, (unsigned long long)r->arg[2]);
        } else {
            const uint8_t *payload = (const uint8_t *)&r->arg[2];
            fprintf(out, "OBS|outbuf|");
            print_nid(out, r->nid);
            fprintf(out, "|%llx|%llu|", (unsigned long long)addr,
                    (unsigned long long)len);
            for (unsigned i = 0; i < r->argc && i < OBS_TRACE_OUTBUF_INLINE; i++) {
                fprintf(out, "%02x", payload[i]);
            }
            fprintf(out, "\n");
        }
        break;
    }
    case OBS_TRACE_NAME: {
        char name[OBS_TRACE_ARGS * 8u + 1u];
        memcpy(name, &r->arg[0], OBS_TRACE_ARGS * 8u);
        name[OBS_TRACE_ARGS * 8u] = '\0';
        fprintf(out, "OBS|name|");
        print_nid(out, r->nid);
        fprintf(out, "|%s\n", name);
        break;
    }
    case OBS_TRACE_COUNT: {
        fprintf(out, "OBS|count|");
        print_nid(out, r->nid);
        fprintf(out, "|%llu\n", (unsigned long long)r->arg[0]);
        break;
    }
    case OBS_TRACE_SHADER: {
        uint64_t addr = r->arg[0];
        uint64_t size = r->arg[1];
        uint64_t hash = r->arg[2];
        uint32_t stage = (uint32_t)r->arg[3];
        fprintf(out, "OBS|shader|%u|%llx|%llu|hash=%016llx\n", stage,
                (unsigned long long)addr, (unsigned long long)size,
                (unsigned long long)hash);
        break;
    }
    case OBS_TRACE_DCB: {
        uint64_t addr = r->arg[0];
        uint64_t dwords = r->arg[1];
        uint32_t queue = (uint32_t)r->arg[2];
        fprintf(out, "OBS|dcb|%u|%llu|%llx\n", queue, (unsigned long long)dwords,
                (unsigned long long)addr);
        break;
    }
    default:
        fprintf(stderr, "trace_decode: unknown record kind %u, skipped\n",
                (unsigned)r->kind);
        break;
    }
}

int obs_trace_decode_stream(FILE *in, FILE *out) {
    if (in == NULL || out == NULL) {
        return -1;
    }
    struct obs_trace_hdr hdr;
    if (fread(&hdr, sizeof(hdr), 1, in) != 1) {
        fprintf(stderr, "trace_decode: could not read header\n");
        return 1;
    }
    if (memcmp(hdr.magic, OBS_TRACE_MAGIC, 8) != 0) {
        fprintf(stderr, "trace_decode: bad magic '%.8s'\n", hdr.magic);
        return 2;
    }
    if (hdr.version != OBS_TRACE_VERSION && hdr.version != 1u) {
        fprintf(stderr, "trace_decode: unsupported version %u\n",
                (unsigned)hdr.version);
        return 3;
    }
    if (hdr.rec_size != OBS_TRACE_REC_SIZE) {
        fprintf(stderr, "trace_decode: bad record size %u\n", (unsigned)hdr.rec_size);
        return 4;
    }
    if (hdr.endian != OBS_TRACE_ENDIAN) {
        fprintf(stderr, "trace_decode: stream was written on different-endian host\n");
        return 5;
    }

    fprintf(out, "OBS|tracemeta|version=%u|rec_size=%u\n", (unsigned)hdr.version,
            (unsigned)hdr.rec_size);

    struct obs_trace_rec r;
    while (fread(&r, sizeof(r), 1, in) == 1) {
        obs_trace_decode_record(&r, out);
    }
    return 0;
}

#ifndef TRACER_SELFTEST
int main(void) {
    return obs_trace_decode_stream(stdin, stdout);
}
#endif
