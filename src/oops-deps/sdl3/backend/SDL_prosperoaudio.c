/*
 * SDL3's audio driver for this console, over `oops/audio.h`.
 *
 * One playback device. The port takes interleaved signed 16-bit stereo frames only, so
 * `OpenDevice` pins `SDL_AUDIO_S16LE` and two channels at the caller's rate, and SDL
 * converts on the caller's behalf.
 *
 * `oops_audio_write` blocks until the port has taken the frames, so the wait happens in
 * `PlayDevice` and `WaitDevice` returns at once. SDL owns the audio thread.
 *
 * `oops/audio.h` is output only, so there is no recording device.
 */
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PRIVATE

#include "audio/SDL_sysaudio.h"

#include "oops/audio.h"

#define PROSPEROAUDIO_DRIVER_NAME "prospero"

struct SDL_PrivateAudioData {
    oops_audio_port_t *port;
    Uint8 *mixbuf;
    int mixlen;
};

static void PROSPEROAUDIO_DetectDevices(SDL_AudioDevice **default_playback,
                                        SDL_AudioDevice **default_recording) {
    /* The one playback device, handed to SDL as the default. */
    *default_playback =
        SDL_AddAudioDevice(false, "Prospero audio out", NULL, (void *)0x1);
    (void)default_recording;
}

static bool PROSPEROAUDIO_OpenDevice(SDL_AudioDevice *device) {
    struct SDL_PrivateAudioData *h;

    if (device->recording) {
        /* Unreachable while `HasRecordingSupport` is false; refused regardless. */
        return SDL_SetError("prospero: this platform has no audio input");
    }

    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return false;
    }
    device->hidden = h;

    /* What the port accepts, set before `SDL_UpdatedAudioDeviceFormat` sizes buffers
     * from it. The frequency is the caller's. */
    device->spec.format = SDL_AUDIO_S16LE;
    device->spec.channels = 2;
    SDL_UpdatedAudioDeviceFormat(device);

    h->port = oops_audio_open(device->spec.freq, device->spec.channels,
                              device->sample_frames);
    if (!h->port) {
        return SDL_SetError("prospero: oops_audio_open() failed (%d)",
                            oops_audio_get_last_error());
    }

    /*
     * The port may round the chunk size; SDL's buffer is resized to match so every
     * write is whole chunks and the port holds no partial tail.
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

static Uint8 *PROSPEROAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size) {
    struct SDL_PrivateAudioData *h = device->hidden;

    *buffer_size = h->mixlen;
    return h->mixbuf;
}

static bool PROSPEROAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer,
                                     int buflen) {
    struct SDL_PrivateAudioData *h = device->hidden;
    /* The port counts frames and SDL counts bytes: two 16-bit channels, four bytes. */
    const size_t frames = (size_t)buflen / 4u;

    if (oops_audio_write(h->port, (const int16_t *)(const void *)buffer, frames) != 0) {
        return SDL_SetError("prospero: oops_audio_write() failed (%d)",
                            oops_audio_get_last_error());
    }
    return true;
}

static bool PROSPEROAUDIO_WaitDevice(SDL_AudioDevice *device) {
    /* The wait happened in `PlayDevice`; sleeping here as well would underrun. */
    (void)device;
    return true;
}

static void PROSPEROAUDIO_CloseDevice(SDL_AudioDevice *device) {
    struct SDL_PrivateAudioData *h = device->hidden;

    if (h) {
        if (h->port) {
            /* Flushed first, so a partial chunk the port holds is still played. */
            (void)oops_audio_flush(h->port);
            oops_audio_close(h->port);
        }
        SDL_free(h->mixbuf);
        SDL_free(h);
        device->hidden = NULL;
    }
}

static bool PROSPEROAUDIO_Init(SDL_AudioDriverImpl *impl) {
    impl->DetectDevices = PROSPEROAUDIO_DetectDevices;
    impl->OpenDevice = PROSPEROAUDIO_OpenDevice;
    impl->GetDeviceBuf = PROSPEROAUDIO_GetDeviceBuf;
    impl->PlayDevice = PROSPEROAUDIO_PlayDevice;
    impl->WaitDevice = PROSPEROAUDIO_WaitDevice;
    impl->CloseDevice = PROSPEROAUDIO_CloseDevice;

    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->HasRecordingSupport = false;       /* `oops/audio.h` is output only */
    impl->ProvidesOwnCallbackThread = false; /* SDL owns the audio thread */

    return true;
}

AudioBootStrap PRIVATEAUDIO_bootstrap = {
    PROSPEROAUDIO_DRIVER_NAME, "OOPS Prospero audio driver", PROSPEROAUDIO_Init,
    false, /* not demand-only */
    true   /* preferred */
};

#endif /* SDL_AUDIO_DRIVER_PRIVATE */
