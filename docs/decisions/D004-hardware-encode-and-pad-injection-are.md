# D004 - Hardware encode and pad injection are both out of reach from a payload


**hardware** · 2026-09-09

Porthole's design has two halves that touch the platform: encode the composited output in
hardware and stream it, and inject received controller records into the target's pads. **Neither
is reachable from an elfldr payload.** Both were measured on the console rather than inferred.

## What was measured

obSCEne resolved `sceSysmoduleLoadModule` out of the export table a payload is handed - it is at
`0x8002740d0`, and callable - and then called it from an unsigned payload
(REQ-20260909T0840Z-2d17, REQ-20260909T0840Z-9b3c):

| module | call | answer |
|---|---|---|
| VENC, `0x00A0` | `sceSysmoduleLoadModule` | `0xa0020101`, a privilege refusal raised as a signal |
| libScePad, `0x0027` | `sceSysmoduleLoadModule` | `0x805a1000`, an error return |

Afterwards, resolution was re-attempted by all three routes - dlsym against the returned handle,
a re-read of the export table, and the dynamic-library walk. **All seven `sceVencCore` entry
points and all eight `scePad` entry points came back null on every route.** None of them is in
the payload's export table before the load either; the dump carries 2,441 entries and holds
libkernel and the sysmodule loader, and nothing of pad, encoder, audio or video-out.

## Why this is not another "we could not look"

Because it was asked the way the earlier verdicts were not. `106-encoder` had been reporting
`sceSysmoduleLoadModule is not available` while that function sat in the same run's own table
dump - it had never consulted the table. These requests resolved the loader from the table, used
it, and reported the raw refusal. The absence is now a measurement.

## What follows

**The encoder gate stops being provisional.** D003 gated the struct-taking encoder calls because
their parameter layouts were unconfirmed. That reasoning is superseded by a stronger one: the
module cannot be loaded at all, so there is nothing to pass a layout to. A gated build now
declines to make the call, and says so in the log, because the refusal arrives as a signal and a
payload that trips it risks dying before it opens a socket.

**The input half of Porthole cannot work as designed.** The wire contract, the host sender and
the receive path are finished and tested; the last step, handing a decoded record to
`scePadVirtualDeviceInsertData`, has nothing to call. Records are received, checked and
sequence-tracked, and then go nowhere. The payload says so at startup rather than implying
otherwise.

**What a payload can still do** is the useful half of the answer: sockets, through the POSIX
layer (D003's amendment and the requests behind it), the display through oops-sdk, direct memory,
and timing. So a payload can serve a stream and receive input; what it cannot do is produce the
stream in hardware or apply the input.

## What this does not decide

Whether another delivery route reaches either API. The title routes resolve libScePad through
ordinary imports, so injection may well be possible there - but a title is a foreground
application, and Porthole has to be a resident service watching *another* title, so that is not a
straight swap. Two requests are open on that question. Until one comes back, this entry records
the payload route only, which is the route Porthole ships on.

---

**amended** · 2026-09-09

The two questions this entry left open are now answered, both terminally, and one of them is
worse than it looked.

**Pad injection is unreachable in every leg, not just from a payload.** The three virtual-device
functions resolve to zero by dlsym, by the export table and by the dynamic-library walk, in the
**pkg title leg as well as the payload** (obSCEne REQ-20260909T1021Z-5e88). The earlier hope that
a title might reach what a payload cannot is gone: this is an internal interface exported to
neither. So Porthole's input half cannot be built as designed by changing delivery route, and the
route question does not need answering for input at all.

**There is no user-space route to the composited pixels from a payload either**
(REQ-20260909T1021Z-af02). All five `sceVideoOut` entry points are null by every route, the video
sysmodule will not load for an unsigned payload, and `oops_display_*` reports itself unsupported
there because no GPU library is mapped. So the display code Porthole calls does nothing in the
place Porthole runs, and the startup log now says so rather than leaving a reader to infer it.

**But the same measurement names the way through, and Porthole already holds the key.** The
frame-grabber that has been serving on port 9022 all along does it with **kernel read/write**:
querying the display controller's registers and reading the scanout buffer out of physical
memory. That is not a user-space video API at all, which is why every probe for one came back
empty. Porthole receives the kernel read/write handles in its payload arguments and already
initialises them.

So the shape of the app changes. Encode in hardware is refused; inject a pad is unexported
everywhere; but *capture* is reachable by a route nobody had asked about, and the socket half is
built and now rides on the SDK. What Porthole becomes is a capture-and-serve payload over kernel
read/write, closer to `VIDEO.md` part two than part three, with the encoded-stream design parked
rather than pursued. The next question is the mechanics of that route, which is filed.

---

**amended** · 2026-09-09, later

Two more measurements, and they pull in opposite directions.

**The socket half is proven on hardware.** An unsigned payload opened a listener on a scratch
port, accepted a connection from off-console, read 22 bytes and echoed them back, and the host
confirmed receipt (obSCEne REQ-20260909T1315Z-04b6). That is the first time anything has served a
socket from a payload on this console. It also settles the worry raised when Porthole retired its
own socket layer for the SDK's: **all ten calls resolved by loader-bound weak reference**, so the
loader does bind them and the SDK's mechanism is sound where the private runtime resolver was.
The fallback that was going to be asked for is not needed, and was not filed.

**The capture half is characterised but still shut.** `/dev/dce` opens from a payload, and through
it the scanout buffers are at physical `0x4040200000` and `0x4042400000`
(REQ-20260909T1315Z-71dc). The surface is **3840x2160, stride 15360, linear
`B8G8R8A8_UNORM`** - not the 1920x1080 this app's encoder configuration assumes, and unpadded, so
a frame is 33 MB. What does not work is *reading* it: a kernel-read/write copy through the direct
map at `0xffff804040200000` returned -1, because that physical range sits outside the pipe's
direct-map window. Getting the pixels needs a mapping - a `/dev/dce` ioctl, or a page-table entry
- and that is the one thing still missing.

So the position is exact: **the pipe out works and the tap does not.** Porthole can serve, and has
nothing yet to serve but its template. Two consequences worth carrying forward. A 4K frame at 33 MB
is 2 GB/s at sixty frames, so even with a mapping, raw frames are not a stream and something has to
reduce them - which is a design problem, not a hardware one. And the flip counter and buffer index
both read zero, which may simply mean nothing was being composited while the probe ran; a capture
of a blank screen would prove very little, so that wants settling before any capture code is
trusted.
