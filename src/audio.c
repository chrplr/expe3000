/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "audio.h"
#include <string.h>

void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    (void)total_amount;
    AudioMixer *mx = (AudioMixer *)userdata;
    int remaining = additional_amount;
    while (remaining > 0) {
        int chunk = (remaining > AUDIO_SCRATCH_BYTES) ? AUDIO_SCRATCH_BYTES : remaining;
        memset(mx->scratch, 0, chunk);
        SDL_LockMutex(mx->mutex);
        for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
            ActiveSound *s = &mx->slots[i];
            if (!s->active) continue;
            Uint32 sound_remaining = s->resource->len - s->play_pos;
            Uint32 to_mix = (chunk > (int)sound_remaining) ? sound_remaining : (Uint32)chunk;
            
            /* Since we convert everything to S16 Stereo on load, we can use a fixed format here */
            SDL_MixAudio(mx->scratch, s->resource->data + s->play_pos, SDL_AUDIO_S16, to_mix, 1.0f);
            
            s->play_pos += to_mix;
            if (s->play_pos >= s->resource->len) s->active = false;
        }
        SDL_UnlockMutex(mx->mutex);
        SDL_PutAudioStreamData(stream, mx->scratch, chunk);
        remaining -= chunk;
    }
}

void audio_mixer_init(AudioMixer *mx) {
    memset(mx, 0, sizeof(AudioMixer));
    mx->mutex = SDL_CreateMutex();
}

void audio_mixer_destroy(AudioMixer *mx) {
    if (mx->mutex) SDL_DestroyMutex(mx->mutex);
}
