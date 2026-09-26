/*
 * SDL3's audio driver for this console, over `oops/audio.h`.
 *
 * # One device, one format, and SDL is told so rather than left to find out
 *
 * `oops_audio_open` gives a playback port that takes **interleaved signed 16-bit stereo frames**
 * and nothing else. SDL asks a backend to fill in the format it actually got, and then converts on
 * the caller's behalf, so stating the truth in `OpenDevice` costs nothing and getting it wrong
 * costs everything: a device left claiming float32 would have SDL hand over float samples that the
 * port reads as 16-bit integers, which is noise at full volume into somebody's television.
 *
 * So the format is pinned to `SDL_AUDIO_S16LE` and two channels, the sample rate is whatever the
 * caller asked for, and `SDL_UpdatedAudioDeviceFormat` recomputes the buffer size and the silence
 * value from that.
 *
 * # `WaitDevice` does nothing, and that is correct rather than lazy
 *
 * SDL's audio thread is a loop: wait until the device wants data, get a buffer, fill it, play it.
 * `oops_audio_write` **blocks** until the port has taken the frames, so the waiting already
 * happens inside `PlayDevice` and a `WaitDevice` that slept as well would halve the throughput and
 * underrun. SDL's own blocking backends do the same thing; the flag that matters is
 * `ProvidesOwnCallbackThread = false`, which says SDL still owns the loop.
 *
 * # Recording is absent, not stubbed
 *
 * `HasRecordingSupport` is false. `oops/audio.h` is output only - there is no `oops_audio_read` -
 * so SDL will not offer a recording device at all, which is better than offering one that returns
 * silence and has a program wondering why its microphone is dead.
 */
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PRIVATE

#include "audio/SDL_sysaudio.h"

#include "oops/audio.h"

#define PROSPEROAUDIO_DRIVER_NAME "prospero"

struct SDL_PrivateAudioData
{
    oops_audio_port_t *port;
    Uint8 *mixbuf;
    int mixlen;
};

static void PROSPEROAUDIO_DetectDevices(SDL_AudioDevice **default_playback,
                                        SDL_AudioDevice **default_recording)
{
    /* One playback device and no recording one. `OnlyHasDefaultPlaybackDevice` below already tells
     * SDL there is nothing to enumerate; this hands it the default. */
    *default_playback = SDL_AddAudioDevice(false, "Prospero audio out", NULL, (void *)0x1);
    (void)default_recording;
}

static bool PROSPEROAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h;

    if (device->recording) {
        /* Cannot be reached while `HasRecordingSupport` is false, and refused rather than trusted
         * to stay unreachable. */
        return SDL_SetError("prospero: this platform has no audio input");
    }

    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return false;
    }
    device->hidden = h;

    /* **What the port actually accepts**, before `SDL_UpdatedAudioDeviceFormat` sizes anything
     * from it. The frequency is the caller's; the format and the channel count are not
     * negotiable. */
    device->spec.format = SDL_AUDIO_S16LE;
    device->spec.channels = 2;
    SDL_UpdatedAudioDeviceFormat(device);

    h->port = oops_audio_open(device->spec.freq, device->spec.channels, device->sample_frames);
    if (!h->port) {
        return SDL_SetError("prospero: oops_audio_open() failed (%d)", oops_audio_get_last_error());
    }

    /*
     * **The port may round the buffer up, and SDL has to be told.** `oops_audio_get_chunk_frames`
     * reports the frames per hardware chunk after rounding, and writing a multiple of it is what
     * leaves no tail. If that differs from what SDL asked for, taking SDL's number anyway would
     * mean every write carried a partial chunk the port had to hold - a steady drift between what
     * SDL thinks it has queued and what has actually been heard.
     */
    {
        const int chunk = oops_audio_get_chunk_frames(h->port);
        if (chunk > 0 && chunk != device->sample_frames) {
            device->sample_frames = chunk;
            SDL_UpdatedAudioDeviceFormat(device);
        }
    }

    h->mixlen = device->buffer_size;
    h->mixbuf = (Uint8 *)SDL_malloc((size_t)h->mixlen);
    if (!h->mixbuf) {
        return false;
    }
    SDL_memset(h->mixbuf, device->silence_value, (size_t)h->mixlen);
    return true;
}

static Uint8 *PROSPEROAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    struct SDL_PrivateAudioData *h = device->hidden;

    *buffer_size = h->mixlen;
    return h->mixbuf;
}

static bool PROSPEROAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    /* Two channels of 16-bit samples: four bytes a frame. `SDL_AUDIO_FRAMESIZE` would say the same
     * thing from the spec, and this is the one place it is worth being explicit about, because the
     * port counts frames and SDL counts bytes. */
    const size_t frames = (size_t)buflen / 4u;

    if (oops_audio_write(h->port, (const int16_t *)(const void *)buffer, frames) != 0) {
        return SDL_SetError("prospero: oops_audio_write() failed (%d)",
                            oops_audio_get_last_error());
    }
    return true;
}

static bool PROSPEROAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    /* Nothing to wait for: `oops_audio_write` blocks until the port has taken the frames, so the
     * wait already happened in `PlayDevice`. See the note at the top - sleeping here as well
     * would underrun. */
    (void)device;
    return true;
}

static void PROSPEROAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h = device->hidden;

    if (h) {
        if (h->port) {
            /* **Flushed before closing.** A partial chunk the port is holding is audio the program
             * asked to play; closing without this drops the tail of whatever was last written,
             * which is audible at the end of a sound effect. */
            (void)oops_audio_flush(h->port);
            oops_audio_close(h->port);
        }
        SDL_free(h->mixbuf);
        SDL_free(h);
        device->hidden = NULL;
    }
}

static bool PROSPEROAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->DetectDevices = PROSPEROAUDIO_DetectDevices;
    impl->OpenDevice = PROSPEROAUDIO_OpenDevice;
    impl->GetDeviceBuf = PROSPEROAUDIO_GetDeviceBuf;
    impl->PlayDevice = PROSPEROAUDIO_PlayDevice;
    impl->WaitDevice = PROSPEROAUDIO_WaitDevice;
    impl->CloseDevice = PROSPEROAUDIO_CloseDevice;

    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->HasRecordingSupport = false;   /* output only; see the note at the top */
    impl->ProvidesOwnCallbackThread = false; /* SDL owns the audio thread, and should */

    return true;
}

AudioBootStrap PRIVATEAUDIO_bootstrap = {
    PROSPEROAUDIO_DRIVER_NAME, "OOPS Prospero audio driver",
    PROSPEROAUDIO_Init,
    false, /* not demand-only: this is the one that works here */
    true   /* preferred */
};

#endif /* SDL_AUDIO_DRIVER_PRIVATE */
