# tracer - API and GPU Shader Telemetry Tracer

An in-process diagnostic and telemetry tracer designed to run inside target processes (via `injector`) or as a standalone diagnostic payload.

## Capabilities

1. **API Call Interception**:
   - Records function NID, thread ID, sequence counter, and up to 6 SysV register arguments.
   - Records function return codes.
2. **Out-Parameter Struct Layouts**:
   - Inlines small structures (<= 32 bytes) for precise layout diffing.
   - Automatically hashes larger buffers via FNV-1a to record change state without redistributing copyrighted game assets.
3. **Rate Limiting & Safety**:
   - Per-NID call capping (`OBS_TRACE_CAP = 64`) via an open-addressed linear probing sampler, preventing hot loops from degrading frame pacing.
   - Emits cumulative `COUNT` records at drain to preserve true execution totals.
4. **GPU Command & Shader Telemetry**:
   - Records raw RDNA2 ISA shader registrations (`OBS_TRACE_SHADER`).
   - Records PM4 command buffer submissions (`OBS_TRACE_DCB`).

## Role in THE LOOP

Within the [OOPS ecosystem](../../../docs/THE_LOOP.md), `tracer` is the **Passive Observation Engine**:

```
Commercial Title Executing on Physical PS5 Hardware
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│ tracer (In-Process Hook Engine)                        │
│ - Hooks API calls, sceAgcSubmitDcb, sceVideoOutSubmit  │
│ - Captures real call sequences, arguments, & returns   │
│ - Dumps submitted PM4 DCB packets & RDNA2 shader code  │
└────────────────────┬───────────────────────────────────┘
                     │ Flushes binary trace
                     ▼
          /data/trace-<TITLE_ID>.bin
                     │
                     ▼ (pulled via pros pull)
┌────────────────────────────────────────────────────────┐
│ obscene-tool trace <bin>                               │
│ - Emits standard OBS| records                          │
└────────────────────┬───────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────┐
│ Orbistoun Emulator (orbistoun-corpus & orbistoun-gpu)  │
│ - Grounds PM4 packet decoders with real draw streams   │
│ - Grounds RDNA2 shader recompiler with real bytecode   │
└────────────────────────────────────────────────────────┘
```

## Building & Verification

```bash
# Host selftest
make check

# Target payload compilation
make elf

# Stage artifact for distribution
make dist
```

## Offline Decoding & Ingestion
```bash
# Pull binary trace from target console
pros.exe pull /data/trace-CUSA12345.bin .

# Decode into human-readable / machine-parsable OBS records
obscene-tool trace trace-CUSA12345.bin > trace-CUSA12345.obs.log
```
