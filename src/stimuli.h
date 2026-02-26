#ifndef STIMULI_H
#define STIMULI_H

#include <SDL3/SDL.h>

typedef enum {
    STIM_IMAGE,
    STIM_SOUND,
    STIM_TEXT,
    STIM_END
} StimType;

typedef struct {
    Uint64 timestamp_ms;
    Uint64 duration_ms;
    StimType type;
    char file_path[256];
} Stimulus;

typedef struct {
    Stimulus *stimuli;
    int count;
} Experiment;

#endif
