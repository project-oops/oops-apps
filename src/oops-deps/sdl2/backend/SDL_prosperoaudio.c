/*
 * Audio output over `oops/audio.h`, driven by SDL's own audio thread.
 *
 * `oops_audio_write` blocks until the chunk is queued, so it is the wait and
 * `WaitDevice` is unset. The port is 48 kHz, 16-bit signed, stereo; `OpenDevice` writes
 * that into `spec`, and SDL's `allowed_changes` decides whether a converter is built or
 * the open fails.
 */
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PROSPERO

#include "SDL_audio.h"
#include "SDL_timer.h"
#include "audio/SDL_audio_c.h"
#include "audio/SDL_sysaudio.h"

#include "oops/audio.h"

/*
 * `SDL_sysaudio.h` `#undef`s its `_THIS`, so each audio backend re-declares it, as
 * upstream's `src/audio/dummy/SDL_dummyaudio.h:29` does.
 */
#define _THIS SDL_AudioDevice *_this

#define PROSPERO_AUDIO_FREQ 48000
#define PROSPERO_AUDIO_CHANNELS 2

struct SDL_PrivateAudioData {
    oops_audio_port_t *port;
    Uint8 *buffer;
    int buffer_bytes;
};

static int PROSPERO_AudioOpenDevice(_THIS, const char *devname) {
    struct SDL_PrivateAudioData *h;
    int frames;

    (void)devname;

    if (_this->iscapture) {
        return SDL_SetError("prospero: there is no audio capture here");
    }

    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return SDL_OutOfMemory();
    }
    _this->hidden = h;

    /* The port's format, set before SDL_CalculateAudioSpec derives the rest. */
    _this->spec.freq = PROSPERO_AUDIO_FREQ;
    _this->spec.format = AUDIO_S16SYS;
    _this->spec.channels = PROSPERO_AUDIO_CHANNELS;

    frames = (int)_this->spec.samples;
    if (frames <= 0) {
        frames = 1024;
    }

    h->port = oops_audio_open(PROSPERO_AUDIO_FREQ, PROSPERO_AUDIO_CHANNELS, frames);
    if (!h->port) {
        return SDL_SetError("prospero: the audio port did not open (%d)",
                            oops_audio_get_last_error());
    }

    /*
     * The port may round the chunk size; `spec.samples` must match it, since it is the
     * number of frames SDL's callback fills per buffer.
     */
    frames = oops_audio_get_chunk_frames(h->port);
    if (frames > 0) {
        _this->spec.samples = (Uint16)frames;
    }

    SDL_CalculateAudioSpec(&_this->spec);

    h->buffer_bytes = (int)_this->spec.size;
    h->buffer = (Uint8 *)SDL_malloc((size_t)h->buffer_bytes);
    if (!h->buffer) {
        return SDL_OutOfMemory();
    }
    SDL_memset(h->buffer, _this->spec.silence, (size_t)h->buffer_bytes);

    return 0;
}

static Uint8 *PROSPERO_AudioGetDeviceBuf(_THIS) {
    return _this->hidden->buffer;
}

static void PROSPERO_AudioPlayDevice(_THIS) {
    struct SDL_PrivateAudioData *h = _this->hidden;
    size_t frames =
        (size_t)(h->buffer_bytes / (PROSPERO_AUDIO_CHANNELS * (int)sizeof(int16_t)));

    /*
     * A failed write is dropped, not retried: SDL cannot be told a buffer did not play,
     * and a retry loop would hang the audio thread on a device that has gone away.
     */
    (void)oops_audio_write(h->port, (const int16_t *)h->buffer, frames);
}

static void PROSPERO_AudioCloseDevice(_THIS) {
    struct SDL_PrivateAudioData *h = _this->hidden;

    if (!h) {
        return;
    }
    if (h->port) {
        oops_audio_flush(
            h->port); /* the held partial chunk, so the last sound is heard */
        oops_audio_close(h->port);
    }
    SDL_free(h->buffer);
    SDL_free(h);
    _this->hidden = NULL;
}

static SDL_bool PROSPERO_AudioInit(SDL_AudioDriverImpl *impl) {
    impl->OpenDevice = PROSPERO_AudioOpenDevice;
    impl->PlayDevice = PROSPERO_AudioPlayDevice;
    impl->GetDeviceBuf = PROSPERO_AudioGetDeviceBuf;
    impl->CloseDevice = PROSPERO_AudioCloseDevice;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->HasCaptureSupport = SDL_FALSE;

    /*
     * `WaitDevice` is unset because the write blocks. `ProvidesOwnCallbackThread` stays
     * false so SDL's audio thread drives the device and `SDL_LockAudio` applies.
     */
    return SDL_TRUE;
}

AudioBootStrap PROSPEROAUDIO_bootstrap = {"prospero", "OOPS console audio",
                                          PROSPERO_AudioInit, SDL_FALSE};

#endif /* SDL_AUDIO_DRIVER_PROSPERO */
