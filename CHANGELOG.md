# Changelog

oops-apps is a home for programs, not a single artifact - each app under it builds and ships
on its own. There is no repository-wide version; what a release means is decided per app, and
an app's own `README.md` is where its state is recorded.

Entries are grouped **Added / Changed / Fixed**, newest first.

Nothing has shipped yet - this is the initial state.

## [unreleased]

### Added

- **A repository for the homebrew this collection builds on its own SDK.** Something a person
  runs on the hardware for its own sake - a tool, a demo, a service - built on
  [oops-sdk](https://github.com/project-oops/oops-sdk). The admission rule is deliberately
  narrow: a probe that exists to *measure* the platform belongs in obSCEne, an app that *does*
  something belongs here. (D001)
- **Porthole, moving in from obSCEne.** The target half of the capture-and-input path whose
  host half lives in Prosperous - it was a payload inside a probe and is now an app in its own
  right, which is what prompted the repository. The freestanding base layer it needs came in
  with it on loan, and belongs in oops-sdk rather than here. (D002)

### Changed

- **Porthole's target sockets are the platform's POSIX ones, not libSceNet.** obSCEne dumped the
  export table an elfldr payload is handed on 2026-09-08, and none of libSceNet is in it - not
  `sceNetSocket`, not one of the nine the payload needed. The POSIX layer is: `bind`, `listen`,
  `accept`, `recv`, `_sendto`, `_setsockopt`, `close`, `fcntl` and `__error`, at real addresses.
  So the payload now reaches sockets the way its own host build always did, both halves speak one
  shape, and the two spellings the platform uses are tried per call rather than one firmware's
  naming being baked in. `socket` itself was in none of seventeen spellings, so it is resolved by
  name where offered and otherwise asked of the kernel, with the klog naming which route worked.
  The socket itself comes from `__sys_socketex`, which takes a name the way the vendor's own
  creator does and was found under a spelling none of seventeen guesses reached; the raw system
  call is kept behind it, both routes having returned a working descriptor on hardware.
- **Porthole sets a socket's mode with the socket option, because `fcntl` cannot do it here.**
  Measured on this console: `fcntl(F_SETFL, O_NONBLOCK)` answers -1 while
  `setsockopt(SOL_SOCKET, SO_NBIO)` answers 0. Reading flags back with `F_GETFL` does work, so
  this is a platform that will tell you a socket's mode but not let you set it that way. `fcntl`
  is still tried second, for a firmware where the option is the half that is missing.
- **A full send buffer is waited out rather than treated as a dead connection.** An accepted
  socket inherits the listener's non-blocking mode here, confirmed by reading the flags back off
  one, so the video connection is put back to blocking; should that ever fail, a full buffer
  answers try-again and closing on it would drop a working connection whenever the network fell
  behind. Giving up mid-unit is not an option either, since half an access unit on the wire is
  the corruption the send loop exists to prevent.
  The virtual-port field that made two sockaddr shapes disagree is no part of the POSIX struct,
  so that question does not arise on this path.
- **Porthole's encoder session is off unless a build asks for it.** The four struct-taking
  encoder calls pass parameter layouts not yet confirmed against the platform, which obSCEne's
  D300 had reserved for M2, yet every start made two of them and every frame the other two. They
  are now compiled in only with `PORTHOLE_ENCODER_SESSION`, a default build serves the template
  stream, and the selftest asserts the default. (D003)

### Fixed

- **Porthole serves without the encoder, which is the only way the gate works.** `porthole_run`
  refused to start unless the encoder opened, and `porthole_capture_encode` refused unless it
  had, so a gated payload would have exited without opening a socket - the opposite of what the
  gate was for. obSCEne measured on 2026-09-08 that no `sceVencCore` entry point resolves on any
  delivery route, and that none of the twenty-four is in the export table an elfldr payload is
  handed, so this was not hypothetical. The encoder is now attempted and logged, never required; gated off, the
  template stream serves. A gate-on build still refuses, having been asked for video it cannot
  produce. (D003)
- **The startup log no longer announces a pad subsystem it did not reach.** Opening the pad
  always returned success, and the entry point took that as licence to print "self-resolved
  successfully" above whatever addresses it had - which on this hardware, where none of the four
  controller symbols is in a payload's export table, meant announcing success above four zeroes.
  Opening now reports `PORTHOLE_NO_PAD` when the add and insert calls are missing, since without
  those there is no input path at all, and the log prints the addresses either way with a
  headline saying which case it is. Records are still read, checked and sequence-tracked; they
  are just not injected, and the log now says so.
- **A controller record of an unknown version is refused rather than interpreted.** The host half
  has refused a version mismatch from the start and the payload accepted anything, so the field
  meant nothing on the receiving end and a later version's gyro bytes would have been read as
  reserved zeroes.
- **A video frame is sent until all of it has gone.** A short write dropped the tail of an
  access unit, which the host's reader resynchronises past - so the loss would have surfaced as
  a stuttering picture rather than as an error anywhere. The template units are too small for it
  to bite today; real frames are not.
- **One access unit gets a second of the configured bitrate.** The frame buffer was four
  kilobytes on the stack, which no 1080p keyframe fits, so with the encoder gate on every real
  access unit would have been refused and the test pattern served instead - a stream that frames
  and keyframes perfectly while showing colour bars, which is the one fault the host cannot tell
  from the outside. It is now static, because a payload's stack will not hold a megabyte and
  there is no allocator, and sized from `PORTHOLE_DEFAULT_BITRATE` so the two cannot drift
  apart. A compile-time floor refuses a configuration too small for a keyframe, and an access
  unit that still does not fit names its size in the log rather than quietly becoming a test
  pattern.

- **Porthole's serving loop no longer waits on its input socket.** A receive that blocked held
  the video frame with it, so the stream stalled whenever a pad was at rest and the host went
  quiet; a blocking accept meant no video at all until an input client also connected. The
  listeners are now non-blocking, the input receive takes only what has arrived, and the video
  connection is set to block explicitly.
- **A host that reconnects is a new sender.** Each slot's last sequence is forgotten when a new
  input connection is accepted, so a restarted host's records are not dropped as stale against
  the previous sender's high mark. A superseded record now reports `PORTHOLE_STALE` rather
  than looking applied, which is what lets the selftest tell the two apart.
