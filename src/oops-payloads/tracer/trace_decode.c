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

struct nid_name_entry {
    uint64_t nid;
    char name[64];
};
static struct nid_name_entry s_name_table[1024];
static uint32_t s_name_count = 0;

static const char *lookup_name(uint64_t nid) {
    for (uint32_t i = 0; i < s_name_count; i++) {
        if (s_name_table[i].nid == nid) {
            return s_name_table[i].name;
        }
    }
    return NULL;
}

static void register_name(uint64_t nid, const char *name) {
    if (lookup_name(nid) != NULL || s_name_count >= 1024 || name == NULL) {
        return;
    }
    s_name_table[s_name_count].nid = nid;
    size_t len = strlen(name);
    if (len >= sizeof(s_name_table[0].name))
        len = sizeof(s_name_table[0].name) - 1;
    memcpy(s_name_table[s_name_count].name, name, len);
    s_name_table[s_name_count].name[len] = '\0';
    s_name_count++;
}

void obs_trace_decode_record(const struct obs_trace_rec *r, FILE *out) {
    if (r == NULL || out == NULL) {
        return;
    }
    switch (r->kind) {
    case OBS_TRACE_ENTRY: {
        const char *name = lookup_name(r->nid);
        fprintf(out, "OBS|call|%s|", name ? name : "?");
        print_nid(out, r->nid);
        fprintf(out, "|%u|%u", (unsigned)r->tid, (unsigned)r->seq);
        for (unsigned i = 0; i < r->argc && i < OBS_TRACE_ARGS; i++) {
            fprintf(out, "|%llx", (unsigned long long)r->arg[i]);
        }
        fprintf(out, "\n");
        break;
    }
    case OBS_TRACE_EXIT: {
        const char *name = lookup_name(r->nid);
        if (name != NULL) {
            fprintf(out, "OBS|ret|%s|", name);
        } else {
            fprintf(out, "OBS|ret|");
        }
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
        register_name(r->nid, name);
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
    case OBS_TRACE_APR_RESOLVE: {
        uint32_t idx = r->seq;
        uint64_t id = r->nid;
        uint64_t size = r->arg[0];
        uint32_t status = (uint32_t)r->arg[1];
        char path[33];
        memcpy(path, &r->arg[2], 32);
        path[32] = '\0';
        fprintf(out, "OBS|apr_resolve|idx=%u|id=%llu|size=%llu|status=%u|path=%s\n",
                idx, (unsigned long long)id, (unsigned long long)size, status, path);
        break;
    }
    case OBS_TRACE_OPEN: {
        int fd = (int)(int64_t)r->arg[0];
        uint64_t flags = r->arg[1];
        char path[33];
        memcpy(path, &r->arg[2], 32);
        path[32] = '\0';
        fprintf(out, "OBS|open|fd=%d|flags=0x%llx|path=%s\n", fd,
                (unsigned long long)flags, path);
        break;
    }
    case OBS_TRACE_STAT: {
        int fd = (int)(int64_t)r->arg[0];
        uint64_t size = r->arg[1];
        uint32_t mode = (uint32_t)r->arg[2];
        uint64_t ino = r->nid;
        fprintf(out, "OBS|stat|fd=%d|ino=%llu|size=%llu|mode=0%o\n", fd,
                (unsigned long long)ino, (unsigned long long)size, (unsigned)mode);
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
    if (hdr.version > OBS_TRACE_VERSION || hdr.version < 1u) {
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
