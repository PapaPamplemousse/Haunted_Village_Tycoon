/**
 * @file debug.h
 * @brief Declares helper utilities for rendering biome debug overlays.
 */

#ifndef DEBUG_BIOME_H
#define DEBUG_BIOME_H

#include "world.h"
#include "map.h"
#include "raylib.h"

/**
 * @brief Toggles and renders biome debug visualization.
 *
 * @param map World map to inspect.
 * @param cam Active camera used for coordinate conversion.
 * @param showDebug Pointer to a flag that toggles overlay visibility.
 */
void debug_biome_draw(Map* map, Camera2D* cam, bool* showDebug);

#endif
