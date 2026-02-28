/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "config.h"
#include "audio.h"
#include "resources.h"
#include "experiment.h"
#include "csv_parser.h"
#include "dlp.h"

int main(int argc, const char *argv[]) {
    /* ─── Capturing command line for logs ─── */
    char cmd_line[1024] = "";
    for (int i = 0; i < argc; i++) {
        strncat(cmd_line, argv[i], sizeof(cmd_line) - strlen(cmd_line) - 1);
        if (i < argc - 1) strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
    }

    /* ─── 1. Configuration ─── */
    Config cfg;
    if (!parse_args(argc, argv, &cfg)) return 0;

    /* ─── 2. Systems Initialization (Safe check) ─── */
    if (!(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO) & (SDL_INIT_VIDEO | SDL_INIT_AUDIO))) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return 1;
        }
    }
    
    SDL_Log("Audio driver: %s", SDL_GetCurrentAudioDriver());
    TTF_Init();

    /* ─── 3. Window & Renderer Setup ─── */
    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    SDL_DisplayID target_display = (cfg.display_index < num_displays) ? displays[cfg.display_index] : displays[0];
    SDL_free(displays);

    Uint32 window_flags = 0;
    if (cfg.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *window = SDL_CreateWindow("expe3000", cfg.screen_w, cfg.screen_h, window_flags);
    if (!window) return 1;

    if (cfg.fullscreen) {
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display), SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display));
    }
    SDL_HideCursor();

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) return 1;

    if (cfg.vsync) SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, cfg.screen_w, cfg.screen_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* ─── 4. Font & CSV ─── */
    display_splash(renderer, cfg.start_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor);

    TTF_Font *font = NULL;
    const char *font_path = cfg.font_file ? cfg.font_file : get_default_font_path();
    if (font_path) {
        font = TTF_OpenFont(font_path, (float)cfg.font_size);
        if (font) SDL_Log("Loaded font: %s", font_path);
        else SDL_Log("Failed to load font '%s': %s", font_path, SDL_GetError());
    }

    Experiment *exp = parse_csv(cfg.csv_file);
    if (!exp) return 1;

    /* ─── 5. Path resolution ─── */
    char base_path[1024] = "";
    if (cfg.stimuli_dir[0] != '\0') {
        strncpy(base_path, cfg.stimuli_dir, sizeof(base_path) - 1);
        size_t len = strlen(base_path);
        if (len > 0 && base_path[len-1] != '/' && base_path[len-1] != '\\') {
#ifdef _WIN32
            strncat(base_path, "\\", sizeof(base_path) - len - 1);
#else
            strncat(base_path, "/", sizeof(base_path) - len - 1);
#endif
        }
    }

    /* ─── 6. Audio Mixer & DLP ─── */
    AudioMixer mx;
    audio_mixer_init(&mx);
    SDL_AudioSpec target_spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *master_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &target_spec, audio_callback, &mx);
    
    if (master_stream) {
        SDL_Log("Audio stream created successfully (S16, 2 channels, 44100Hz)");
        SDL_ResumeAudioStreamDevice(master_stream);
    } else {
        SDL_Log("CRITICAL: Failed to create audio stream: %s", SDL_GetError());
    }

    dlp_io8g_t *dlp = cfg.dlp_device ? dlp_new(cfg.dlp_device, 9600) : NULL;
    if (dlp) {
        SDL_Log("DLP device opened: %s", cfg.dlp_device);
        dlp_unset(dlp, "12345678");
    }

    /* ─── 7. Load Resources ─── */
    SDL_Log("Loading resources...");
    CacheEntry *cache = NULL;
    Resource *resources = load_resources(renderer, exp, font, base_path, &cache);
    
    /* Stats */
    int ic = 0, sc = 0, tc = 0; size_t tm = 0;
    for (CacheEntry *curr = cache; curr; curr = curr->next) {
        if (curr->type == STIM_IMAGE || curr->type == STIM_TEXT) {
            if (curr->type == STIM_IMAGE) ic++; else tc++;
            if (curr->texture) { float tw, th; SDL_GetTextureSize(curr->texture, &tw, &th); tm += (size_t)(tw * th * 4); }
        } else if (curr->type == STIM_SOUND) { sc++; tm += curr->sound.len; }
    }
    SDL_Log("Resources loaded: %d images, %d sounds, %d text textures. Total: %.2f MB", ic, sc, tc, (double)tm / 1048576.0);

    /* ─── 8. Run Experiment ─── */
    EventLog log = {0};
    run_experiment(&cfg, exp, resources, renderer, &mx, &log, dlp, master_stream, font);
    printf("\n");

    /* ─── 9. Save Results ─── */
    FILE *rf = fopen(cfg.output_file, "w");
    if (rf) {
        time_t rt; time(&rt);
        fprintf(rf, "# expe3000 Results\n# Date: %s", ctime(&rt));
        fprintf(rf, "# Command Line: %s\n", cmd_line);
        fprintf(rf, "timestamp_ms,event_type,label\n");
        for (int i = 0; i < log.count; i++) {
            fprintf(rf, "%" PRIu64 ",%s,%s\n", log.entries[i].timestamp_ms, log.entries[i].type, log.entries[i].label);
        }
        fclose(rf);
        SDL_Log("Results saved to: %s", cfg.output_file);
    }

    /* ─── 10. Cleanup ─── */
    display_splash(renderer, cfg.end_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor);

    if (font) TTF_CloseFont(font);
    if (dlp) dlp_close(dlp);
    if (master_stream) SDL_DestroyAudioStream(master_stream);
    
    free_event_log(&log);
    free_resources(resources, cache);
    audio_mixer_destroy(&mx);
    free_experiment(exp);
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
