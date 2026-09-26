# tracer - reference

Capabilities, role, and the offline decode path. The [README](../README.md) is the overview.

## Capabilities

1. **API call interception** - records function NID, thread ID, sequence counter, up to 6 SysV
   register arguments, and the return code.
2. **Out-parameter struct layouts** - inlines small structures (≤ 32 bytes) for layout diffing;
   hashes larger buffers via FNV-1a to record change state without redistributing copyrighted
   game assets.
3. **Rate limiting and safety** - per-NID call capping (`OBS_TRACE_CAP = 64`) via an
   open-addressed linear-probing sampler, so a hot loop cannot degrade frame pacing; emits
   cumulative `COUNT` records at drain to preserve true execution totals.
4. **GPU command and shader telemetry** - records raw RDNA2 ISA shader registrations
   (`OBS_TRACE_SHADER`) and PM4 command-buffer submissions (`OBS_TRACE_DCB`).

## Role in the loop

tracer is the collection's **passive observation engine**. Running inside a commercial title on
the hardware, it hooks API calls, `sceAgcSubmitDcb` and `sceVideoOutSubmit`, capturing call
sequences, arguments and returns and dumping submitted PM4 packets and RDNA2 shader code. The
binary trace flushes to `/data/trace-<TITLE_ID>.bin`.

```
Title on hardware ─▶ tracer (in-process hooks) ─▶ /data/trace-<TITLE_ID>.bin
        pulled via `pros pull` ─▶ offline host decode (OBS| records)
        ─▶ Orbistoun (grounds the PM4 decoders and the RDNA2 recompiler with real streams)
```

## Building and verification

```bash
make check   # host selftest
make elf     # target payload compilation
make dist    # stage artifact for distribution
```

## Offline decoding

Decoding is host-side: the on-disk trace format and its reader live in this app's own
`trace_decode.c` / `trace_format.h` (exercised by `make check`). The decoded OBS records - PM4
draw streams and RDNA2 shader bytecode - ground Orbistoun's packet decoders and shader
recompiler.

```bash
pros.exe pull /data/trace-<TITLE_ID>.bin .
```
