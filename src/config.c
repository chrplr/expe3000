/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"
#include "argparse.h"
#include "version.h"
#include "gui_setup.h"
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CACHE_FILE ".expe3000_cache"

static const char *const usage_lines[] = {
    "expe3000 <stimuli_csv_file> [options]",
    NULL,
};

void save_config_cache(const Config *cfg) {
    FILE *f = fopen(CACHE_FILE, "w");
    if (!f) return;
    fprintf(f, "csv_file=%s\n", cfg->csv_file);
    fprintf(f, "output_file=%s\n", cfg->output_file);
    fprintf(f, "stimuli_dir=%s\n", cfg->stimuli_dir);
    fprintf(f, "screen_w=%d\n", cfg->screen_w);
    fprintf(f, "screen_h=%d\n", cfg->screen_h);
    fprintf(f, "use_fixation=%d\n", cfg->use_fixation ? 1 : 0);
    fprintf(f, "fullscreen=%d\n", cfg->fullscreen ? 1 : 0);
    fclose(f);
}

void load_config_cache(Config *cfg) {
    FILE *f = fopen(CACHE_FILE, "r");
    if (!f) return;
    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        char *val = strchr(line, '=');
        if (!val) continue;
        *val = '\0'; val++;
        char *nl = strchr(val, '\n'); if (nl) *nl = '\0';
        char *rl = strchr(val, '\r'); if (rl) *rl = '\0';

        if (strcmp(line, "csv_file") == 0) strncpy(cfg->csv_file, val, 1023);
        else if (strcmp(line, "output_file") == 0) strncpy(cfg->output_file, val, 1023);
        else if (strcmp(line, "stimuli_dir") == 0) strncpy(cfg->stimuli_dir, val, 1023);
        else if (strcmp(line, "screen_w") == 0) cfg->screen_w = atoi(val);
        else if (strcmp(line, "screen_h") == 0) cfg->screen_h = atoi(val);
        else if (strcmp(line, "use_fixation") == 0) cfg->use_fixation = (atoi(val) != 0);
        else if (strcmp(line, "fullscreen") == 0) cfg->fullscreen = (atoi(val) != 0);
    }
    fclose(f);
}

bool parse_args(int argc, const char **argv, Config *cfg) {
    memset(cfg, 0, sizeof(Config));
    strncpy(cfg->output_file, "results.csv", 1023);
    cfg->font_size = 24; cfg->screen_w = 1920; cfg->screen_h = 1080;
    cfg->display_index = 0; cfg->scale_factor = 1.0f; cfg->use_fixation = true;
    cfg->vsync = true;

    /* Load cache over defaults */
    load_config_cache(cfg);

    int no_vsync = 0, use_fixation = -1, fullscreen = -1, show_version = 0;
    const char *scale_str = NULL, *duration_str = NULL, *res_str = NULL;
    const char *output_file_arg = NULL, *stim_dir_arg = NULL;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BOOLEAN('v', "version", &show_version, "show version"),
        OPT_GROUP("Output"),
        OPT_STRING ('o', "output", &output_file_arg, "output csv"),
        OPT_STRING (  0, "stimuli-dir", &stim_dir_arg, "stimuli dir"),
        OPT_GROUP("Display"),
        OPT_BOOLEAN('F', "fullscreen", &fullscreen, "fullscreen"),
        OPT_INTEGER('d', "display", &cfg->display_index, "display index"),
        OPT_STRING ('r', "res", &res_str, "WxH"),
        OPT_STRING ('s', "scale", &scale_str, "scale"),
        OPT_BOOLEAN('x', "no-fixation", &use_fixation, "no-fixation"),
        OPT_GROUP("Text"),
        OPT_STRING ('f', "font", &cfg->font_file, "font file"),
        OPT_INTEGER('z', "font-size", &cfg->font_size, "font size"),
        OPT_GROUP("Other"),
        OPT_STRING ('D', "total-duration", &duration_str, "duration ms"),
        OPT_STRING (  0, "dlp", &cfg->dlp_device, "dlp device"),
        OPT_BOOLEAN(  0, "no-vsync", &no_vsync, "no-vsync"),
        OPT_END(),
    };

    struct argparse ap;
    argparse_init(&ap, options, usage_lines, 0);
    argc = argparse_parse(&ap, argc, argv);

    if (show_version) { 
        printf("expe3000 %s\n", EXPE3000_VERSION); 
        exit(0); 
    }
    if (output_file_arg) strncpy(cfg->output_file, output_file_arg, 1023);
    if (stim_dir_arg) strncpy(cfg->stimuli_dir, stim_dir_arg, 1023);

    if (argc > 0) {
        strncpy(cfg->csv_file, (const char *)argv[0], 1023);
    } else {
        SDL_PathInfo info;
        if (cfg->csv_file[0] == '\0' && SDL_GetPathInfo("experiment.csv", &info)) {
            strncpy(cfg->csv_file, "experiment.csv", 1023);
        }
        
        if (cfg->csv_file[0] == '\0' || !SDL_GetPathInfo(cfg->csv_file, &info)) {
            if (!cfg->stimuli_dir[0] && SDL_GetPathInfo("assets", &info) && info.type == SDL_PATHTYPE_DIRECTORY)
                strncpy(cfg->stimuli_dir, "assets", 1023);

            if (!SDL_Init(SDL_INIT_VIDEO)) return false;
            if (!TTF_Init()) return false;
            if (!run_gui_setup(cfg)) return false;
        }
    }

    if (use_fixation == 1) cfg->use_fixation = false;
    if (fullscreen == 1) cfg->fullscreen = true;
    cfg->vsync = !no_vsync;
    if (res_str) sscanf(res_str, "%dx%d", &cfg->screen_w, &cfg->screen_h);
    if (scale_str) cfg->scale_factor = (float)atof(scale_str);
    if (duration_str) cfg->total_duration = (Uint64)atoll(duration_str);

    return true;
}
