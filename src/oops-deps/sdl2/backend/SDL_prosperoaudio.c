/*
 * Audio output over `oops/audio.h`.
 *
 * # The device is a blocking write, which is the shape SDL likes best
 *
 * SDL runs its own audio thread, calls the title's callback to fill a buffer, then asks the
 * driver to hand over a buffer (`GetDeviceBuf`), play it (`PlayDevice`), and wait until there is
 * room for the next one (`WaitDevice`). `oops_audio_write` blocks until the chunk is queued, so
 * it *is* the wait - and `WaitDevice` is left unset rather than given an empty body, because a
 * sleep there would be a second, wrong, source of pacing on top of the first.
 *
 * # One format, and the caller is told so rather than humoured
 *
 * The port is 48 kHz, 16-bit signed, stereo. `OpenDevice` writes those three into `spec` and
 * lets `SDL_OpenAudioDevice`'s `allowed_changes` decide what happens next: a title that passed
 * the change flags gets a converter built for it by SDL, and one that insisted on 22 kHz mono
 * gets a failure it can read. Both are better than resampling silently in here.
 */
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_PROSPERO

#include "SDL_audio.h"
#include "SDL_timer.h"
#include "audio/SDL_audio_c.h"
#include "audio/SDL_sysaudio.h"

#include "oops/audio.h"

/*
 * `SDL_sysaudio.h` defines `_THIS` for its own struct declarations and then `#undef`s it again
 * on the way out, so every audio backend re-declares it - upstream's own `SDL_dummyaudio.h` does
 * this on its line 29. The video subsystem leaves its `_THIS` standing, which is why
 * `SDL_prosperovideo.c` needs no such line and this one does.
 */
#define _THIS SDL_AudioDevice *_this

#define PROSPERO_AUDIO_FREQ 48000
#define PROSPERO_AUDIO_CHANNELS 2

struct SDL_PrivateAudioData
{
    oops_audio_port_t *port;
    Uint8 *buffer;
    int buffer_bytes;
};

static int PROSPERO_AudioOpenDevice(_THIS, const char *devname)
{
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

    /* What the port is, written back before SDL_CalculateAudioSpec derives the rest. */
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
     * The port may have rounded the buffer to what the hardware grants, and SDL must be told:
     * `spec.samples` is the number of frames its callback is asked for, and a callback filling
     * 1024 frames into a buffer the device plays 256 of is how audio turns to noise.
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

static Uint8 *PROSPERO_AudioGetDeviceBuf(_THIS)
{
    return _this->hidden->buffer;
}

static void PROSPERO_AudioPlayDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = _this->hidden;
    size_t frames = (size_t)(h->buffer_bytes / (PROSPERO_AUDIO_CHANNELS * (int)sizeof(int16_t)));

    /*
     * A failed write is dropped rather than retried. SDL has no way to be told that a buffer did
     * not play, and a retry loop in the audio thread is how a device that has gone away turns
     * into a hang instead of silence.
     */
    (void)oops_audio_write(h->port, (const int16_t *)h->buffer, frames);
}

static void PROSPERO_AudioCloseDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = _this->hidden;

    if (!h) {
        return;
    }
    if (h->port) {
        oops_audio_flush(h->port); /* the held partial chunk, so the last sound is heard */
        oops_audio_close(h->port);
    }
    SDL_free(h->buffer);
    SDL_free(h);
    _this->hidden = NULL;
}

static SDL_bool PROSPERO_AudioInit(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = PROSPERO_AudioOpenDevice;
    impl->PlayDevice = PROSPERO_AudioPlayDevice;
    impl->GetDeviceBuf = PROSPERO_AudioGetDeviceBuf;
    impl->CloseDevice = PROSPERO_AudioCloseDevice;

    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->HasCaptureSupport = SDL_FALSE;

    /*
     * `WaitDevice` is deliberately not set: `oops_audio_write` blocks until the chunk is queued,
     * so the write is the wait. `ProvidesOwnCallbackThread` stays false - SDL's audio thread
     * drives this, which is what makes `SDL_LockAudio` mean anything.
     */
    return SDL_TRUE;
}

AudioBootStrap PROSPEROAUDIO_bootstrap = {
    "prospero", "OOPS console audio", PROSPERO_AudioInit, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_PROSPERO */
