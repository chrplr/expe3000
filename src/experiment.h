/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef EXPERIMENT_H
#define EXPERIMENT_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "config.h"
#include "stimuli.h"
#include "resources.h"
#include "audio.h"
#include "dlp.h"

typedef struct {
    Uint64 intended_ms;
    Uint64 timestamp_ms;
    char   type[16];
    char   label[256];
} EventLogEntry;

typedef struct {
    EventLogEntry *entries;
    int            count;
    int            capacity;
} EventLog;

/**
 * @brief Logs an event with intended and actual timestamps.
 */
bool log_event(EventLog *log, Uint64 intended_ms, Uint64 actual_ms, const char *type, const char *label);

/**
 * @brief Frees the event log memory.
 */
void free_event_log(EventLog *log);

/**
 * @brief Core experiment loop.
 */
bool run_experiment(Config *cfg, Experiment *exp, Resource *resources, 
                    SDL_Renderer *rend, AudioMixer *mx, EventLog *log, 
                    dlp_io8g_t *dlp, SDL_AudioStream *ms, TTF_Font *fnt);

/**
 * @brief Displays a splash screen and waits for a keypress.
 */
bool display_splash(SDL_Renderer *renderer, const char *file_path, int screen_w, int screen_h, float scale_factor, SDL_Color bg_color);

#endif // EXPERIMENT_H
