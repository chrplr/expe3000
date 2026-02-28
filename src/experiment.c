/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "experiment.h"
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CROSS_SIZE 20

bool log_event(EventLog *log, Uint64 intended_ms, Uint64 actual_ms, const char *type, const char *label) {
    if (log->count >= log->capacity) {
        int new_cap = log->capacity == 0 ? 64 : log->capacity * 2;
        EventLogEntry *tmp = realloc(log->entries, new_cap * sizeof(EventLogEntry));
        if (!tmp) return false;
        log->entries  = tmp;
        log->capacity = new_cap;
    }
    EventLogEntry *e = &log->entries[log->count];
    e->intended_ms = intended_ms;
    e->timestamp_ms = actual_ms;
    strncpy(e->type,  type,  sizeof(e->type)  - 1); e->type[sizeof(e->type)   - 1] = '\0';
    strncpy(e->label, label, sizeof(e->label) - 1); e->label[sizeof(e->label) - 1] = '\0';
    log->count++;
    return true;
}

void free_event_log(EventLog *log) {
    if (log->entries) free(log->entries);
    log->entries = NULL; log->count = 0; log->capacity = 0;
}

static void draw_fixation_cross(SDL_Renderer *renderer, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float mx = (float)(w / 2), my = (float)(h / 2);
    SDL_RenderLine(renderer, mx - CROSS_SIZE, my, mx + CROSS_SIZE, my);
    SDL_RenderLine(renderer, mx, my - CROSS_SIZE, mx, my + CROSS_SIZE);
}

bool display_splash(SDL_Renderer *renderer, const char *file_path, int screen_w, int screen_h, float scale_factor, SDL_Color bg_color) {
    if (!file_path) return true;
    SDL_Texture *tex = IMG_LoadTexture(renderer, file_path);
    if (!tex) return true;
    float tw, th; SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = {(screen_w - tw * scale_factor) / 2.0f, (screen_h - th * scale_factor) / 2.0f, tw * scale_factor, th * scale_factor};
    SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, tex, NULL, &dst);
    SDL_RenderPresent(renderer);
    bool quit = false; SDL_Event event;
    while (true) {
        if (!SDL_WaitEvent(&event)) break;
        if (event.type == SDL_EVENT_QUIT) { quit = true; break; }
        if (event.type == SDL_EVENT_KEY_DOWN) break;
    }
    SDL_DestroyTexture(tex);
    return !quit;
}

bool run_experiment(Config *cfg, Experiment *exp, Resource *resources, 
                    SDL_Renderer *rend, AudioMixer *mx, EventLog *log, 
                    dlp_io8g_t *dlp, SDL_AudioStream *ms, TTF_Font *fnt) {
    (void)fnt;
    (void)ms;
    float rr = 60.0f;
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(SDL_GetRenderWindow(rend)));
    if (mode && mode->refresh_rate > 0) rr = mode->refresh_rate;
    Uint64 fd_ms = (Uint64)(1000.0f / rr);
    Uint64 la_ms = fd_ms / 2;

    bool run = true; bool aborted = false; SDL_Event ev; Uint64 st_ticks = SDL_GetTicks();
    int cs = 0, avi = -1; Uint64 vet = 0;

    while (run) {
        Uint64 ct = SDL_GetTicks() - st_ticks;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) { run = false; aborted = true; }
            else if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) { run = false; aborted = true; }
                else log_event(log, ct, ct, "RESPONSE", SDL_GetKeyName(ev.key.key));
            }
        }

        bool trig = false; int tidx = -1;
        if (cs < exp->count && (ct + la_ms) >= exp->stimuli[cs].timestamp_ms) {
            Stimulus *s = &exp->stimuli[cs];
            if ((s->type == STIM_IMAGE || s->type == STIM_TEXT) && resources[cs].texture) {
                avi = cs; trig = true; tidx = cs;
                vet = ct + s->duration_ms;
                if (dlp) dlp_set(dlp, s->type == STIM_IMAGE ? "1" : "3");
            } else if (s->type == STIM_SOUND && resources[cs].sound.data) {
                SDL_LockMutex(mx->mutex);
                for (int j = 0; j < MAX_ACTIVE_SOUNDS; j++) {
                    if (!mx->slots[j].active) {
                        mx->slots[j].resource = &resources[cs].sound; mx->slots[j].play_pos = 0; mx->slots[j].active = true;
                        log_event(log, s->timestamp_ms, ct, "SOUND_ONSET", s->file_path);
                        if (dlp) { dlp_set(dlp, "2"); SDL_Delay(5); dlp_unset(dlp, "2"); }
                        break;
                    }
                }
                SDL_UnlockMutex(mx->mutex);
            }
            cs++; fprintf(stdout, "\rStimulus: %d/%d ", cs, exp->count); fflush(stdout);
        }

        if (avi != -1 && ct >= vet) {
            Uint64 intended_off = exp->stimuli[avi].timestamp_ms + exp->stimuli[avi].duration_ms;
            log_event(log, intended_off, ct, exp->stimuli[avi].type == STIM_IMAGE ? "IMAGE_OFFSET" : "TEXT_OFFSET", exp->stimuli[avi].file_path);
            if (dlp) dlp_unset(dlp, exp->stimuli[avi].type == STIM_IMAGE ? "1" : "3");
            avi = -1;
        }

        if (cs >= exp->count && avi == -1 && ct >= cfg->total_duration) run = false;

        SDL_SetRenderDrawColor(rend, cfg->bg_color.r, cfg->bg_color.g, cfg->bg_color.b, cfg->bg_color.a); 
        SDL_RenderClear(rend);
        if (avi != -1) {
            Resource *r = &resources[avi];
            SDL_FRect dr = {(cfg->screen_w - (r->w * cfg->scale_factor)) / 2.0f, (cfg->screen_h - (r->h * cfg->scale_factor)) / 2.0f, r->w * cfg->scale_factor, r->h * cfg->scale_factor};
            SDL_RenderTexture(rend, r->texture, NULL, &dr);
        } else if (cfg->use_fixation) draw_fixation_cross(rend, cfg->screen_w, cfg->screen_h, cfg->fixation_color);
        SDL_RenderPresent(rend);

        if (trig) {
            Uint64 ot = SDL_GetTicks() - st_ticks;
            log_event(log, exp->stimuli[tidx].timestamp_ms, ot, exp->stimuli[tidx].type == STIM_IMAGE ? "IMAGE_ONSET" : "TEXT_ONSET", exp->stimuli[tidx].file_path);
            vet = ot + exp->stimuli[tidx].duration_ms;
        }
        if (!cfg->vsync) SDL_Delay(1);
    }
    return !aborted;
}
