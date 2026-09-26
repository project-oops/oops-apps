# SDL2_mixer

Audio mixing over SDL2, pinned at `release-2.8.2`. Extreme Tux Racer uses it: `Mix_OpenAudio`,
`Mix_LoadWAV`, `Mix_LoadMUS`, `Mix_PlayChannel`, `Mix_PlayMusic`, the volume calls, the halts,
and a few more.

It sits on [`../sdl2`](../sdl2/) and its backends, so the sound a title mixes here reaches the
hardware through `oops/audio.h`. It compiles for `x86_64-unknown-freebsd -ffreestanding` against
`oops-sdk`'s libc and no host headers. `patches/` is empty.

## Formats

Extreme Tux Racer's data directory ships `.wav` and `.ogg` and no other audio, so:

- **`MUSIC_WAV`** - SDL_mixer decodes WAV itself.
- **`MUSIC_OGG_STB`** - the bundled `stb_vorbis`, which decodes Ogg Vorbis with no external
  libvorbis. Upstream's own option, and one fewer pinned dependency for the same files.

MP3, FLAC, Opus, MIDI, MOD and Timidity are off. No title here has a file any of them would
open, and a decoder nothing calls is still a parser of untrusted input to build and carry across
a bump.

## Ordering

A title includes `oops-sdl.mk` first and this second - this file needs SDL2's headers on the
path and does not include that one itself, because a title wanting a mixer has already chosen
its SDL.
