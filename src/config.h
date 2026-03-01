/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct {
    char csv_file[1024];
    char output_file[1024];
    char stimuli_dir[1024];
    char *start_splash;
    char *end_splash;
    char *font_file;
    char *dlp_device;
    int   font_size;
    int   screen_w;
    int   screen_h;
    int   display_index;
    float scale_factor;
    Uint64 total_duration;
    bool  use_fixation;
    bool  fullscreen;
    bool  vsync;
    bool  gui;
    SDL_Color bg_color;
    SDL_Color text_color;
    SDL_Color fixation_color;
} Config;

/**
 * @brief Parses command line arguments and fills the config structure.
 * 
 * If no arguments are provided and default files aren't found, it may trigger the GUI setup.
 * 
 * @param argc Argument count.
 * @param argv Argument vector.
 * @param cfg Pointer to the Config structure to fill.
 * @return true if the program should proceed, false if it should exit (e.g., --help or error).
 */
bool parse_args(int argc, const char **argv, Config *cfg);

/**
 * @brief Saves the current configuration to a local file.
 */
void save_config_cache(const Config *cfg);

/**
 * @brief Loads the configuration from a local file if it exists.
 */
void load_config_cache(Config *cfg);

#endif // CONFIG_H
