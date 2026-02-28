/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include "csv_parser.h"
#include "argparse.h"
#include "version.h"
#include "dlp.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ─── Constants ─────────────────────────────────────────────── */

#define MAX_ACTIVE_SOUNDS   16
#define CROSS_SIZE          20
#define AUDIO_SCRATCH_BYTES 4096   /* pre-allocated scratch buffer for audio thread */

/* ─── Data Structures ────────────────────────────────────────── */

typedef struct {
    Uint8        *data;
    Uint32        len;
    SDL_AudioSpec spec;
} SoundResource;

typedef struct {
    SDL_Texture  *texture;
    float         w, h;
    SoundResource sound;
} Resource;

typedef struct {
    const SoundResource *resource;
    Uint32               play_pos;
    bool                 active;
} ActiveSound;

typedef struct {
    ActiveSound  slots[MAX_ACTIVE_SOUNDS];
    SDL_Mutex   *mutex;
    Uint8        scratch[AUDIO_SCRATCH_BYTES]; /* avoids malloc on audio thread */
} AudioMixer;

typedef struct {
    Uint64 timestamp_ms;
    char   type[16];
    char   label[256];
} EventLogEntry;

typedef struct {
    EventLogEntry *entries;
    int            count;
    int            capacity;
} EventLog;

typedef struct {
    char *csv_file;
    char *output_file;
    char *stimuli_dir;
    char *start_splash;
    char *end_splash;
    char *font_file;
    char *dlp_device;
    int         font_size;
    int         screen_w;
    int         screen_h;
    int         display_index;
    float       scale_factor;
    Uint64      total_duration;
    bool        use_fixation;
    bool        fullscreen;
    bool        vsync;
} Config;

/* ─── Event Logging ──────────────────────────────────────────── */

static bool log_event(EventLog *log, Uint64 time_ms, const char *type, const char *label) {
    if (log->count >= log->capacity) {
        int new_cap = log->capacity == 0 ? 64 : log->capacity * 2;
        EventLogEntry *tmp = realloc(log->entries, new_cap * sizeof(EventLogEntry));
        if (!tmp) {
            SDL_Log("log_event: realloc failed");
            return false;
        }
        log->entries  = tmp;
        log->capacity = new_cap;
    }

    EventLogEntry *e = &log->entries[log->count];
    e->timestamp_ms = time_ms;
    strncpy(e->type,  type,  sizeof(e->type)  - 1); e->type[sizeof(e->type)   - 1] = '\0';
    strncpy(e->label, label, sizeof(e->label) - 1); e->label[sizeof(e->label) - 1] = '\0';
    log->count++;
    return true;
}

static void free_event_log(EventLog *log) {
    free(log->entries);
    log->entries  = NULL;
    log->count    = 0;
    log->capacity = 0;
}

/* ─── Audio ──────────────────────────────────────────────────── */

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream,
                                   int additional_amount, int total_amount)
{
    (void)total_amount;
    AudioMixer *mx = (AudioMixer *)userdata;

    int remaining = additional_amount;
    while (remaining > 0) {
        int chunk = (remaining > AUDIO_SCRATCH_BYTES) ? AUDIO_SCRATCH_BYTES : remaining;
        memset(mx->scratch, 0, chunk);

        SDL_LockMutex(mx->mutex);
        for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
            ActiveSound *s = &mx->slots[i];
            if (!s->active) continue;

            Uint32 sound_remaining = s->resource->len - s->play_pos;
            Uint32 to_mix = (chunk > (int)sound_remaining) ? sound_remaining : (Uint32)chunk;

            SDL_MixAudio(mx->scratch, s->resource->data + s->play_pos,
                         s->resource->spec.format, (float)to_mix, 1.0f);

            s->play_pos += to_mix;
            if (s->play_pos >= s->resource->len) s->active = false;
        }
        SDL_UnlockMutex(mx->mutex);

        SDL_PutAudioStreamData(stream, mx->scratch, chunk);
        remaining -= chunk;
    }
}

/* ─── Rendering Helpers ──────────────────────────────────────── */

static void draw_fixation_cross(SDL_Renderer *renderer, int w, int h) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    float mx = (float)(w / 2), my = (float)(h / 2);
    SDL_RenderLine(renderer, mx - CROSS_SIZE, my, mx + CROSS_SIZE, my);
    SDL_RenderLine(renderer, mx, my - CROSS_SIZE, mx, my + CROSS_SIZE);
}

static bool display_splash(SDL_Renderer *renderer, const char *file_path,
                            int screen_w, int screen_h, float scale_factor)
{
    if (!file_path) return true;

    SDL_Texture *tex = IMG_LoadTexture(renderer, file_path);
    if (!tex) {
        SDL_Log("display_splash: failed to load '%s': %s", file_path, SDL_GetError());
        return true;
    }

    float tw, th;
    SDL_GetTextureSize(tex, &tw, &th);
    SDL_FRect dst = {
        (screen_w  - tw * scale_factor) / 2.0f,
        (screen_h  - th * scale_factor) / 2.0f,
        tw * scale_factor,
        th * scale_factor
    };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, tex, NULL, &dst);
    SDL_RenderPresent(renderer);

    bool quit = false;
    SDL_Event event;
    while (true) {
        if (!SDL_WaitEvent(&event)) break;
        if (event.type == SDL_EVENT_QUIT)    { quit = true; break; }
        if (event.type == SDL_EVENT_KEY_DOWN) break;
    }

    SDL_DestroyTexture(tex);
    return !quit;
}

/* ─── Argument Parsing ───────────────────────────────────────── */

static const char *const usage_lines[] = {
    "expe3000 <stimuli_csv_file> [options]",
    NULL,
};

// Dialog state for GUI mode
char dialog_path[1024] = "";
bool dialog_waiting = false;

void SDLCALL file_dialog_callback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    char *target = (char *)userdata;
    if (filelist && filelist[0]) {
        strncpy(target, filelist[0], 1023);
    } else {
        target[0] = '\0';
    }
    dialog_waiting = false;
}

static bool parse_args(int argc, const char **argv, Config *cfg) {
    memset(cfg, 0, sizeof(Config));
    cfg->output_file    = "results.csv";
    cfg->font_size      = 24;
    cfg->screen_w       = 1920;
    cfg->screen_h       = 1080;
    cfg->display_index  = 0;
    cfg->scale_factor   = 1.0f;
    cfg->total_duration = 0;
    cfg->use_fixation   = true;
    cfg->fullscreen     = false;
    cfg->vsync          = true;

    int         no_vsync        = 0;
    int         use_fixation    = 0;
    int         fullscreen      = 0;
    int         show_version    = 0;
    const char *scale_str       = NULL;
    const char *duration_str    = NULL;
    const char *res_str         = NULL;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_BOOLEAN('v', "version",        &show_version,      "show version and exit"),
        OPT_GROUP("Output"),
        OPT_STRING ('o', "output",         &cfg->output_file,  "output log file (default: results.csv)"),
        OPT_STRING (  0, "stimuli-dir",    &cfg->stimuli_dir,  "folder containing stimuli files"),
        OPT_GROUP("Display"),
        OPT_BOOLEAN('F', "fullscreen",     &fullscreen,        "run in fullscreen mode"),
        OPT_INTEGER('d', "display",        &cfg->display_index,"display monitor index (default: 0)"),
        OPT_STRING ('r', "res",            &res_str,           "resolution WxH (default: 1920x1080)"),
        OPT_STRING ('s', "scale",          &scale_str,         "image magnification factor (default: 1.0)"),
        OPT_BOOLEAN('x', "no-fixation",       &use_fixation,      "disable fixation cross between stimuli"),
        OPT_GROUP("Splash screens"),
        OPT_STRING (  0, "start-splash",   &cfg->start_splash, "image to display at start"),
        OPT_STRING (  0, "end-splash",     &cfg->end_splash,   "image to display at end"),
        OPT_GROUP("Text stimuli"),
        OPT_STRING ('f', "font",           &cfg->font_file,    "TTF font file for text stimuli (optional)"),
        OPT_INTEGER('z', "font-size",      &cfg->font_size,    "font size in points (default: 24)"),
        OPT_GROUP("Timing"),
        OPT_STRING ('D', "total-duration", &duration_str,      "minimum total experiment duration in ms"),
        OPT_STRING (  0, "dlp",            &cfg->dlp_device,   "device path for DLP-IO8-G trigger (e.g. /dev/ttyUSB0)"),
        OPT_BOOLEAN(  0, "no-vsync",       &no_vsync,          "disable VSync (not recommended for timing)"),
        OPT_END(),
    };

    struct argparse ap;
    argparse_init(&ap, options, usage_lines, 0);
    argparse_describe(&ap,
        "\nPresent a timed sequence of image, sound, and text stimuli defined\n"
        "in a CSV file and log keyboard responses to an output file.",
        "\nThe CSV stimulus file is the only required argument.");

    argc = argparse_parse(&ap, argc, argv);

    if (show_version) {
        int sdl_ver = SDL_GetVersion();
        printf("expe3000 version %s\n", EXPE3000_VERSION);
        printf("Author: Christophe Pallier <christophe@pallier.org>\n");
        exit(EXIT_SUCCESS);
    }

    if (argc > 0) {
        cfg->csv_file = (char *)argv[0];
    } else {
        SDL_PathInfo info;
        if (SDL_GetPathInfo("experiment.csv", &info)) {
            cfg->csv_file = "experiment.csv";
            SDL_Log("No CSV file specified, using default: experiment.csv");
            if (!cfg->stimuli_dir && SDL_GetPathInfo("assets", &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
                cfg->stimuli_dir = "assets";
                SDL_Log("Defaulting stimuli-dir to: assets");
            }
        } else {
            // GUI Mode if no CSV provided and no default experiment.csv
            SDL_Init(SDL_INIT_VIDEO);
            SDL_DialogFileFilter filters[] = { {"CSV Files", "csv"}, {"All Files", "*"} };
            dialog_waiting = true;
            SDL_ShowOpenFileDialog(file_dialog_callback, dialog_path, NULL, filters, 2, NULL, false);
            while (dialog_waiting) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) exit(0);
                SDL_Delay(16);
            }
            if (dialog_path[0] == '\0') return false;
            cfg->csv_file = strdup(dialog_path);
        }
    }

    cfg->use_fixation = !(bool)use_fixation;
    cfg->fullscreen   = (bool)fullscreen;
    cfg->vsync        = !no_vsync;

    if (res_str) sscanf(res_str, "%dx%d", &cfg->screen_w, &cfg->screen_h);
    if (scale_str) cfg->scale_factor = (float)atof(scale_str);
    if (duration_str) cfg->total_duration = (Uint64)atoll(duration_str);

    return true;
}

/* ─── Resource Loading / Freeing ─────────────────────────────── */

typedef struct CacheEntry {
    StimType type;
    char     file_path[256];
    SDL_Texture *texture;
    float    w, h;
    SoundResource sound;
    struct CacheEntry *next;
} CacheEntry;

static CacheEntry* find_in_cache(CacheEntry *head, StimType type, const char *path) {
    CacheEntry *curr = head;
    while (curr) {
        if (curr->type == type && strcmp(curr->file_path, path) == 0) return curr;
        curr = curr->next;
    }
    return NULL;
}

static Resource *load_resources(SDL_Renderer *renderer, const Experiment *exp,
                                 TTF_Font *font, const char *base_path,
                                 CacheEntry **cache_out)
{
    *cache_out = NULL;
    Resource *res = calloc(exp->count, sizeof(Resource));
    if (!res) return NULL;

    for (int i = 0; i < exp->count; i++) {
        const Stimulus *s = &exp->stimuli[i];
        CacheEntry *entry = find_in_cache(*cache_out, s->type, s->file_path);

        if (entry) {
            res[i].texture = entry->texture;
            res[i].w = entry->w; res[i].h = entry->h;
            res[i].sound = entry->sound;
            continue;
        }

        entry = calloc(1, sizeof(CacheEntry));
        entry->type = s->type;
        strncpy(entry->file_path, s->file_path, sizeof(entry->file_path) - 1);

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%s", base_path, s->file_path);

        if (s->type == STIM_IMAGE) {
            entry->texture = IMG_LoadTexture(renderer, full_path);
            if (entry->texture) SDL_GetTextureSize(entry->texture, &entry->w, &entry->h);
            else SDL_Log("Failed to load image: %s", full_path);
        } else if (s->type == STIM_SOUND) {
            if (!SDL_LoadWAV(full_path, &entry->sound.spec, &entry->sound.data, &entry->sound.len))
                SDL_Log("Failed to load sound: %s", full_path);
        } else if (s->type == STIM_TEXT) {
            if (font) {
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface *surf = TTF_RenderText_Blended(font, s->file_path, 0, white);
                if (surf) {
                    entry->texture = SDL_CreateTextureFromSurface(renderer, surf);
                    entry->w = (float)surf->w; entry->h = (float)surf->h;
                    SDL_DestroySurface(surf);
                }
            }
        }

        res[i].texture = entry->texture;
        res[i].w = entry->w; res[i].h = entry->h;
        res[i].sound = entry->sound;

        entry->next = *cache_out;
        *cache_out = entry;
    }
    return res;
}

static void free_resources(Resource *resources, CacheEntry *cache) {
    CacheEntry *curr = cache;
    while (curr) {
        CacheEntry *next = curr->next;
        if (curr->texture) SDL_DestroyTexture(curr->texture);
        if (curr->sound.data) SDL_free(curr->sound.data);
        free(curr);
        curr = next;
    }
    free(resources);
}

/* ─── Font Discovery ────────────────────────────────────────── */

static SDL_EnumerationResult find_font_callback(void *userdata, const char *dirname, const char *fname) {
    char *result = (char *)userdata;
    const char *ext = strrchr(fname, '.');
    if (ext && (SDL_strcasecmp(ext, ".ttf") == 0 || SDL_strcasecmp(ext, ".ttc") == 0)) {
        SDL_snprintf(result, 1024, "%s/%s", dirname, fname);
        return SDL_ENUM_SUCCESS;
    }
    return SDL_ENUM_CONTINUE;
}

static const char* get_default_font_path(void) {
    static char local_font[1024];
    local_font[0] = '\0';
    SDL_EnumerateDirectory("fonts", find_font_callback, local_font);
    if (local_font[0] != '\0') return local_font;

    static const char *const paths[] = {
#if defined(_WIN32)
        "C:\\Windows\\Fonts\\arial.ttf",
#elif defined(__APPLE__)
        "/Library/Fonts/Arial.ttf",
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

/* ─── Main ───────────────────────────────────────────────────── */

int main(int argc, const char *argv[]) {
    char cmd_line[1024] = "";
    for (int i = 0; i < argc; i++) {
        strncat(cmd_line, argv[i], sizeof(cmd_line) - strlen(cmd_line) - 1);
        if (i < argc - 1) strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
    }

    Config cfg;
    if (!parse_args(argc, argv, &cfg)) return 0;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return 1;
    TTF_Init();

    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    SDL_DisplayID target_display = (cfg.display_index < num_displays) ? displays[cfg.display_index] : displays[0];
    SDL_free(displays);

    Uint32 window_flags = 0;
    if (cfg.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *window = SDL_CreateWindow("expe3000", cfg.screen_w, cfg.screen_h, window_flags);
    if (cfg.fullscreen) SDL_SetWindowPosition(window, SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display), SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display));
    SDL_HideCursor();

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (cfg.vsync) SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, cfg.screen_w, cfg.screen_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    display_splash(renderer, cfg.start_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor);

    TTF_Font *font = NULL;
    const char *font_to_load = cfg.font_file ? cfg.font_file : get_default_font_path();
    if (font_to_load) {
        font = TTF_OpenFont(font_to_load, (float)cfg.font_size);
        if (font) SDL_Log("Loaded font: %s", font_to_load);
        else SDL_Log("Failed to load font '%s': %s", font_to_load, SDL_GetError());
    } else {
        SDL_Log("Warning: No font loaded. Text stimuli will be skipped.");
    }

    Experiment *exp = parse_csv(cfg.csv_file);
    if (!exp) return 1;

    char base_path[512] = "";
    const char *last_slash = strrchr(cfg.csv_file, '/');
    const char *last_backslash = strrchr(cfg.csv_file, '\\');
    const char *sep = (last_slash > last_backslash) ? last_slash : last_backslash;
    if (sep) {
        size_t dir_len = sep - cfg.csv_file + 1;
        if (dir_len < sizeof(base_path)) {
            strncpy(base_path, cfg.csv_file, dir_len);
            base_path[dir_len] = '\0';
        }
    }
    if (cfg.stimuli_dir) {
        strncat(base_path, cfg.stimuli_dir, sizeof(base_path) - strlen(base_path) - 1);
        size_t len = strlen(base_path);
        if (len > 0 && base_path[len-1] != '/' && base_path[len-1] != '\\') {
#ifdef _WIN32
            strncat(base_path, "\\", sizeof(base_path) - len - 1);
#else
            strncat(base_path, "/", sizeof(base_path) - len - 1);
#endif
        }
    }

    dlp_io8g_t *dlp = NULL;
    if (cfg.dlp_device) {
        dlp = dlp_new(cfg.dlp_device, 9600);
        if (dlp) { SDL_Log("DLP device opened: %s", cfg.dlp_device); dlp_unset(dlp, "12345678"); }
        else SDL_Log("Warning: Failed to open DLP device '%s'", cfg.dlp_device);
    }

    AudioMixer mx;
    memset(&mx, 0, sizeof(mx));
    mx.mutex = SDL_CreateMutex();
    SDL_AudioSpec target_spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *master_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &target_spec, audio_callback, &mx);
    SDL_ResumeAudioStreamDevice(master_stream);

    SDL_Log("Loading resources...");
    CacheEntry *cache = NULL;
    Resource *resources = load_resources(renderer, exp, font, base_path, &cache);
    
    int image_count = 0, sound_count = 0, text_count = 0;
    size_t total_memory = 0;
    for (CacheEntry *curr = cache; curr; curr = curr->next) {
        if (curr->type == STIM_IMAGE || curr->type == STIM_TEXT) {
            if (curr->type == STIM_IMAGE) image_count++; else text_count++;
            if (curr->texture) { float tw, th; SDL_GetTextureSize(curr->texture, &tw, &th); total_memory += (size_t)(tw * th * 4); }
        } else if (curr->type == STIM_SOUND) { sound_count++; total_memory += curr->sound.len; }
    }
    SDL_Log("Resources loaded: %d images, %d sounds, %d text textures. Total estimated memory: %.2f MB", image_count, sound_count, text_count, (double)total_memory / (1024.0 * 1024.0));

    EventLog log = {0};
    bool running = true;
    SDL_Event event;
    Uint64 start_time = SDL_GetTicks();
    int current_stim = 0, active_visual_idx = -1;
    Uint64 visual_end_time = 0;

    while (running) {
        Uint64 current_time = SDL_GetTicks() - start_time;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) running = false;
                else log_event(&log, current_time, "RESPONSE", SDL_GetKeyName(event.key.key));
            }
        }

        bool triggered_onset = false;
        int triggered_idx = -1;
        if (current_stim < exp->count && (current_time + (Uint64)(500.0/60.0)) >= exp->stimuli[current_stim].timestamp_ms) {
            Stimulus *s = &exp->stimuli[current_stim];
            if ((s->type == STIM_IMAGE || s->type == STIM_TEXT) && resources[current_stim].texture) {
                active_visual_idx = current_stim;
                triggered_onset = true; triggered_idx = current_stim;
                if (dlp) { if (s->type == STIM_IMAGE) dlp_set(dlp, "1"); else dlp_set(dlp, "3"); }
            } else if (s->type == STIM_SOUND && resources[current_stim].sound.data) {
                SDL_LockMutex(mx.mutex);
                for (int j = 0; j < MAX_ACTIVE_SOUNDS; j++) {
                    if (!mx.slots[j].active) {
                        mx.slots[j].resource = &resources[current_stim].sound;
                        mx.slots[j].play_pos = 0; mx.slots[j].active = true;
                        log_event(&log, current_time, "SOUND_ONSET", s->file_path);
                        if (dlp) { dlp_set(dlp, "2"); SDL_Delay(5); dlp_unset(dlp, "2"); }
                        break;
                    }
                }
                SDL_UnlockMutex(mx.mutex);
            }
            current_stim++;
            fprintf(stdout, "\rStimulus: %d/%d ", current_stim, exp->count); fflush(stdout);
        }

        if (active_visual_idx != -1 && current_time >= visual_end_time) {
            log_event(&log, current_time, (exp->stimuli[active_visual_idx].type == STIM_IMAGE ? "IMAGE_OFFSET" : "TEXT_OFFSET"), exp->stimuli[active_visual_idx].file_path);
            if (dlp) { if (exp->stimuli[active_visual_idx].type == STIM_IMAGE) dlp_unset(dlp, "1"); else dlp_unset(dlp, "3"); }
            active_visual_idx = -1;
        }

        if (current_stim >= exp->count && active_visual_idx == -1 && current_time >= cfg.total_duration) running = false;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (active_visual_idx != -1) {
            Resource *r = &resources[active_visual_idx];
            SDL_FRect dst_rect = {(cfg.screen_w - (r->w * cfg.scale_factor)) / 2.0f, (cfg.screen_h - (r->h * cfg.scale_factor)) / 2.0f, r->w * cfg.scale_factor, r->h * cfg.scale_factor};
            SDL_RenderTexture(renderer, r->texture, NULL, &dst_rect);
        } else if (cfg.use_fixation) {
            draw_fixation_cross(renderer, cfg.screen_w, cfg.screen_h);
        }
        SDL_RenderPresent(renderer); 

        if (triggered_onset) {
            Uint64 onset_time = SDL_GetTicks() - start_time;
            log_event(&log, onset_time, (exp->stimuli[triggered_idx].type == STIM_IMAGE ? "IMAGE_ONSET" : "TEXT_ONSET"), exp->stimuli[triggered_idx].file_path);
            visual_end_time = onset_time + exp->stimuli[triggered_idx].duration_ms;
        }
        if (!cfg.vsync) SDL_Delay(1);
    }
    printf("\n");

    FILE *res_file = fopen(cfg.output_file, "w");
    if (res_file) {
        time_t rawtime; time(&rawtime);
        fprintf(res_file, "# expe3000 Experiment Results\n# Date/Time: %s", ctime(&rawtime));
        fprintf(res_file, "# Command Line: %s\n", cmd_line);
        fprintf(res_file, "timestamp_ms,event_type,label\n");
        for (int i = 0; i < log.count; i++) fprintf(res_file, "%" PRIu64 ",%s,%s\n", log.entries[i].timestamp_ms, log.entries[i].type, log.entries[i].label);
        fclose(res_file);
        SDL_Log("Results saved to: %s", cfg.output_file);
    }

    display_splash(renderer, cfg.end_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor);
    if (font) TTF_CloseFont(font);
    if (dlp) dlp_close(dlp);
    free_event_log(&log);
    free_resources(resources, cache);
    SDL_DestroyAudioStream(master_stream);
    SDL_DestroyMutex(mx.mutex);
    free_experiment(exp);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
