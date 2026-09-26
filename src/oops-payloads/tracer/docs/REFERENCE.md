# tracer - reference

Capabilities, architecture, and the offline decode path. The [README](../README.md) is the overview.

## Capabilities

1. **API call interception** - records function identity (NID or hash), thread ID, sequence counter, SysV register arguments, and return codes.
2. **Out-parameter struct layouts** - inlines small structures (≤ 32 bytes) for layout diffing; hashes larger buffers via FNV-1a to record change states safely and efficiently.
3. **Rate limiting and safety** - per-NID call capping (`OBS_TRACE_CAP = 64`) via an open-addressed linear-probing sampler, ensuring tight loops cannot degrade execution pacing; emits cumulative `COUNT` records at buffer drain to preserve true execution totals.
4. **Filesystem and package resolution** - hooks `sceKernelAprResolveFilepathsToIdsAndFileSizes` to record resolved package file IDs, sizes, and statuses; hooks file opens, closes, and stat queries to record descriptors and inode mappings.
5. **Memory and subsystem probing** - intercepts `sceKernelMapperGetParam` to dump its 56-byte output layout; tracks `sceKernelAllocateDirectMemory` and `sceKernelMapDirectMemory` for physical offsets and virtual mappings.
6. **GPU command and shader telemetry** - captures AGC container headers (`.hdr`) and dumps RDNA2 ISA shader binaries (`.bin`) deduplicated by FNV-1a hash; records PM4 command buffer submissions (`OBS_TRACE_DCB`).

## Workflow

`tracer` executes as an in-process observation payload. Running inside a target process on hardware, it installs detours on active subsystems and drains binary records to `/data/trace.bin`.

```
Target process on hardware ─▶ tracer (in-process detours) ─▶ /data/trace.bin
        pulled via `pros pull` ─▶ offline host decode (OBS| records)
```

## Building and verification

```bash
make check   # host selftest and decoder verification
make elf     # target payload compilation
make dist    # stage artifact for distribution
```

## Offline decoding

Decoding is host-side: the on-disk trace format and its reader live in `trace_decode.c` / `trace_format.h` (tested by `make check`).

```bash
pros.exe pull /data/trace.bin .
./build/tracer_selftest < trace.bin > trace.log
```
