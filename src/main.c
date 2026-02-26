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
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "csv_parser.h"
#include "argparse.h"

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
    const char *csv_file;
    const char *output_file;
    const char *start_splash;
    const char *end_splash;
    const char *font_file;
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

/* Returns false on allocation failure. */
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

/* Audio callback — must not allocate; uses pre-allocated scratch buffer. */
static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream,
                                   int additional_amount, int total_amount)
{
    (void)total_amount;
    AudioMixer *mx = (AudioMixer *)userdata;

    /* Clamp to scratch buffer size to avoid overflow. */
    int mix_len = additional_amount;
    if (mix_len > AUDIO_SCRATCH_BYTES) mix_len = AUDIO_SCRATCH_BYTES;

    memset(mx->scratch, 0, mix_len);

    SDL_LockMutex(mx->mutex);
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        ActiveSound *s = &mx->slots[i];
        if (!s->active) continue;
        Uint32 remaining = s->resource->len - s->play_pos;
        Uint32 chunk     = (mix_len > (int)remaining) ? remaining : (Uint32)mix_len;
        SDL_MixAudio(mx->scratch, s->resource->data + s->play_pos,
                     s->resource->spec.format, chunk, 1.0f);
        s->play_pos += chunk;
        if (s->play_pos >= s->resource->len) s->active = false;
    }
    SDL_UnlockMutex(mx->mutex);

    SDL_PutAudioStreamData(stream, mx->scratch, mix_len);
}

/* ─── Rendering Helpers ──────────────────────────────────────── */

static void draw_fixation_cross(SDL_Renderer *renderer, int w, int h) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    float mx = (float)(w / 2), my = (float)(h / 2);
    SDL_RenderLine(renderer, mx - CROSS_SIZE, my, mx + CROSS_SIZE, my);
    SDL_RenderLine(renderer, mx, my - CROSS_SIZE, mx, my + CROSS_SIZE);
}

/*
 * Displays a full-screen splash image and waits for a keypress.
 * Returns true normally, false if the user requested quit (so the caller
 * can exit cleanly instead of calling exit() and leaking resources).
 */
static bool display_splash(SDL_Renderer *renderer, const char *file_path,
                            int screen_w, int screen_h, float scale_factor)
{
    if (!file_path) return true;

    SDL_Texture *tex = IMG_LoadTexture(renderer, file_path);
    if (!tex) {
        SDL_Log("display_splash: failed to load '%s': %s", file_path, SDL_GetError());
        return true; /* non-fatal — just skip the splash */
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

/*
 * Fills *cfg with defaults then parses argv via cofyc/argparse.
 * Returns false if the caller should exit (--help, missing CSV, etc.).
 *
 * Notes on type mismatches:
 *   --scale and --total-duration cannot be expressed directly as
 *   OPT_INTEGER (int) because they need float and Uint64 respectively.
 *   They are declared as OPT_STRING and converted after parsing.
 *   --res likewise takes a "WxH" compound string requiring sscanf.
 *   --no-vsync is a boolean flag that sets vsync=false, so we store it
 *   in a temporary int and invert it into cfg->vsync afterwards.
 */
static bool parse_args(int argc, const char **argv, Config *cfg) {
    /* Defaults */
    cfg->csv_file       = NULL;
    cfg->output_file    = "results.csv";
    cfg->start_splash   = NULL;
    cfg->end_splash     = NULL;
    cfg->font_file      = NULL;
    cfg->font_size      = 24;
    cfg->screen_w       = 1920;
    cfg->screen_h       = 1080;
    cfg->display_index  = 0;
    cfg->scale_factor   = 1.0f;
    cfg->total_duration = 0;
    cfg->use_fixation   = false;
    cfg->fullscreen     = false;
    cfg->vsync          = true;

    /* Intermediate storage for options that need post-parse conversion */
    int         no_vsync        = 0;
    int         use_fixation    = 0;
    int         fullscreen      = 0;
    const char *scale_str       = NULL;
    const char *duration_str    = NULL;
    const char *res_str         = NULL;

    struct argparse_option options[] = {
        OPT_HELP(),
        OPT_GROUP("Output"),
        OPT_STRING ('o', "output",         &cfg->output_file,  "output log file (default: results.csv)"),
        OPT_GROUP("Display"),
        OPT_BOOLEAN('F', "fullscreen",     &fullscreen,        "run in fullscreen mode"),
        OPT_INTEGER('d', "display",        &cfg->display_index,"display monitor index (default: 0)"),
        OPT_STRING ('r', "res",            &res_str,           "resolution WxH (default: 1920x1080)"),
        OPT_STRING ('s', "scale",          &scale_str,         "image magnification factor (default: 1.0)"),
        OPT_BOOLEAN('x', "fixation",       &use_fixation,      "show fixation cross between stimuli"),
        OPT_GROUP("Splash screens"),
        OPT_STRING (  0, "start-splash",   &cfg->start_splash, "image to display at start"),
        OPT_STRING (  0, "end-splash",     &cfg->end_splash,   "image to display at end"),
        OPT_GROUP("Text stimuli"),
        OPT_STRING ('f', "font",           &cfg->font_file,    "TTF font file for text stimuli"),
        OPT_INTEGER('z', "font-size",      &cfg->font_size,    "font size in points (default: 24)"),
        OPT_GROUP("Timing"),
        OPT_STRING ('D', "total-duration", &duration_str,      "minimum total experiment duration in ms"),
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

    /* First remaining positional argument is the CSV file */
    if (argc > 0)
        cfg->csv_file = argv[0];

    if (!cfg->csv_file) {
        fprintf(stderr, "Error: no CSV stimulus file specified.\n");
        argparse_usage(&ap);
        return false;
    }

    /* Post-parse conversions */
    cfg->use_fixation = (bool)use_fixation;
    cfg->fullscreen   = (bool)fullscreen;
    cfg->vsync        = !no_vsync;

    if (res_str)
        sscanf(res_str, "%dx%d", &cfg->screen_w, &cfg->screen_h);

    if (scale_str)
        cfg->scale_factor = (float)atof(scale_str);

    if (duration_str)
        cfg->total_duration = (Uint64)atoll(duration_str);

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
        if (curr->type == type && strcmp(curr->file_path, path) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

static Resource *load_resources(SDL_Renderer *renderer, const Experiment *exp,
                                 TTF_Font *font, CacheEntry **cache_out)
{
    *cache_out = NULL;
    Resource *res = calloc(exp->count, sizeof(Resource));
    if (!res) return NULL;

    for (int i = 0; i < exp->count; i++) {
        const Stimulus *s = &exp->stimuli[i];

        CacheEntry *entry = find_in_cache(*cache_out, s->type, s->file_path);

        if (entry) {
            res[i].texture = entry->texture;
            res[i].w = entry->w;
            res[i].h = entry->h;
            res[i].sound = entry->sound;
            continue;
        }

        /* Not in cache, load it */
        entry = calloc(1, sizeof(CacheEntry));
        if (!entry) {
            SDL_Log("load_resources: calloc failed for CacheEntry");
            continue;
        }
        entry->type = s->type;
        strncpy(entry->file_path, s->file_path, sizeof(entry->file_path) - 1);

        if (s->type == STIM_IMAGE) {
            entry->texture = IMG_LoadTexture(renderer, s->file_path);
            if (entry->texture)
                SDL_GetTextureSize(entry->texture, &entry->w, &entry->h);
            else
                SDL_Log("load_resources: failed to load image '%s': %s", s->file_path, SDL_GetError());
        } else if (s->type == STIM_SOUND) {
            if (!SDL_LoadWAV(s->file_path, &entry->sound.spec,
                             &entry->sound.data, &entry->sound.len))
                SDL_Log("load_resources: failed to load WAV '%s': %s", s->file_path, SDL_GetError());
        } else if (s->type == STIM_TEXT) {
            if (!font) {
                SDL_Log("load_resources: text stimulus '%s' skipped (no font loaded)", s->file_path);
            } else {
                SDL_Color white = {255, 255, 255, 255};
                SDL_Surface *surf = TTF_RenderText_Blended(font, s->file_path, 0, white);
                if (surf) {
                    entry->texture = SDL_CreateTextureFromSurface(renderer, surf);
                    entry->w = (float)surf->w;
                    entry->h = (float)surf->h;
                    SDL_DestroySurface(surf);
                }
            }
        }

        /* Add to cache */
        entry->next = *cache_out;
        *cache_out = entry;

        /* Copy to result array */
        res[i].texture = entry->texture;
        res[i].w = entry->w;
        res[i].h = entry->h;
        res[i].sound = entry->sound;
    }
    return res;
}

static void free_resources(Resource *resources, CacheEntry *cache) {
    free(resources);

    CacheEntry *curr = cache;
    while (curr) {
        CacheEntry *next = curr->next;
        if (curr->texture)    SDL_DestroyTexture(curr->texture);
        if (curr->sound.data) SDL_free(curr->sound.data);
        free(curr);
        curr = next;
    }
}

/* ─── Result Writing ─────────────────────────────────────────── */

static void write_results(const Config *cfg, const EventLog *log,
                           SDL_Renderer *renderer,
                           const SDL_DisplayMode *mode,
                           const char *timestamp_str,
                           const char *username, const char *hostname,
                           const char *cmd_line)
{
    FILE *f = fopen(cfg->output_file, "w");
    if (!f) {
        SDL_Log("write_results: cannot open '%s' for writing", cfg->output_file);
        return;
    }

    fprintf(f, "# expe3000 Experiment Results\n");
    fprintf(f, "# Date/Time: %s\n", timestamp_str);
    fprintf(f, "# User/Host: %s@%s\n", username, hostname);
    fprintf(f, "# Command Line: %s\n", cmd_line);
    fprintf(f, "# Platform: %s\n", SDL_GetPlatform());
    fprintf(f, "# Video Driver: %s\n", SDL_GetCurrentVideoDriver());
    fprintf(f, "# Renderer: %s (VSync: %s)\n",
            SDL_GetRendererName(renderer), cfg->vsync ? "ON" : "OFF");
    if (mode)
        fprintf(f, "# Display Resolution: %dx%d @ %.2fHz\n",
                mode->w, mode->h, mode->refresh_rate);

    int ver = SDL_GetVersion();
    fprintf(f, "# SDL Version: %d.%d.%d\n",
            SDL_VERSIONNUM_MAJOR(ver), SDL_VERSIONNUM_MINOR(ver), SDL_VERSIONNUM_MICRO(ver));
    fprintf(f, "# Logical Resolution: %dx%d\n", cfg->screen_w, cfg->screen_h);
    fprintf(f, "# Scale Factor: %.2f\n#\n", cfg->scale_factor);
    fprintf(f, "timestamp_ms,event_type,label\n");

    for (int i = 0; i < log->count; i++)
        fprintf(f, "%lu,%s,%s\n",
                (unsigned long)log->entries[i].timestamp_ms,
                log->entries[i].type,
                log->entries[i].label);

    fclose(f);
}

/* ─── Main Loop ──────────────────────────────────────────────── */

static void run_experiment(const Config *cfg, Experiment *exp, Resource *resources,
                            SDL_Renderer *renderer, AudioMixer *mixer, EventLog *log)
{
    /* Determine refresh rate for predictive onset timing. */
    /* NOTE: target_display needs to be passed in or queried here. */
    float refresh_rate = 60.0f;
    int   num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    SDL_DisplayID target_display = (cfg->display_index < num_displays)
                                   ? displays[cfg->display_index] : displays[0];
    SDL_free(displays);

    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(target_display);
    if (mode && mode->refresh_rate > 0) refresh_rate = mode->refresh_rate;

    double frame_duration_ms = 1000.0 / refresh_rate;
    double look_ahead_ms     = frame_duration_ms / 2.0;

    bool    running          = true;
    SDL_Event event;
    Uint64  start_time       = SDL_GetTicks();
    int     current_stim     = 0;
    int     active_visual    = -1;
    Uint64  visual_end_time  = 0;

    while (running) {
        Uint64 now = SDL_GetTicks() - start_time;

        /* ── Input ── */
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE)
                    running = false;
                else
                    log_event(log, now, "RESPONSE", SDL_GetKeyName(event.key.key));
            }
        }

        /* ── Stimulus Trigger (predictive, fires one frame early) ── */
        bool   triggered_onset = false;
        int    triggered_idx   = -1;

        if (current_stim < exp->count &&
            (now + (Uint64)look_ahead_ms) >= exp->stimuli[current_stim].timestamp_ms)
        {
            Stimulus *s = &exp->stimuli[current_stim];

            if ((s->type == STIM_IMAGE || s->type == STIM_TEXT) &&
                resources[current_stim].texture)
            {
                active_visual   = current_stim;
                visual_end_time = now + s->duration_ms;
                triggered_onset = true;
                triggered_idx   = current_stim;
            } else if (s->type == STIM_SOUND && resources[current_stim].sound.data) {
                SDL_LockMutex(mixer->mutex);
                for (int j = 0; j < MAX_ACTIVE_SOUNDS; j++) {
                    if (!mixer->slots[j].active) {
                        mixer->slots[j].resource = &resources[current_stim].sound;
                        mixer->slots[j].play_pos = 0;
                        mixer->slots[j].active   = true;
                        log_event(log, now, "SOUND_ONSET", s->file_path);
                        break;
                    }
                }
                SDL_UnlockMutex(mixer->mutex);
            }
            current_stim++;
        }

        /* ── Visual Offset ── */
        if (active_visual != -1 && now >= visual_end_time) {
            const char *offset_type = (exp->stimuli[active_visual].type == STIM_IMAGE)
                                      ? "IMAGE_OFFSET" : "TEXT_OFFSET";
            log_event(log, now, offset_type, exp->stimuli[active_visual].file_path);
            active_visual = -1;
        }

        /* ── Exit condition ── */
        if (current_stim >= exp->count && active_visual == -1 && now >= cfg->total_duration)
            running = false;

        /* ── Render ── */
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (active_visual != -1) {
            Resource *r = &resources[active_visual];
            SDL_FRect dst = {
                (cfg->screen_w - r->w * cfg->scale_factor) / 2.0f,
                (cfg->screen_h - r->h * cfg->scale_factor) / 2.0f,
                r->w * cfg->scale_factor,
                r->h * cfg->scale_factor
            };
            SDL_RenderTexture(renderer, r->texture, NULL, &dst);
        } else if (cfg->use_fixation) {
            draw_fixation_cross(renderer, cfg->screen_w, cfg->screen_h);
        }

        SDL_RenderPresent(renderer);  /* blocks until VSync when vsync=true */

        /* Log onset AFTER VSync for maximum timestamp accuracy. */
        if (triggered_onset) {
            Uint64 onset_time = SDL_GetTicks() - start_time;
            const char *onset_type = (exp->stimuli[triggered_idx].type == STIM_IMAGE)
                                     ? "IMAGE_ONSET" : "TEXT_ONSET";
            log_event(log, onset_time, onset_type, exp->stimuli[triggered_idx].file_path);

            /* Anchor the offset clock to the actual onset frame. */
            visual_end_time = onset_time + exp->stimuli[triggered_idx].duration_ms;
        }

        if (!cfg->vsync) SDL_Delay(1);
    }
}

/* ─── Entry Point ────────────────────────────────────────────── */

int main(int argc, const char **argv) {
    /* ── Metadata capture ── must happen before parse_args, because
       argparse_parse rewrites argc/argv in-place (stripping flags),
       which would cause the cmd_line loop to walk off the end of the
       repacked array and segfault. */

    /* Build the raw command line from the original argc/argv. */
    char cmd_line[1024] = "";
    for (int i = 0; i < argc; i++) {
        strncat(cmd_line, argv[i], sizeof(cmd_line) - strlen(cmd_line) - 1);
        if (i < argc - 1)
            strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
    }

    time_t rawtime;
    time(&rawtime);
    char timestamp_str[64];
    strncpy(timestamp_str, ctime(&rawtime), sizeof(timestamp_str) - 1);
    timestamp_str[sizeof(timestamp_str) - 1] = '\0';
    /* Strip trailing newline from ctime output. */
    size_t ts_len = strlen(timestamp_str);
    if (ts_len > 0 && timestamp_str[ts_len - 1] == '\n')
        timestamp_str[ts_len - 1] = '\0';

    char hostname[256] = "unknown";
    char username[256] = "unknown";
#ifdef _WIN32
    DWORD hlen = sizeof(hostname); GetComputerNameA(hostname, &hlen);
    DWORD ulen = sizeof(username); GetUserNameA(username, &ulen);
#else
    gethostname(hostname, sizeof(hostname));
    const char *login = getlogin();
    if (login) {
        strncpy(username, login, sizeof(username) - 1);
    } else {
        const char *env_user = getenv("USER");
        if (env_user) strncpy(username, env_user, sizeof(username) - 1);
    }
#endif

    /* Parse arguments — this mutates argc/argv, so it must come after
       the cmd_line and metadata capture above. */
    Config cfg;
    if (!parse_args(argc, argv, &cfg)) return 1;

    /* ── SDL Initialisation ── */
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!TTF_Init()) {
        fprintf(stderr, "TTF_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    if (!displays || num_displays == 0) {
        fprintf(stderr, "No displays found: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return 1;
    }
    SDL_DisplayID target_display = (cfg.display_index < num_displays)
                                   ? displays[cfg.display_index] : displays[0];
    SDL_free(displays);

    Uint32 window_flags = 0;
    if (cfg.fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *window = SDL_CreateWindow("expe3000", cfg.screen_w, cfg.screen_h, window_flags);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit(); SDL_Quit();
        return 1;
    }
    SDL_HideCursor();
    if (cfg.fullscreen)
        SDL_SetWindowPosition(window,
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display),
            SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display));

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit();
        return 1;
    }
    if (cfg.vsync) SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, cfg.screen_w, cfg.screen_h,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* ── Start splash ── */
    if (!display_splash(renderer, cfg.start_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor))
        goto cleanup; /* user quit during splash */

    /* ── Font ── */
    TTF_Font *font = NULL;
    if (cfg.font_file) {
        font = TTF_OpenFont(cfg.font_file, (float)cfg.font_size);
        if (!font) SDL_Log("Failed to load font '%s': %s", cfg.font_file, SDL_GetError());
    }

    /* ── Parse stimuli ── */
    Experiment *exp = parse_csv(cfg.csv_file);
    if (!exp) {
        fprintf(stderr, "Failed to parse CSV: %s\n", cfg.csv_file);
        if (font) TTF_CloseFont(font);
        goto cleanup;
    }

    /* ── Audio mixer ── */
    AudioMixer mixer;
    memset(&mixer, 0, sizeof(mixer));
    mixer.mutex = SDL_CreateMutex();
    if (!mixer.mutex) {
        fprintf(stderr, "SDL_CreateMutex failed: %s\n", SDL_GetError());
        free_experiment(exp);
        if (font) TTF_CloseFont(font);
        goto cleanup;
    }
    SDL_AudioSpec target_spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *master_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &target_spec, audio_callback, &mixer);
    if (!master_stream) {
        fprintf(stderr, "SDL_OpenAudioDeviceStream failed: %s\n", SDL_GetError());
        SDL_DestroyMutex(mixer.mutex);
        free_experiment(exp);
        if (font) TTF_CloseFont(font);
        goto cleanup;
    }
    SDL_ResumeAudioStreamDevice(master_stream);

    /* ── Load resources ── */
    CacheEntry *cache = NULL;
    Resource *resources = load_resources(renderer, exp, font, &cache);
    if (!resources) {
        SDL_Log("load_resources: calloc failed");
        SDL_DestroyAudioStream(master_stream);
        SDL_DestroyMutex(mixer.mutex);
        free_experiment(exp);
        if (font) TTF_CloseFont(font);
        goto cleanup;
    }

    /* ── Run ── */
    EventLog log = {0};
    run_experiment(&cfg, exp, resources, renderer, &mixer, &log);

    /* ── Write results ── */
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(target_display);
    write_results(&cfg, &log, renderer, mode,
                  timestamp_str, username, hostname, cmd_line);

    /* ── End splash ── */
    display_splash(renderer, cfg.end_splash, cfg.screen_w, cfg.screen_h, cfg.scale_factor);

    /* ── Cleanup ── */
    free_event_log(&log);
    free_resources(resources, cache);
    SDL_DestroyAudioStream(master_stream);
    SDL_DestroyMutex(mixer.mutex);
    free_experiment(exp);
    if (font) TTF_CloseFont(font);

cleanup:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
