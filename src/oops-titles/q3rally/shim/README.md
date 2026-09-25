# Shim

**Empty: nothing has been measured yet, so nothing is known to be needed.**

This is where the port answers what upstream expects and the target does not have, without editing
upstream. `shim/include/` goes on the include path ahead of upstream's own headers, so a missing
system header can be supplied here; a shim translation unit supplies the functions behind it.

Two rules earned elsewhere in this collection:

- **A shim that reports success without doing the work is worse than a missing one.** `common/posix`
  has `chmod` and `ftruncate` failing with `ENOSYS` rather than returning 0, because a caller told it
  succeeded then relies on it.
- **Anything reusable belongs in `common/`, not here.** The BSD sockets Craft needed and the SQLite
  platform layer both started as one title's problem and are now shared.
