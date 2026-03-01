/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef RESOURCES_H
#define RESOURCES_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "stimuli.h"
#include "audio.h"

typedef struct {
    SDL_Texture  *texture;
    float         w, h;
    SoundResource sound;
} Resource;

typedef struct CacheEntry {
    StimType type;
    char     file_path[256];
    SDL_Texture *texture;
    float w, h;
    SoundResource sound;
    struct CacheEntry *next;
} CacheEntry;

/**
 * @brief Loads all resources defined in an experiment.
 */
Resource *load_resources(SDL_Renderer *renderer, const Experiment *exp, TTF_Font *font, SDL_Color text_color, const char *base_path, CacheEntry **cache_out);

/**
 * @brief Frees all allocated resources and the cache.
 */
void free_resources(Resource *resources, CacheEntry *cache);

/**
 * @brief Automatically finds a default font on the system.
 */
const char* get_default_font_path(void);

#endif // RESOURCES_H
