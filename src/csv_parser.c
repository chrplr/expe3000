#include <SDL3/SDL.h>
#include "csv_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

Experiment* parse_csv(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("Error opening CSV file");
        return NULL;
    }

    Experiment *exp = malloc(sizeof(Experiment));
    exp->stimuli = NULL;
    exp->count = 0;

    char line[512];
    Uint64 last_timestamp = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == ' ') continue;

        exp->stimuli = realloc(exp->stimuli, (exp->count + 1) * sizeof(Stimulus));
        Stimulus *s = &exp->stimuli[exp->count];

        char type_str[16];
        if (sscanf(line, "%" SCNu64 ",%" SCNu64 ",%15[^,],%255[^,\n\r]", &s->timestamp_ms, &s->duration_ms, type_str, s->file_path) == 4) {
            if (exp->count > 0 && s->timestamp_ms < last_timestamp) {
                fprintf(stderr, "Error: Stimulus at line %d has a timestamp (%" PRIu64 ") smaller than the previous one (%" PRIu64 "). The CSV file must be sorted by the first column.\n", exp->count + 1, s->timestamp_ms, last_timestamp);
                fclose(file);
                free_experiment(exp);
                return NULL;
            }
            last_timestamp = s->timestamp_ms;

            if (strcmp(type_str, "IMAGE") == 0) s->type = STIM_IMAGE;
            else if (strcmp(type_str, "SOUND") == 0) s->type = STIM_SOUND;
            else if (strcmp(type_str, "TEXT") == 0) s->type = STIM_TEXT;
            else s->type = STIM_END;
            exp->count++;
        }
    }

    fclose(file);
    SDL_Log("parse_csv: read %d events from '%s'", exp->count, file_path);
    return exp;
}

void free_experiment(Experiment *exp) {
    if (exp) {
        if (exp->stimuli) free(exp->stimuli);
        free(exp);
    }
}
