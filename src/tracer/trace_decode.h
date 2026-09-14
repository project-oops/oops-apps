#ifndef OOPS_TRACE_DECODE_H
#define OOPS_TRACE_DECODE_H

#include <stdio.h>
#include "trace_format.h"

#ifdef __cplusplus
extern "C" {
#endif

int obs_trace_decode_stream(FILE *in, FILE *out);
void obs_trace_decode_record(const struct obs_trace_rec *r, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* OOPS_TRACE_DECODE_H */

