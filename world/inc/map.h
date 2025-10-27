/**
 * @file map.h
 * @brief Map management, rendering, and interaction system.
 *
 * This module provides functions for initializing, updating,
 * and rendering the world map. It manages both the static
 * terrain (tiles) and the dynamic elements (objects, buildings),
 * and handles user interactions such as highlighting, editing,
 * or detecting player actions on the map.
 *
 * @date 2025-10-23
 * @author Hugo
 */

#ifndef MAP_H
#define MAP_H

#include "world.h"

// -----------------------------------------------------------------------------
// FUNCTION DECLARATIONS
// -----------------------------------------------------------------------------

/**
 * @brief Initializes the map with default terrain and object data.
 *
 * This function allocates and fills the map structure with
 * initial tile types (e.g., grass, water, walls) and clears
 * all object references.
 *
 * @param[out] map Pointer to the map structure to initialize.
 * @param seed Random seed used for deterministic generation.
 *
 * @note This function should be called once before any rendering
 *       or world update occurs.
 */
void map_init(Map* map, unsigned int seed);

/**
 * @brief Unloads map-related resources such as textures or objects.
 *
 * This function frees memory and releases graphical assets
 * associated with the map and its elements.
 *
 * @note Call this when closing the game or reloading a new map.
 */
void map_unload(Map* map);

/**
 * @brief Renders the map and its contents to the active camera view.
 *
 * This function draws the visible portion of the map, including
 * terrain tiles and placed objects, using isometric or top-down
 * projection as defined in the rendering logic.
 *
 * @param[in] map Pointer to the map to render.
 * @param[in] camera Pointer to the active camera controlling the view.
 */
void draw_map(Map* map, Camera2D* camera);

/**
 * @brief Gets the tile type at the specified coordinates.
 *
 * @param[in] map Pointer to the map.
 * @param[in] x X coordinate (tile space).
 * @param[in] y Y coordinate (tile space).
 * @return The TileTypeID at the specified location.
 */
TileTypeID map_get_tile(Map* map, int x, int y);

/**
 * @brief Sets the tile type at the specified coordinates.
 *
 * @param[in,out] map Pointer to the map.
 * @param[in] x X coordinate (tile space).
 * @param[in] y Y coordinate (tile space).
 * @param[in] id TileTypeID to assign.
 */
void map_set_tile(Map* map, int x, int y, TileTypeID id);

/**
 * @brief Places a new object on the map, replacing any previous one.
 *
 * @param[in,out] map Pointer to the map.
 * @param[in] id Object type to place.
 * @param[in] x X coordinate (tile space).
 * @param[in] y Y coordinate (tile space).
 */
void map_place_object(Map* map, ObjectTypeID id, int x, int y);

/**
 * @brief Removes the object at the given tile coordinates, if any.
 *
 * @param[in,out] map Pointer to the map.
 * @param[in] x X coordinate (tile space).
 * @param[in] y Y coordinate (tile space).
 */
void map_remove_object(Map* map, int x, int y);

#endif /* MAP_H */
