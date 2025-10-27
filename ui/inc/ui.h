/**
 * @file ui.h
 * @brief Declares utility functions for the in-game editor UI widgets.
 */
#pragma once

#include "input.h"
#include <stdbool.h>
#include <raylib.h>
#include "entity.h"

/**
 * @brief Updates the inventory UI state based on user input.
 *
 * @param[in,out] input Mutable input state that stores selection and toggle information.
 */
void ui_update_inventory(InputState* input, const EntitySystem* entities);

/**
 * @brief Draws the inventory window and selection hints.
 *
 * @param[in] input Current input state used to highlight active selections.
 */
void ui_draw_inventory(const InputState* input, const EntitySystem* entities);

/**
 * @brief Toggles the visibility of the inventory window.
 */
void ui_toggle_inventory(void);

/**
 * @brief Indicates whether the inventory panel is currently visible.
 *
 * @return true if the panel is open, false otherwise.
 */
bool ui_is_inventory_open(void);
