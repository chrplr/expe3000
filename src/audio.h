/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <SDL3/SDL.h>

#define MAX_ACTIVE_SOUNDS   16
#define AUDIO_SCRATCH_BYTES 4096

typedef struct {
    Uint8        *data;
    Uint32        len;
    SDL_AudioSpec spec;
} SoundResource;

typedef struct {
    const SoundResource *resource;
    Uint32               play_pos;
    bool                 active;
} ActiveSound;

typedef struct {
    ActiveSound  slots[MAX_ACTIVE_SOUNDS];
    SDL_Mutex   *mutex;
    Uint8        scratch[AUDIO_SCRATCH_BYTES];
} AudioMixer;

/**
 * @brief SDL Audio Callback function.
 */
void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount);

/**
 * @brief Initializes the audio mixer.
 */
void audio_mixer_init(AudioMixer *mx);

/**
 * @brief Cleans up the audio mixer resources.
 */
void audio_mixer_destroy(AudioMixer *mx);

#endif // AUDIO_H
