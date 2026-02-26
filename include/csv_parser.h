#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include "stimuli.h"

Experiment* parse_csv(const char *file_path);
void free_experiment(Experiment *exp);

#endif
