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
#include <time.h>

#define CACHE_FILE ".expe3000_cache"

static const char *const usage_lines[] = {
    "expe3000 <stimuli_csv_file> [options]",
    NULL,
};

static void parse_color(const char *str, SDL_Color *color) {
    int r, g, b;
    if (sscanf(str, "%d,%d,%d", &r, &g, &b) == 3) {
        color->r = (Uint8)r;
        color->g = (Uint8)g;
        color->b = (Uint8)b;
        color->a = 255;
    }
}

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
    fprintf(f, "bg_color=%d,%d,%d\n", cfg->bg_color.r, cfg->bg_color.g, cfg->bg_color.b);
    fprintf(f, "text_color=%d,%d,%d\n", cfg->text_color.r, cfg->text_color.g, cfg->text_color.b);
    fprintf(f, "fixation_color=%d,%d,%d\n", cfg->fixation_color.r, cfg->fixation_color.g, cfg->fixation_color.b);
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
        else if (strcmp(line, "bg_color") == 0) parse_color(val, &cfg->bg_color);
        else if (strcmp(line, "text_color") == 0) parse_color(val, &cfg->text_color);
        else if (strcmp(line, "fixation_color") == 0) parse_color(val, &cfg->fixation_color);
    }
    fclose(f);
}

bool parse_args(int argc, const char **argv, Config *cfg) {
    memset(cfg, 0, sizeof(Config));
    strncpy(cfg->output_file, "results.csv", 1023);
    cfg->font_size = 24; cfg->screen_w = 1920; cfg->screen_h = 1080;
    cfg->display_index = 0; cfg->scale_factor = 1.0f; cfg->use_fixation = true;
    cfg->vsync = true;
    cfg->bg_color = (SDL_Color){0, 0, 0, 255};
    cfg->text_color = (SDL_Color){255, 255, 255, 255};
    cfg->fixation_color = (SDL_Color){255, 255, 255, 255};

    int no_vsync = 0, use_fixation = 0, fullscreen = 0, show_version = 0, force_gui = 0;
    const char *scale_str = NULL, *duration_str = NULL, *res_str = NULL;
    const char *output_file_arg = NULL, *stim_dir_arg = NULL;
    const char *bg_color_str = NULL, *text_color_str = NULL, *fixation_color_str = NULL;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BOOLEAN('v', "version", &show_version, "show version"),
        OPT_GROUP("Output"),
        OPT_STRING ('o', "output", &output_file_arg, "output csv"),
        OPT_STRING (  0, "stimuli-dir", &stim_dir_arg, "stimuli dir"),
        OPT_GROUP("Display"),
        OPT_BOOLEAN('g', "gui", &force_gui, "force starting with the GUI"),
        OPT_BOOLEAN('F', "fullscreen", &fullscreen, "fullscreen"),
        OPT_INTEGER('d', "display", &cfg->display_index, "display index"),
        OPT_STRING ('r', "res", &res_str, "WxH"),
        OPT_STRING ('s', "scale", &scale_str, "scale"),
        OPT_BOOLEAN('x', "no-fixation", &use_fixation, "no-fixation"),
        OPT_STRING (  0, "bg-color", &bg_color_str, "background color R,G,B"),
        OPT_STRING (  0, "fixation-color", &fixation_color_str, "fixation cross color R,G,B"),
        OPT_GROUP("Text"),
        OPT_STRING ('f', "font", &cfg->font_file, "font file"),
        OPT_INTEGER('z', "font-size", &cfg->font_size, "font size"),
        OPT_STRING (  0, "text-color", &text_color_str, "text color R,G,B"),
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

    if (force_gui) {
        load_config_cache(cfg);
        /* If user provided arguments, they should override the cache */
        if (output_file_arg) strncpy(cfg->output_file, output_file_arg, 1023);
        if (stim_dir_arg) strncpy(cfg->stimuli_dir, stim_dir_arg, 1023);
        if (argc > 0) strncpy(cfg->csv_file, (const char *)argv[0], 1023);
        if (bg_color_str) parse_color(bg_color_str, &cfg->bg_color);
        if (text_color_str) parse_color(text_color_str, &cfg->text_color);
        if (fixation_color_str) parse_color(fixation_color_str, &cfg->fixation_color);
        
        if (!SDL_Init(SDL_INIT_VIDEO)) return false;
        if (!TTF_Init()) return false;
        if (!run_gui_setup(cfg)) return false;
    } else if (argc > 0) {
        strncpy(cfg->csv_file, (const char *)argv[0], 1023);
    } else {
        /* No arguments provided: launch GUI setup pre-populated from cache */
        load_config_cache(cfg);
        
        if (!SDL_Init(SDL_INIT_VIDEO)) return false;
        if (!TTF_Init()) return false;
        
        SDL_PathInfo info;
        if (!cfg->stimuli_dir[0] && SDL_GetPathInfo("assets", &info) && info.type == SDL_PATHTYPE_DIRECTORY)
            strncpy(cfg->stimuli_dir, "assets", 1023);

        if (!run_gui_setup(cfg)) return false;
    }

    if (use_fixation > 0) cfg->use_fixation = false;
    if (fullscreen > 0) cfg->fullscreen = true;
    cfg->vsync = !no_vsync;
    if (res_str) sscanf(res_str, "%dx%d", &cfg->screen_w, &cfg->screen_h);
    if (scale_str) cfg->scale_factor = (float)atof(scale_str);
    if (duration_str) cfg->total_duration = (Uint64)atoll(duration_str);
    if (bg_color_str) parse_color(bg_color_str, &cfg->bg_color);
    if (text_color_str) parse_color(text_color_str, &cfg->text_color);
    if (fixation_color_str) parse_color(fixation_color_str, &cfg->fixation_color);

    /* Decorate output filename with experiment basename and timestamp */
    if (cfg->csv_file[0] != '\0') {
        char base_out[512];
        char ext[16] = ".csv";
        strncpy(base_out, cfg->output_file, 511);
        base_out[511] = '\0';
        char *dot = strrchr(base_out, '.');
        if (dot) {
            strncpy(ext, dot, 15);
            ext[15] = '\0';
            *dot = '\0';
        }

        char csv_base[256];
        const char *last_slash = strrchr(cfg->csv_file, '/');
        const char *last_backslash = strrchr(cfg->csv_file, '\\');
        const char *start = cfg->csv_file;
        if (last_slash && last_slash >= start) start = last_slash + 1;
        if (last_backslash && last_backslash >= start) start = last_backslash + 1;
        strncpy(csv_base, start, 255);
        csv_base[255] = '\0';
        char *csv_dot = strrchr(csv_base, '.');
        if (csv_dot) *csv_dot = '\0';

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", t);

        snprintf(cfg->output_file, 1023, "%s_%s_%s%s", base_out, csv_base, timestamp, ext);
    }

    return true;
}
