#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "csv_parser.h"

#ifdef _WIN32
#include <windows.h>
#define gethostname(name, len) GetComputerNameA(name, &(DWORD){len})
#define getusername(name, len) GetUserNameA(name, &(DWORD){len})
#else
#include <unistd.h>
#endif

#define MAX_ACTIVE_SOUNDS 16
#define CROSS_SIZE 20

typedef struct {
    Uint8 *data;
    Uint32 len;
    SDL_AudioSpec spec;
} SoundResource;

typedef struct {
    SDL_Texture *texture;
    float w, h;
    SoundResource sound;
} Resource;

typedef struct {
    const SoundResource *resource;
    Uint32 play_pos;
    bool active;
} ActiveSound;

typedef struct {
    ActiveSound slots[MAX_ACTIVE_SOUNDS];
    SDL_Mutex *mutex;
} AudioMixer;

typedef struct {
    Uint64 timestamp_ms;
    char type[16];
    char label[256];
} EventLogEntry;

typedef struct {
    EventLogEntry *entries;
    int count;
    int capacity;
} EventLog;

AudioMixer mixer;

// Dialog state
char dialog_path[512] = "";
bool dialog_waiting = false;

void SDLCALL file_dialog_callback(void *userdata, const char * const *filelist, int filter) {
    (void)filter;
    char *target = (char *)userdata;
    if (filelist && filelist[0]) {
        strncpy(target, filelist[0], 511);
    } else {
        target[0] = '\0'; // Cancelled
    }
    dialog_waiting = false;
}

void log_event(EventLog *log, Uint64 time, const char *type, const char *label) {
    if (log->count >= log->capacity) {
        log->capacity = log->capacity == 0 ? 64 : log->capacity * 2;
        log->entries = realloc(log->entries, log->capacity * sizeof(EventLogEntry));
    }
    log->entries[log->count].timestamp_ms = time;
    strncpy(log->entries[log->count].type, type, 15);
    strncpy(log->entries[log->count].label, label, 255);
    log->count++;
}

void SDLCALL audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
    (void)userdata; (void)total_amount;
    Uint8 *buffer = malloc(additional_amount);
    if (!buffer) return;
    memset(buffer, 0, additional_amount);

    SDL_LockMutex(mixer.mutex);
    for (int i = 0; i < MAX_ACTIVE_SOUNDS; i++) {
        if (!mixer.slots[i].active) continue;
        ActiveSound *s = &mixer.slots[i];
        Uint32 remaining = s->resource->len - s->play_pos;
        Uint32 len = (additional_amount > (int)remaining) ? remaining : (Uint32)additional_amount;
        SDL_MixAudio(buffer, s->resource->data + s->play_pos, s->resource->spec.format, (float)len, 1.0f);
        s->play_pos += len;
        if (s->play_pos >= s->resource->len) s->active = false;
    }
    SDL_UnlockMutex(mixer.mutex);
    SDL_PutAudioStreamData(stream, buffer, additional_amount);
    free(buffer);
}

void draw_fixation_cross(SDL_Renderer *renderer, int screen_w, int screen_h) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    int mid_x = screen_w / 2;
    int mid_y = screen_h / 2;
    SDL_RenderLine(renderer, (float)mid_x - CROSS_SIZE, (float)mid_y, (float)mid_x + CROSS_SIZE, (float)mid_y);
    SDL_RenderLine(renderer, (float)mid_x, (float)mid_y - CROSS_SIZE, (float)mid_x, (float)mid_y + CROSS_SIZE);
}

void display_splash(SDL_Renderer *renderer, const char *file_path, int screen_w, int screen_h, float scale_factor) {
    if (!file_path) return;
    SDL_Texture *tex = IMG_LoadTexture(renderer, file_path);
    if (!tex) {
        SDL_Log("Failed to load splash: %s", file_path);
        return;
    }
    float w, h;
    SDL_GetTextureSize(tex, &w, &h);
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_FRect dst_rect = {(screen_w - (w * scale_factor)) / 2.0f, (screen_h - (h * scale_factor)) / 2.0f, w * scale_factor, h * scale_factor};
    SDL_RenderTexture(renderer, tex, NULL, &dst_rect);
    SDL_RenderPresent(renderer);

    bool waiting = true;
    SDL_Event event;
    while (waiting) {
        if (SDL_WaitEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exit(0);
            } else if (event.type == SDL_EVENT_KEY_DOWN) {
                waiting = false;
            }
        }
    }
    SDL_DestroyTexture(tex);
}

int main(int argc, char *argv[]) {
    // Initializing SDL first to enable dialogs/GUI mode
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return 1;
    TTF_Init();

    bool use_fixation = false;
    bool fullscreen = false;
    bool vsync = true;
    int display_index = 0;
    int screen_w = 1920;
    int screen_h = 1080;
    float scale_factor = 1.0f;
    char *csv_file = NULL;
    char *output_file = NULL;
    char *start_splash = NULL;
    char *end_splash = NULL;
    char *font_file = NULL;
    int font_size = 24;
    Uint64 total_duration = 0;
    char *stimuli_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s <stimuli_csv_file> [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --output [file]      Output log file\n");
            printf("  --fixation           Show fixation cross between stimuli\n");
            printf("  --fullscreen         Run in fullscreen mode\n");
            printf("  --display [index]    Display monitor index\n");
            printf("  --res [WxH]          Resolution (default: 1920x1080)\n");
            printf("  --scale [factor]     Image magnification factor\n");
            printf("  --start-splash [img] Splashscreen at start\n");
            printf("  --end-splash [img]   Splashscreen at end\n");
            printf("  --font [file]        TTF font file for text stimuli\n");
            printf("  --font-size [pt]     Font size in points\n");
            printf("  --total-duration [ms] Minimum experiment duration\n");
            printf("  --stimuli-dir [dir]  Stimuli directory (relative to CSV)\n");
            printf("  --no-vsync           Disable VSYNC\n");
            return 0;
        }
        if (strcmp(argv[i], "--fixation") == 0) use_fixation = true;
        else if (strcmp(argv[i], "--fullscreen") == 0) fullscreen = true;
        else if (strcmp(argv[i], "--no-vsync") == 0) vsync = false;
        else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output_file = argv[++i];
        else if (strcmp(argv[i], "--display") == 0 && i + 1 < argc) display_index = atoi(argv[++i]);
        else if (strcmp(argv[i], "--res") == 0 && i + 1 < argc) sscanf(argv[++i], "%dx%d", &screen_w, &screen_h);
        else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) scale_factor = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--start-splash") == 0 && i + 1 < argc) start_splash = argv[++i];
        else if (strcmp(argv[i], "--end-splash") == 0 && i + 1 < argc) end_splash = argv[++i];
        else if (strcmp(argv[i], "--font") == 0 && i + 1 < argc) font_file = argv[++i];
        else if (strcmp(argv[i], "--font-size") == 0 && i + 1 < argc) font_size = atoi(argv[++i]);
        else if (strcmp(argv[i], "--total-duration") == 0 && i + 1 < argc) total_duration = (Uint64)atoll(argv[++i]);
        else if (strcmp(argv[i], "--stimuli-dir") == 0 && i + 1 < argc) stimuli_dir = argv[++i];
        else if (csv_file == NULL) csv_file = argv[i];
    }

    // GUI Mode if no CSV provided
    if (csv_file == NULL) {
        SDL_DialogFileFilter filters[] = { {"CSV Files", "csv"}, {"All Files", "*"} };
        dialog_waiting = true;
        SDL_ShowOpenFileDialog(file_dialog_callback, dialog_path, NULL, filters, 2, NULL, false);
        while (dialog_waiting) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) exit(0);
            SDL_Delay(16);
        }
        if (dialog_path[0] == '\0') return 0;
        csv_file = strdup(dialog_path);

        if (output_file == NULL) {
            dialog_waiting = true;
            SDL_ShowSaveFileDialog(file_dialog_callback, dialog_path, NULL, filters, 2, "results.csv");
            while (dialog_waiting) {
                SDL_Event e;
                while (SDL_PollEvent(&e)) if (e.type == SDL_EVENT_QUIT) exit(0);
                SDL_Delay(16);
            }
            output_file = (dialog_path[0] != '\0') ? strdup(dialog_path) : "results.csv";
        }

        const SDL_MessageBoxButtonData buttons[] = {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "1920x1080" },
            { 0, 1, "2560x1440" },
            { 0, 2, "3840x2160" },
        };
        const SDL_MessageBoxData messageboxdata = { SDL_MESSAGEBOX_INFORMATION, NULL, "Resolution", "Select the experiment resolution:", SDL_arraysize(buttons), buttons, NULL };
        int buttonid;
        if (SDL_ShowMessageBox(&messageboxdata, &buttonid) == 0) {
            if (buttonid == 1) { screen_w = 2560; screen_h = 1440; }
            else if (buttonid == 2) { screen_w = 3840; screen_h = 2160; }
        }
    } else {
        if (output_file == NULL) output_file = "results.csv";
    }

    // Capture metadata
    time_t rawtime; time(&rawtime);
    char *timestamp_str = ctime(&rawtime);
    timestamp_str[strlen(timestamp_str) - 1] = '\0';
    char hostname[256] = "unknown", username[256] = "unknown";
#ifdef _WIN32
    DWORD hlen = sizeof(hostname); GetComputerNameA(hostname, &hlen);
    DWORD ulen = sizeof(username); GetUserNameA(username, &ulen);
#else
    gethostname(hostname, sizeof(hostname));
    char *login = getlogin();
    if (login) strncpy(username, login, sizeof(username) - 1);
    else { char *env_user = getenv("USER"); if (env_user) strncpy(username, env_user, sizeof(username) - 1); }
#endif

    char cmd_line[1024] = "";
    for (int i = 0; i < argc; i++) {
        strncat(cmd_line, argv[i], sizeof(cmd_line) - strlen(cmd_line) - 1);
        if (i < argc - 1) strncat(cmd_line, " ", sizeof(cmd_line) - strlen(cmd_line) - 1);
    }

    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    SDL_DisplayID target_display = (display_index < num_displays) ? displays[display_index] : displays[0];
    SDL_free(displays);

    Uint32 window_flags = 0;
    if (fullscreen) window_flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window *window = SDL_CreateWindow("expe3000", screen_w, screen_h, window_flags);
    if (fullscreen) SDL_SetWindowPosition(window, SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display), SDL_WINDOWPOS_UNDEFINED_DISPLAY(target_display));
    
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (vsync) SDL_SetRenderVSync(renderer, 1);
    SDL_SetRenderLogicalPresentation(renderer, screen_w, screen_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    display_splash(renderer, start_splash, screen_w, screen_h, scale_factor);

    TTF_Font *font = NULL;
    if (font_file) {
        font = TTF_OpenFont(font_file, (float)font_size);
        if (font) {
            SDL_Log("Loaded font: %s", font_file);
        } else {
            SDL_Log("Failed to load font '%s': %s", font_file, SDL_GetError());
        }
    } else {
        const char *default_fonts[] = {
            "fonts/Inconsolata.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "C:\\Windows\\Fonts\\arial.ttf",
            "/System/Library/Fonts/Helvetica.ttc"
        };
        for (int i = 0; i < 5; i++) {
            font = TTF_OpenFont(default_fonts[i], (float)font_size);
            if (font) {
                SDL_Log("Loaded default font: %s", default_fonts[i]);
                break;
            }
        }
        if (!font) SDL_Log("Warning: No font loaded. Text stimuli will not be displayed.");
    }

    Experiment *exp = parse_csv(csv_file);
    if (!exp) return 1;

    // Determine base path for stimuli: directory of csv_file + stimuli_dir
    char base_path[512] = "";
    const char *last_slash = strrchr(csv_file, '/');
    const char *last_backslash = strrchr(csv_file, '\\');
    const char *sep = (last_slash > last_backslash) ? last_slash : last_backslash;
    if (sep) {
        size_t dir_len = sep - csv_file + 1;
        if (dir_len < sizeof(base_path)) {
            strncpy(base_path, csv_file, dir_len);
            base_path[dir_len] = '\0';
        }
    }
    if (stimuli_dir) {
        strncat(base_path, stimuli_dir, sizeof(base_path) - strlen(base_path) - 1);
        size_t len = strlen(base_path);
        if (len > 0 && base_path[len-1] != '/' && base_path[len-1] != '\\') {
#ifdef _WIN32
            strncat(base_path, "\\", sizeof(base_path) - len - 1);
#else
            strncat(base_path, "/", sizeof(base_path) - len - 1);
#endif
        }
    }

    memset(&mixer, 0, sizeof(mixer));
    mixer.mutex = SDL_CreateMutex();
    SDL_AudioSpec target_spec = { SDL_AUDIO_S16, 2, 44100 };
    SDL_AudioStream *master_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &target_spec, audio_callback, NULL);
    SDL_ResumeAudioStreamDevice(master_stream);

    SDL_Log("Loading resources...");
    Resource *resources = calloc(exp->count, sizeof(Resource));
    int image_count = 0, sound_count = 0, text_count = 0;
    size_t total_memory = 0;

    for (int i = 0; i < exp->count; i++) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s%s", base_path, exp->stimuli[i].file_path);

        if (exp->stimuli[i].type == STIM_IMAGE) {
            resources[i].texture = IMG_LoadTexture(renderer, full_path);
            if (resources[i].texture) {
                SDL_GetTextureSize(resources[i].texture, &resources[i].w, &resources[i].h);
                image_count++;
                total_memory += (size_t)(resources[i].w * resources[i].h * 4);
            } else {
                SDL_Log("Failed to load image: %s", full_path);
            }
        } else if (exp->stimuli[i].type == STIM_SOUND) {
            if (SDL_LoadWAV(full_path, &resources[i].sound.spec, &resources[i].sound.data, &resources[i].sound.len)) {
                sound_count++;
                total_memory += resources[i].sound.len;
            } else {
                SDL_Log("Failed to load sound: %s", full_path);
            }
        } else if (exp->stimuli[i].type == STIM_TEXT && font) {
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface *surf = TTF_RenderText_Blended(font, exp->stimuli[i].file_path, 0, white);
            if (surf) {
                resources[i].texture = SDL_CreateTextureFromSurface(renderer, surf);
                resources[i].w = (float)surf->w; resources[i].h = (float)surf->h;
                total_memory += (size_t)(resources[i].w * resources[i].h * 4);
                text_count++;
                SDL_DestroySurface(surf);
            }
        }
    }

    SDL_Log("Resources loaded: %d images, %d sounds, %d text textures. Total estimated memory: %.2f MB",
            image_count, sound_count, text_count, (double)total_memory / (1024.0 * 1024.0));

    float refresh_rate = 60.0f;
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(target_display);
    if (mode && mode->refresh_rate > 0) refresh_rate = mode->refresh_rate;
    Uint64 frame_duration_ms = (Uint64)(1000.0f / refresh_rate);
    Uint64 look_ahead_ms = frame_duration_ms / 2;

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

        if (current_stim < exp->count && (current_time + look_ahead_ms) >= exp->stimuli[current_stim].timestamp_ms) {
            Stimulus *s = &exp->stimuli[current_stim];
            if ((s->type == STIM_IMAGE || s->type == STIM_TEXT) && resources[current_stim].texture) {
                active_visual_idx = current_stim;
                visual_end_time = current_time + s->duration_ms;
                triggered_onset = true; triggered_idx = current_stim;
            } else if (s->type == STIM_SOUND && resources[current_stim].sound.data) {
                SDL_LockMutex(mixer.mutex);
                for (int j = 0; j < MAX_ACTIVE_SOUNDS; j++) {
                    if (!mixer.slots[j].active) {
                        mixer.slots[j].resource = &resources[current_stim].sound;
                        mixer.slots[j].play_pos = 0; mixer.slots[j].active = true;
                        log_event(&log, current_time, "SOUND_ONSET", s->file_path);
                        break;
                    }
                }
                SDL_UnlockMutex(mixer.mutex);
            }
            current_stim++;
            fprintf(stdout, "\rStimulus: %d/%d ", current_stim, exp->count);
            fflush(stdout);
        }

        if (active_visual_idx != -1 && current_time >= visual_end_time) {
            log_event(&log, current_time, (exp->stimuli[active_visual_idx].type == STIM_IMAGE ? "IMAGE_OFFSET" : "TEXT_OFFSET"), exp->stimuli[active_visual_idx].file_path);
            active_visual_idx = -1;
        }

        if (current_stim >= exp->count && active_visual_idx == -1 && current_time >= total_duration) running = false;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (active_visual_idx != -1) {
            Resource *r = &resources[active_visual_idx];
            SDL_FRect dst_rect = {(screen_w - (r->w * scale_factor)) / 2.0f, (screen_h - (r->h * scale_factor)) / 2.0f, r->w * scale_factor, r->h * scale_factor};
            SDL_RenderTexture(renderer, r->texture, NULL, &dst_rect);
        } else if (use_fixation) {
            draw_fixation_cross(renderer, screen_w, screen_h);
        }

        SDL_RenderPresent(renderer); 
        if (triggered_onset) {
            Uint64 onset_time = SDL_GetTicks() - start_time;
            log_event(&log, onset_time, (exp->stimuli[triggered_idx].type == STIM_IMAGE ? "IMAGE_ONSET" : "TEXT_ONSET"), exp->stimuli[triggered_idx].file_path);
        }
        if (!vsync) SDL_Delay(1);
    }
    printf("\n");

    FILE *res_file = fopen(output_file, "w");
    if (res_file) {
        fprintf(res_file, "# expe3000 Experiment Results\n# Date/Time: %s\n# User/Host: %s@%s\n# Command Line: %s\n", timestamp_str, username, hostname, cmd_line);
        fprintf(res_file, "# Platform: %s\n# Video Driver: %s\n# Renderer: %s (VSYNC: %s)\n", SDL_GetPlatform(), SDL_GetCurrentVideoDriver(), SDL_GetRendererName(renderer), vsync ? "ON" : "OFF");
        if (mode) fprintf(res_file, "# Display Resolution: %dx%d @ %.2fHz\n", mode->w, mode->h, mode->refresh_rate);
        int version = SDL_GetVersion();
        fprintf(res_file, "# SDL Version: %d.%d.%d\n# Logical Resolution: %dx%d\n# Scale Factor: %.2f\n# \n", SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version), screen_w, screen_h, scale_factor);
        fprintf(res_file, "timestamp_ms,event_type,label\n");
        for (int i = 0; i < log.count; i++) fprintf(res_file, "%lu,%s,%s\n", (unsigned long)log.entries[i].timestamp_ms, log.entries[i].type, log.entries[i].label);
        fclose(res_file);
        SDL_Log("Results saved to: %s", output_file);
    }

    display_splash(renderer, end_splash, screen_w, screen_h, scale_factor);
    if (font) TTF_CloseFont(font);
    free(log.entries); SDL_DestroyAudioStream(master_stream);
    for (int i = 0; i < exp->count; i++) { if (resources[i].texture) SDL_DestroyTexture(resources[i].texture); if (resources[i].sound.data) SDL_free(resources[i].sound.data); }
    free(resources); free_experiment(exp); SDL_DestroyMutex(mixer.mutex); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit();
    return 0;
}
