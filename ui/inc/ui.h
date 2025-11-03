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
 * @brief Initializes the UI system and loads the shared theme atlas.
 *
 * @param atlasPath Path to the ui.png spritesheet.
 * @return true on success, false otherwise.
 */
bool ui_init(const char* atlasPath);

/**
 * @brief Releases resources held by the UI system.
 */
void ui_shutdown(void);

/**
 * @brief Updates UI state (inventory, pause menu, settings...) based on input.
 *
 * @param input     Mutable input state (selection + key bindings).
 * @param entities  Entity system reference (for inventory tab population).
 * @param deltaTime Frame delta time.
 */
void ui_update(InputState* input, const EntitySystem* entities, float deltaTime);

/**
 * @brief Draws all UI overlays (inventory, pause/settings, hints).
 *
 * @param input    Current input state.
 * @param entities Entity system data for inventory rendering.
 */
void ui_draw(InputState* input, const EntitySystem* entities);

/**
 * @brief Indicates if the inventory window is currently open.
 */
bool ui_is_inventory_open(void);

/**
 * @brief Indicates if any modal UI (inventory, pause, settings) is capturing input.
 */
bool ui_is_input_blocked(void);

/**
 * @brief Indicates whether the game is currently paused through the menu.
 */
bool ui_is_paused(void);

/**
 * @brief Lets the main loop know the user picked the "Exit" option.
 */
bool ui_should_close_application(void);
