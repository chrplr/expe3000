/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "gui_setup.h"
#include "resources.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <string.h>

static void SDLCALL file_dialog_callback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    char *target = (char *)userdata;
    if (filelist && filelist[0]) strncpy(target, filelist[0], 1023);
}

bool run_gui_setup(Config *cfg) {
    SDL_Window *window = SDL_CreateWindow("expe3000 Setup", 800, 750, 0);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    TTF_Font *gui_font = TTF_OpenFont(get_default_font_path(), 18);
    if (!gui_font) return false;

    bool setup_done = false;
    SDL_Event e;
    int focus_box = -1; // 0: csv, 1: stimuli_dir, 2: output
    
    struct { int w, h; const char *label; } res_options[] = {
        {800, 600, "800x600 (SVGA)"},
        {1024, 768, "1024x768 (XGA)"},
        {1366, 1024, "1366x1024 (SXGA-)"},
        {1920, 1080, "1920x1080 (FHD)"},
        {2560, 1440, "2560x1440 (QHD)"},
        {3840, 2160, "3840x2160 (4K UHD)"}
    };
    int selected_res = 3; // Default to 1080p
    
    /* Pre-select resolution based on cfg */
    for (int i = 0; i < 6; i++) {
        if (cfg->screen_w == res_options[i].w && cfg->screen_h == res_options[i].h) {
            selected_res = i;
            break;
        }
    }

    SDL_StartTextInput(window);

    while (!setup_done) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                SDL_StopTextInput(window);
                return false;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                float mx = e.button.x, my = e.button.y;
                if (mx >= 50 && mx <= 700 && my >= 50 && my <= 80) focus_box = 0;
                else if (mx >= 50 && mx <= 700 && my >= 120 && my <= 150) focus_box = 1;
                else if (mx >= 50 && mx <= 700 && my >= 190 && my <= 220) focus_box = 2;
                else focus_box = -1;

                if (mx >= 710 && mx <= 780) {
                    if (my >= 50 && my <= 80) {
                        SDL_DialogFileFilter filters[] = {{"CSV Files", "csv"}};
                        SDL_ShowOpenFileDialog(file_dialog_callback, cfg->csv_file, window, filters, 1, NULL, false);
                    } else if (my >= 120 && my <= 150) {
                        SDL_ShowOpenFolderDialog(file_dialog_callback, cfg->stimuli_dir, window, NULL, false);
                    } else if (my >= 190 && my <= 220) {
                        SDL_ShowSaveFileDialog(file_dialog_callback, cfg->output_file, window, NULL, 0, "results.csv");
                    }
                }
                for (int i = 0; i < 6; i++) {
                    if (mx >= 50 && mx <= 300 && my >= 260 + (float)i * 40 && my <= 290 + (float)i * 40) selected_res = i;
                }
                if (mx >= 50 && mx <= 300 && my >= 520 && my <= 550) cfg->use_fixation = !cfg->use_fixation;
                if (mx >= 50 && mx <= 300 && my >= 570 && my <= 600) cfg->fullscreen = !cfg->fullscreen;

                if (mx >= 350 && mx <= 450 && my >= 650 && my <= 690) {
                    if (cfg->csv_file[0] != '\0') {
                        cfg->screen_w = res_options[selected_res].w;
                        cfg->screen_h = res_options[selected_res].h;
                        save_config_cache(cfg);
                        setup_done = true;
                    }
                }
            }
            if (e.type == SDL_EVENT_TEXT_INPUT && focus_box != -1) {
                char *target = (focus_box == 0) ? cfg->csv_file : (focus_box == 1 ? cfg->stimuli_dir : cfg->output_file);
                strncat(target, e.text.text, 1023 - strlen(target));
            }
            if (e.type == SDL_EVENT_KEY_DOWN && focus_box != -1) {
                if (e.key.key == SDLK_BACKSPACE) {
                    char *target = (focus_box == 0) ? cfg->csv_file : (focus_box == 1 ? cfg->stimuli_dir : cfg->output_file);
                    size_t len = strlen(target);
                    if (len > 0) target[len - 1] = '\0';
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);
        SDL_Color black = {0, 0, 0, 255};

        const char *labels[] = {"Experiment CSV:", "Stimuli Directory:", "Output Results CSV:"};
        float label_y[] = {20, 90, 160};
        for (int i = 0; i < 3; i++) {
            SDL_Surface *s = TTF_RenderText_Blended(gui_font, labels[i], 0, black);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_FRect r = {50, label_y[i], (float)s->w, (float)s->h};
                SDL_RenderTexture(renderer, t, NULL, &r);
                SDL_DestroySurface(s); SDL_DestroyTexture(t);
            }
        }

        for (int i = 0; i < 3; i++) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_FRect box = {50, 50 + (float)i * 70, 650, 30}; SDL_RenderFillRect(renderer, &box);
            SDL_SetRenderDrawColor(renderer, (focus_box == i) ? 0 : 180, (focus_box == i) ? 120 : 180, 255, 255);
            SDL_RenderRect(renderer, &box);

            char *text = (i == 0) ? cfg->csv_file : (i == 1 ? cfg->stimuli_dir : cfg->output_file);
            if (text[0]) {
                SDL_Surface *s = TTF_RenderText_Blended(gui_font, text, 0, black);
                if (s) {
                    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                    SDL_FRect r = {55, 55 + (float)i * 70, (float)s->w, (float)s->h};
                    SDL_RenderTexture(renderer, t, NULL, &r);
                    SDL_DestroySurface(s); SDL_DestroyTexture(t);
                }
            }

            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_FRect btn = {710, 50 + (float)i * 70, 70, 30}; SDL_RenderFillRect(renderer, &btn);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderRect(renderer, &btn);
            SDL_Surface *sb = TTF_RenderText_Blended(gui_font, "...", 0, black);
            if (sb) {
                SDL_Texture *tb = SDL_CreateTextureFromSurface(renderer, sb);
                SDL_FRect rb = {735, 55 + (float)i * 70, (float)sb->w, (float)sb->h};
                SDL_RenderTexture(renderer, tb, NULL, &rb);
                SDL_DestroySurface(sb); SDL_DestroyTexture(tb);
            }
        }

        for (int i = 0; i < 6; i++) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_FRect check = {50, 260 + (float)i * 40, 20, 20};
            SDL_RenderFillRect(renderer, &check);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderRect(renderer, &check);
            if (selected_res == i) {
                SDL_FRect mark = {54, 264 + (float)i * 40, 12, 12};
                SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255);
                SDL_RenderFillRect(renderer, &mark);
            }
            SDL_Surface *s = TTF_RenderText_Blended(gui_font, res_options[i].label, 0, black);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
                SDL_FRect r = {80, 260 + (float)i * 40, (float)s->w, (float)s->h};
                SDL_RenderTexture(renderer, t, NULL, &r);
                SDL_DestroySurface(s); SDL_DestroyTexture(t);
            }
        }

        // Fixation checkbox
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect fix_check = {50, 520, 20, 20}; SDL_RenderFillRect(renderer, &fix_check);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderRect(renderer, &fix_check);
        if (cfg->use_fixation) {
            SDL_FRect mark = {54, 524, 12, 12};
            SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &mark);
        }
        SDL_Surface *s_fix = TTF_RenderText_Blended(gui_font, "Show fixation cross", 0, black);
        if (s_fix) {
            SDL_Texture *t_fix = SDL_CreateTextureFromSurface(renderer, s_fix);
            SDL_FRect r_fix = {80, 520, (float)s_fix->w, (float)s_fix->h};
            SDL_RenderTexture(renderer, t_fix, NULL, &r_fix);
            SDL_DestroySurface(s_fix); SDL_DestroyTexture(t_fix);
        }

        // Fullscreen checkbox
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_FRect full_check = {50, 570, 20, 20}; SDL_RenderFillRect(renderer, &full_check);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderRect(renderer, &full_check);
        if (cfg->fullscreen) {
            SDL_FRect mark = {54, 574, 12, 12};
            SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255); SDL_RenderFillRect(renderer, &mark);
        }
        SDL_Surface *s_full = TTF_RenderText_Blended(gui_font, "Fullscreen mode", 0, black);
        if (s_full) {
            SDL_Texture *t_full = SDL_CreateTextureFromSurface(renderer, s_full);
            SDL_FRect r_full = {80, 570, (float)s_full->w, (float)s_full->h};
            SDL_RenderTexture(renderer, t_full, NULL, &r_full);
            SDL_DestroySurface(s_full); SDL_DestroyTexture(t_full);
        }

        // Start button
        SDL_SetRenderDrawColor(renderer, 0, 150, 0, 255);
        SDL_FRect start_btn = {350, 650, 100, 40}; SDL_RenderFillRect(renderer, &start_btn);
        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *s_st = TTF_RenderText_Blended(gui_font, "START", 0, white);
        if (s_st) {
            SDL_Texture *t_st = SDL_CreateTextureFromSurface(renderer, s_st);
            SDL_FRect r_st = {375, 660, (float)s_st->w, (float)s_st->h};
            SDL_RenderTexture(renderer, t_st, NULL, &r_st);
            SDL_DestroySurface(s_st); SDL_DestroyTexture(t_st);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_StopTextInput(window);
    TTF_CloseFont(gui_font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return true;
}
