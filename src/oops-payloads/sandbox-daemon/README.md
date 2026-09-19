# sandbox-daemon

On-demand filesystem namespace and unsandboxing daemon for Prospero.

## Role

`sandbox-daemon` runs as a decoupled background service under `pldmgr` / `elfldr`.
It services unsandboxing requests on-demand from Big Apps (`SeaShell`, homebrew tools)
by updating the requesting process's `p_fd->fd_rdir`, `fd_jdir`, `fd_cdir`, and `p_ucred`
credentials in kernel memory.

## Architecture: Zero-Polling Event Loop

Unlike legacy jailbreak daemons that continuously poll trigger files in `/data` every 500ms–1000ms,
`sandbox-daemon` is completely **event-driven**:
- **Blocking Socket**: Listens on `127.0.0.1:9069` via standard blocking `sys_call(SYS_accept)`.
- **Zero Overhead**: While waiting for client requests, the daemon thread sleeps in the kernel's
  socket wait queue, consuming **0% CPU** and performing **zero disk I/O**.
- **On-Demand Execution**: Wakes only when a client connects, services the handshake in microseconds,
  and returns immediately to sleep.

## Kernel Structures & Offsets (FreeBSD 11/12 ABI)

- **`struct filedesc` Layout**:
  - `0x08`: `fd_cdir` (`struct vnode *`, current working directory vnode)
  - `0x10`: `fd_rdir` (`struct vnode *`, chroot root directory vnode)
  - `0x18`: `fd_jdir` (`struct vnode *`, jail root directory vnode)
- **Dynamic `rootvnode` Resolution**:
  - Standalone daemons and PID 1 (`mini-syscore`) operate without having called `chroot(2)`, meaning
    `fd_rdir` defaults to NULL while `fd_cdir` points to the kernel's true `rootvnode`.
  - On PS5 FW 12.40, PID 1 has both `fd_rdir` and `fd_cdir` set to `0xffffdc5d01d89fe0`.
  - The daemon dynamically checks PID 1 and self to resolve `rootvnode` with zero hardcoding.
- **Dynamic `allproc` Discovery**:
  - Scans `[kdata+0x2600000, kdata+0x2B00000]` for a valid proc chain containing the daemon's own PID.
- **Process Credential Escalation**:
  - Sets root UID: `cr_uid = 0`, `cr_ruid = 0`.
  - Sets SCE authority: `cr_sceauthid = 0x4801000000000013`.
  - Sets all capabilities: `cr_scecaps = 0xFFFFFFFFFFFFFFFF`.
  - Sets SCE attributes: `attrs[3] |= 0x80`.
  - Borrows `cr_prison` from PID 1 (`prison0`).

## Protocol

- **Client Handshake**: TCP `127.0.0.1:9069` — client sends 4-byte PID, daemon replies with int32_t status (`0` = success).
- **Client Disconnect**: Client closes socket upon receiving status and proceeds with global storage access.

## Build

```bash
make check      # Host test
make skeleton   # Freestanding target object
make elf        # Target payload ELF (dist/sandbox-daemon-prospero.elf)
```

