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
#ifndef _WIN32
#include <unistd.h>
#endif

#include "config.h"
#include "audio.h"
#include "resources.h"
#include "experiment.h"
#include "csv_parser.h"
#include "dlp.h"
#include "version.h"

#if defined(__clang__)
#define COMPILER_NAME "Clang"
#elif defined(__GNUC__)
#define COMPILER_NAME "GCC"
#elif defined(_MSC_VER)
#define COMPILER_NAME "MSVC"
#else
#define COMPILER_NAME "Unknown Compiler"
#endif

int main(int argc, const char *argv[]) {
    /* ─── Capturing command line for logs ─── */
    char cmd_line[1024] = "";
    for (int i = 0; i < argc; i++) {
        strncat(cmd_line, argv[i], sizeof(cmd_line) - strlen(cmd_line) - 1);
        if (i < argc - 1) strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
    }

    SDL_Log("expe3000 version: %s (compiled: %s %s)", EXPE3000_VERSION, __DATE__, __TIME__);
    SDL_Log("Compiler: %s %s", COMPILER_NAME, __VERSION__);
    SDL_Log("Author: Christophe Pallier (christophe@pallier.org)");
    SDL_Log("GitHub: https://github.com/chrplr/expe3000");

    /* ─── 1. Configuration ─── */
    Config cfg;
    EventLog log = {0};
    if (!parse_args(argc, argv, &cfg)) {
        printf("Usage: expe3000 <stimuli_csv_file> [options]\n");
        return 0;
    }

    /* ─── 2. Systems Initialization (Safe check) ─── */
    if (!(SDL_WasInit(SDL_INIT_VIDEO | SDL_INIT_AUDIO) & (SDL_INIT_VIDEO | SDL_INIT_AUDIO))) {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
            return 1;
        }
    }
    
    SDL_Log("Audio driver: %s", SDL_GetCurrentAudioDriver());
    SDL_Log("Video driver: %s", SDL_GetCurrentVideoDriver());
    int sdl_version = SDL_GetVersion();
    SDL_Log("SDL version: %d.%d.%d", SDL_VERSIONNUM_MAJOR(sdl_version), SDL_VERSIONNUM_MINOR(sdl_version), SDL_VERSIONNUM_MICRO(sdl_version));
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    /* ─── 3. Window & Renderer Setup ─── */
    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    if (!displays) {
        fprintf(stderr, "SDL_GetDisplays Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_DisplayID target_display = (cfg.display_index < num_displays) ? displays[cfg.display_index] : displays[0];
    SDL_free(displays);

    Uint32 window_flags = 0;
    SDL_Window *window = SDL_CreateWindow("expe3000", cfg.screen_w, cfg.screen_h, window_flags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    if (cfg.fullscreen) {
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display), SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display));
        SDL_SetWindowFullscreen(window, true);
    }
    SDL_HideCursor();

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Log("Renderer: %s", SDL_GetRendererName(renderer));

    if (cfg.vsync) SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, cfg.screen_w, cfg.screen_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(target_display);
    if (mode) {
        SDL_Log("Display: %dx%d @ %.2fHz (Physical)", mode->w, mode->h, mode->refresh_rate);
    }
    SDL_Log("Logical Resolution: %dx%d (Letterbox)", cfg.screen_w, cfg.screen_h);

    /* ─── 4. Font & CSV ─── */
    display_splash(renderer, cfg.start_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor, cfg.bg_color);

    TTF_Font *font = NULL;
    const char *font_path = cfg.font_file ? cfg.font_file : get_default_font_path();
    if (font_path) {
        font = TTF_OpenFont(font_path, (float)cfg.font_size);
        if (font) SDL_Log("Loaded font: %s", font_path);
        else SDL_Log("Failed to load font '%s': %s", font_path, SDL_GetError());
    }

    Experiment *exp = parse_csv(cfg.csv_file);
    if (!exp) {
        fprintf(stderr, "Error: Failed to parse experiment CSV file: %s\n", cfg.csv_file);
        return 1;
    }

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
    Resource *resources = load_resources(renderer, exp, font, cfg.text_color, base_path, &cache);
    
    /* Stats */
    int ic = 0, sc = 0, tc = 0; size_t tm = 0;
    int missing_count = 0;
    for (CacheEntry *curr = cache; curr; curr = curr->next) {
        if (curr->type == STIM_IMAGE || curr->type == STIM_TEXT) {
            if (curr->texture) {
                if (curr->type == STIM_IMAGE) ic++; else tc++;
                float tw, th; SDL_GetTextureSize(curr->texture, &tw, &th);
                tm += (size_t)(tw * th * 4);
            } else {
                missing_count++;
            }
        } else if (curr->type == STIM_SOUND) {
            if (curr->sound.data) {
                sc++;
                tm += curr->sound.len;
            } else {
                missing_count++;
            }
        }
    }

    if (missing_count > 0) {
        SDL_Log("WARNING: %d resources failed to load.", missing_count);
        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Quit" },
            { 0, 1, "Continue" },
        };
        const SDL_MessageBoxData messageboxdata = {
            SDL_MESSAGEBOX_WARNING,
            window,
            "Resource Loading Failure",
            "Some resources failed to load. Do you want to continue anyway?",
            SDL_arraysize(buttons),
            buttons,
            NULL
        };
        int buttonid;
        if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0 || buttonid == 0) {
            SDL_Log("User chose to quit due to missing resources.");
            goto cleanup;
        }
        SDL_Log("User chose to continue despite missing resources.");
    }

    SDL_Log("Resources loaded: %d images, %d sounds, %d text textures. Total: %.2f MB", ic, sc, tc, (double)tm / 1048576.0);

    /* ─── 8. Run Experiment ─── */
    time_t start_time = time(NULL);
    bool completed = run_experiment(&cfg, exp, resources, renderer, &mx, &log, dlp, master_stream, font);
    time_t end_time = time(NULL);
    printf("\n");

    /* ─── 9. Save Results ─── */
    FILE *rf = fopen(cfg.output_file, "w");
    if (rf) {
        fprintf(rf, "# expe3000 version: %s (compiled: %s %s)\n", EXPE3000_VERSION, __DATE__, __TIME__);
        fprintf(rf, "# Author: Christophe Pallier (christophe@pallier.org)\n");
        fprintf(rf, "# GitHub: https://github.com/chrplr/expe3000\n");
        fprintf(rf, "# Compiler: %s %s\n", COMPILER_NAME, __VERSION__);
        int sdl_v = SDL_GetVersion();
        fprintf(rf, "# SDL Version: %d.%d.%d\n", SDL_VERSIONNUM_MAJOR(sdl_v), SDL_VERSIONNUM_MINOR(sdl_v), SDL_VERSIONNUM_MICRO(sdl_v));
        fprintf(rf, "# Platform: %s\n", SDL_GetPlatform());
        
        char hostname[256] = "unknown";
#ifdef _WIN32
        if (getenv("COMPUTERNAME")) strncpy(hostname, getenv("COMPUTERNAME"), 255);
#else
        if (gethostname(hostname, 255) != 0) {
            if (getenv("HOSTNAME")) strncpy(hostname, getenv("HOSTNAME"), 255);
            else if (getenv("HOST")) strncpy(hostname, getenv("HOST"), 255);
        }
#endif
        fprintf(rf, "# Hostname: %s\n", hostname);

#ifdef _WIN32
        fprintf(rf, "# Username: %s\n", getenv("USERNAME") ? getenv("USERNAME") : "unknown");
#else
        fprintf(rf, "# Username: %s\n", getenv("USER") ? getenv("USER") : (getenv("LOGNAME") ? getenv("LOGNAME") : "unknown"));
#endif
        fprintf(rf, "# Video Driver: %s\n", SDL_GetCurrentVideoDriver());
        fprintf(rf, "# Audio Driver: %s\n", SDL_GetCurrentAudioDriver());
        fprintf(rf, "# Renderer: %s\n", SDL_GetRendererName(renderer));
        const SDL_DisplayMode *dm = SDL_GetCurrentDisplayMode(target_display);
        if (dm) {
            fprintf(rf, "# Display Mode: %dx%d @ %.2fHz (Physical)\n", dm->w, dm->h, dm->refresh_rate);
        }
        fprintf(rf, "# Logical Resolution: %dx%d\n", cfg.screen_w, cfg.screen_h);
        fprintf(rf, "# Font: %s\n", font_path ? font_path : "none");
        fprintf(rf, "# Font Size: %d\n", cfg.font_size);
        fprintf(rf, "# Background Color: %d,%d,%d\n", cfg.bg_color.r, cfg.bg_color.g, cfg.bg_color.b);
        fprintf(rf, "# Text Color: %d,%d,%d\n", cfg.text_color.r, cfg.text_color.g, cfg.text_color.b);
        fprintf(rf, "# Fixation Color: %d,%d,%d\n", cfg.fixation_color.r, cfg.fixation_color.g, cfg.fixation_color.b);
        fprintf(rf, "# Start Date: %s", ctime(&start_time));
        fprintf(rf, "# End Date: %s", ctime(&end_time));
        fprintf(rf, "# Completion Status: %s\n", completed ? "Completed Normally" : "Aborted (ESC or Quit)");
        fprintf(rf, "# Command Line: %s\n", cmd_line);
        fprintf(rf, "intended_ms,timestamp_ms,event_type,label\n");
        for (int i = 0; i < log.count; i++) {
            fprintf(rf, "%" PRIu64 ",%" PRIu64 ",%s,%s\n", log.entries[i].intended_ms, log.entries[i].timestamp_ms, log.entries[i].type, log.entries[i].label);
        }
        fclose(rf);
        SDL_Log("Results saved to: %s", cfg.output_file);
    } else {
        fprintf(stderr, "Error: Could not open results file for writing: %s\n", cfg.output_file);
    }

    /* ─── 10. Cleanup ─── */
    display_splash(renderer, cfg.end_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor, cfg.bg_color);

cleanup:
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
