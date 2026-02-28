/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "resources.h"
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CacheEntry* find_in_cache(CacheEntry *head, StimType type, const char *path) {
    CacheEntry *curr = head;
    while (curr) {
        if (curr->type == type && strcmp(curr->file_path, path) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

Resource *load_resources(SDL_Renderer *renderer, const Experiment *exp, TTF_Font *font, const char *base_path, CacheEntry **cache_out) {
    *cache_out = NULL;
    Resource *res = calloc(exp->count, sizeof(Resource));
    if (!res) return NULL;

    SDL_AudioSpec target_spec = { SDL_AUDIO_S16, 2, 44100 };

    for (int i = 0; i < exp->count; i++) {
        const Stimulus *s = &exp->stimuli[i];
        CacheEntry *entry = find_in_cache(*cache_out, s->type, s->file_path);

        if (entry) {
            res[i].texture = entry->texture; res[i].w = entry->w; res[i].h = entry->h; res[i].sound = entry->sound;
            continue;
        }

        entry = calloc(1, sizeof(CacheEntry));
        entry->type = s->type; strncpy(entry->file_path, s->file_path, 255);

        char full_path[1024]; snprintf(full_path, 1024, "%s%s", base_path, s->file_path);

        if (s->type == STIM_IMAGE) {
            entry->texture = IMG_LoadTexture(renderer, full_path);
            if (entry->texture) SDL_GetTextureSize(entry->texture, &entry->w, &entry->h);
            else SDL_Log("Failed to load image: %s", full_path);
        } else if (s->type == STIM_SOUND) {
            SDL_AudioSpec src_spec;
            Uint8 *src_data;
            Uint32 src_len;
            if (SDL_LoadWAV(full_path, &src_spec, &src_data, &src_len)) {
                if (src_spec.format == target_spec.format && src_spec.channels == target_spec.channels && src_spec.freq == target_spec.freq) {
                    entry->sound.spec = src_spec;
                    entry->sound.data = src_data;
                    entry->sound.len = src_len;
                } else {
                    /* Convert to target format */
                    Uint8 *dst_data;
                    int dst_len;
                    if (SDL_ConvertAudioSamples(&src_spec, src_data, src_len, &target_spec, &dst_data, &dst_len)) {
                        entry->sound.spec = target_spec;
                        entry->sound.data = dst_data;
                        entry->sound.len = (Uint32)dst_len;
                        SDL_free(src_data);
                    } else {
                        SDL_Log("Failed to convert sound %s: %s", full_path, SDL_GetError());
                        entry->sound.data = src_data; /* Fallback to original */
                        entry->sound.spec = src_spec;
                        entry->sound.len = src_len;
                    }
                }
            } else {
                SDL_Log("Failed to load sound: %s", full_path);
            }
        } else if (s->type == STIM_TEXT && font) {
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface *surf = TTF_RenderText_Blended(font, s->file_path, 0, white);
            if (surf) {
                entry->texture = SDL_CreateTextureFromSurface(renderer, surf);
                entry->w = (float)surf->w; entry->h = (float)surf->h;
                SDL_DestroySurface(surf);
            }
        }

        res[i].texture = entry->texture; res[i].w = entry->w; res[i].h = entry->h; res[i].sound = entry->sound;
        entry->next = *cache_out; *cache_out = entry;
    }
    return res;
}

void free_resources(Resource *resources, CacheEntry *cache) {
    CacheEntry *curr = cache;
    while (curr) {
        CacheEntry *next = curr->next;
        if (curr->texture) SDL_DestroyTexture(curr->texture);
        if (curr->sound.data) SDL_free(curr->sound.data);
        free(curr); curr = next;
    }
    free(resources);
}

static SDL_EnumerationResult find_font_callback(void *userdata, const char *dirname, const char *fname) {
    char *result = (char *)userdata;
    const char *ext = strrchr(fname, '.');
    if (ext && (SDL_strcasecmp(ext, ".ttf") == 0 || SDL_strcasecmp(ext, ".ttc") == 0)) {
        SDL_snprintf(result, 1024, "%s/%s", dirname, fname);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

const char* get_default_font_path(void) {
    static char local_font[1024]; local_font[0] = '\0';
    SDL_EnumerateDirectory("fonts", find_font_callback, local_font);
    if (local_font[0] != '\0') return local_font;
    static const char *const paths[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\arial.ttf",
#elif defined(__APPLE__)
        "/System/Library/Fonts/Helvetica.ttc",
#else
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
#endif
        NULL
    };
    for (int i = 0; paths[i] != NULL; ++i) {
        SDL_PathInfo info;
        if (SDL_GetPathInfo(paths[i], &info)) return paths[i];
    }
    return NULL;
}
