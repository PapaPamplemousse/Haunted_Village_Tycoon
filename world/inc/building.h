/**
 * @file building.h
 * @brief Building detection and management system.
 *
 * This module provides data structures and functions used to detect,
 * store, and manage buildings (enclosed areas) within the world map.
 * A "building" is typically defined as a contiguous enclosed space
 * surrounded by wall-type tiles and possibly containing objects or
 * furniture. The detection process analyzes the world map to identify
 * these enclosed regions and populate the global building list.
 *
 * @date 2025-10-23
 * @author Hugo
 */

#ifndef BUILDING_H
#define BUILDING_H

#include "world.h"

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// GLOBAL VARIABLES
// -----------------------------------------------------------------------------

/**
 * @var buildings
 * @brief Global array storing all detected buildings on the map.
 *
 * Each element contains detailed metadata such as bounds, center,
 * room type classification, and contained objects.
 */
extern Building buildings[MAX_BUILDINGS];

/**
 * @var buildingCount
 * @brief Number of currently detected buildings in the world.
 *
 * This counter indicates how many valid entries exist in the
 * @ref buildings array.
 */
extern int buildingCount;

// -----------------------------------------------------------------------------
// FUNCTIONS
// -----------------------------------------------------------------------------

/**
 * @brief Detects enclosed buildings within the given map and updates the global building list.
 *
 * This function performs a flood-fill or contour search on the map to locate
 * enclosed areas bounded by structural wall tiles. Each enclosed area is
 * registered as a building, and its properties (bounds, area, contained objects)
 * are calculated and stored.
 *
 * @param[in,out] map Pointer to the game map structure containing tiles and objects.
 *
 * @note This function updates the global variables @ref buildings and @ref buildingCount.
 *       It should be called whenever the world layout changes (e.g., walls added/removed).
 */
void update_building_detection(Map* map);

/**
 * @brief Adds a building entry using an explicit bounding box.
 *
 * @param[in,out] map Pointer to the current world map.
 * @param bounds Axis-aligned rectangle delimiting the building interior in tiles.
 * @return Pointer to the registered building data, or NULL on failure.
 */
Building* register_building_from_bounds(Map* map, Rectangle bounds);

#endif /* BUILDING_H */
