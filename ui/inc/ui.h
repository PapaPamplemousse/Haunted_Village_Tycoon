// ui.h
#pragma once

#include "input.h"
#include <stdbool.h>
#include <raylib.h>

void ui_update_inventory(InputState* input);
void ui_draw_inventory(const InputState* input);
void ui_toggle_inventory(void);
bool ui_is_inventory_open(void);
