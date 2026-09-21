# SDL2_mixer

Audio mixing over SDL2, pinned at `release-2.8.2`. Extreme Tux Racer wants it; its surface there
is **14 functions** - `Mix_OpenAudio`, `Mix_LoadWAV`, `Mix_LoadMUS`, `Mix_PlayChannel`,
`Mix_PlayMusic`, the two volume calls, the two halts, and a handful more.

Sits on [`../sdl2`](../sdl2/) and its console backends, so the sound a title mixes here reaches
the hardware through `oops/audio.h` like everything else.

## Where it stands

**10 of 10 sources compile** for `x86_64-unknown-freebsd -ffreestanding`, first attempt, against
`oops-sdk`'s libc and no host headers. **`patches/` is empty.**

## WAV and Vorbis, and nothing else

Extreme Tux Racer's data directory ships **ten `.wav` and ten `.ogg`** and no other audio. So:

- **`MUSIC_WAV`** - SDL_mixer decodes WAV itself.
- **`MUSIC_OGG_STB`** - the bundled `stb_vorbis`, which decodes Ogg Vorbis with **no external
  libvorbis at all**. Upstream's own option, and one fewer pinned dependency for the same files.

MP3, FLAC, Opus, MIDI, MOD and Timidity are off. Not because they could not be made to work, but
because no title here has a file any of them would open - and a decoder nothing calls is still a
parser of untrusted input, still something to build, and still something to carry across a bump.

That `stb_vorbis` exists is the reason this dependency cost an afternoon rather than a week: the
obvious reading of "SDL_mixer needs Ogg" is that it needs libvorbis and libogg beneath it, and
upstream had already solved that for exactly this kind of build.

## Ordering

A title includes `oops-sdl.mk` **first** and this second - this file needs SDL2's headers on the
path and does not include that one itself, because a title wanting a mixer has already decided
which SDL it is using.
