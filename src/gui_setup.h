/*
 * Copyright (C) Christophe Pallier <Christophe@pallier.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef GUI_SETUP_H
#define GUI_SETUP_H

#include "config.h"

/**
 * @brief Runs the interactive GUI setup window.
 * 
 * @param cfg Pointer to the Config structure to be updated.
 * @return true if the user clicked START, false if they closed the window.
 */
bool run_gui_setup(Config *cfg);

#endif // GUI_SETUP_H
