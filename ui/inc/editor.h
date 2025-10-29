/**
 * @file editor.h
 * @brief High-level editing logic that connects user input with the map system.
 *
 * This module interprets user actions (mouse clicks, selected tiles/objects)
 * and applies the corresponding modifications to the map.
 *
 * It acts as the bridge between the input system and the map data.
 */

#ifndef EDITOR_H
#define EDITOR_H

#include "map.h"
#include "input.h"
#include "entity.h"
/**
 * @brief Processes user interactions and updates the map accordingly.
 *
 * This function handles:
 *  - Tile placement (left-click)
 *  - Object placement/removal
 *  - Terrain reset (right-click)
 *
 * @param[in,out] map Pointer to the map being edited.
 * @param[in] camera Pointer to the active 2D camera (used for mouse-to-world conversion).
 * @param[in] input Current input state (selected tile/object).
 * @param[out] dirtyWorldRect Optional rectangle (in world coordinates) describing the
 *             modified area for systems that need to react incrementally. Can be NULL.
 * @return true if the map content changed (e.g., tiles or objects were modified).
 */
bool editor_update(Map* map, Camera2D* camera, InputState* input, EntitySystem* entities, Rectangle* dirtyWorldRect);

#endif // EDITOR_H
