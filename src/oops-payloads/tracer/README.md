# tracer

<p align="center">
  <img src="../../../common/assets/no-logo.svg" alt="No logo yet" width="200">
</p>

An in-process API, file I/O, memory, and GPU command/shader telemetry tracer for target processes.

## What it is

`tracer` is an in-process telemetry and observation payload for native processes. It runs inside a target process (injected via `injector` or executed standalone) to monitor system interactions, record function calls and argument shapes, capture out-parameter memory layouts, and record GPU command buffers and compiled shader binaries.

## How it works

1. **Inline detour interception.** Installs 14-byte inline x86_64 detours (`jmp [rip+0]`) on target function entry points. Trampolines preserve prologue instructions and execute the original implementation after recording telemetry.
2. **Dynamic symbol resolution.** Resolves target entry points at startup across loaded modules and libraries via symbol names, computed NID hashes (`obs_compute_nid`), and kernel export tables.
3. **Low-overhead binary ring buffer.** Telemetry events are encoded into fixed 64-byte records (`struct obs_trace_rec`) in memory with zero allocations and zero string formatting on the hot path.
4. **Rate limiting.** An open-addressed linear-probing sampler caps hot functions to a configurable threshold (`OBS_TRACE_CAP = 64`) to prevent ring-buffer saturation and maintain execution pacing.
5. **Periodic flushing.** Flushes binary records to `/data/trace.bin` periodically on display presentation flips or when buffer capacity is reached.
6. **Offline decoding.** The resulting binary trace is decoded host-side by `trace_decode` into structured text records (`OBS|`).

## What it captures

- **Filesystem and package operations:**
  - Asynchronous package reads (`sceKernelAprResolveFilepathsToIdsAndFileSizes`) - requested paths, resolved file IDs, file sizes, and status codes.
  - File descriptor operations - `sceKernelOpen`, `open`, `sceKernelClose`, `close`.
  - File metadata queries - `sceKernelFstat`, `fstat` (inode numbers and out-parameter metadata).
- **Subsystem and memory parameters:**
  - Kernel mapper parameters (`sceKernelMapperGetParam`) - records the 56-byte output layout.
  - Direct memory allocation and mapping - `sceKernelAllocateDirectMemory` and `sceKernelMapDirectMemory` (lengths, alignments, physical offsets, and virtual addresses).
  - Dynamic module loading - `sceSysmoduleLoadModule` and internal loaders (module IDs and return codes).
- **GPU telemetry:**
  - Shader compilation (`sceAgcCreateShader`) - dumps AGC container headers (`.hdr`) and distinct RDNA2 bytecode binaries (`.bin`) deduplicated by FNV-1a hash.
  - Command buffer submissions (`sceAgcDriverSubmitDcb`) - records PM4 command buffer GPU addresses, sizes in dwords, and queue indices.
- **Generic function calls:**
  - SysV register arguments, return codes, thread IDs, and sequence correlation numbers.
  - Out-parameter structures - inline byte payloads for small buffers (≤ 32 bytes) or FNV-1a hashes for larger buffers.

## Building and testing

```bash
make check   # runs host-side self-tests and wire format decoder verification
make elf     # builds freestanding target payload ELF
```

## Docs

- [Reference](docs/REFERENCE.md) - architecture, wire format, and decode workflow.
- [oops-apps catalog and guide](../../../docs/USER_GUIDE.md)
