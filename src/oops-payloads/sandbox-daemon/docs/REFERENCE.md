# sandbox-daemon - reference

Design and protocol. The [README](../README.md) is the overview; the exact kernel structure
layouts live in the source (`sandbox_daemon.c`), which is their single source of truth.

## Role

`sandbox-daemon` runs as a decoupled background service under `pldmgr` / `elfldr`. It services
requests on demand from Big Apps (`SeaShell`, homebrew tools) by updating the requesting process's
directory and credential fields in kernel memory, so a process that asked for it can reach global
storage.

## Event loop

`sandbox-daemon` is event-driven rather than polling a trigger file:

- **Blocking socket** - listens on `127.0.0.1:9069` via a standard blocking accept.
- **Idle** - while waiting, the daemon thread sleeps in the kernel's socket wait queue, consuming
  no CPU and doing no disk I/O.
- **On demand** - it wakes only when a client connects, services the handshake, and returns to
  sleep.

## Values resolved at run time

Nothing is hardcoded to a firmware. The daemon reads the running kernel to find the values it
needs - the true root directory node, the process table, and the credentials it grants - by
checking known processes against itself rather than baking in an address. The fields it reads
are defined and commented in the source.

## Protocol

- **Handshake** - TCP `127.0.0.1:9069`; the client sends a 4-byte PID, the daemon replies with an
  `int32` status (`0` = success).
- **Disconnect** - the client closes on receiving the status and proceeds with global storage
  access.

## Build

```bash
make check      # host test
make skeleton   # freestanding target object
make elf        # target payload ELF (dist/sandbox-daemon-prospero.elf)
```
