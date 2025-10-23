/**
 * @file tile.h
 * @brief Tile type definition and resource management system.
 *
 * This module provides initialization, access, and cleanup routines
 * for all available tile types in the world. It manages the
 * `TileType` definitions that describe visual appearance,
 * walkability, and interaction properties of each terrain tile.
 *
 * Typical usage:
 * - Call @ref init_tile_types() during game startup.
 * - Retrieve tile definitions via @ref get_tile_type().
 * - Call @ref unload_tile_types() on shutdown to release resources.
 *
 * @date 2025-10-23
 * @author Hugo
 */

#ifndef TILE_H
#define TILE_H

#include "world.h"

// -----------------------------------------------------------------------------
// FUNCTION DECLARATIONS
// -----------------------------------------------------------------------------

/**
 * @brief Initializes the global list of available tile types.
 *
 * This function sets up the tile definitions (name, category,
 * color, and properties) and loads associated textures if required.
 *
 * @note Must be called once before any tile rendering or map generation.
 */
void init_tile_types(void);

/**
 * @brief Releases all textures and resources used by tile types.
 *
 * This function unloads any textures or dynamic resources
 * allocated during @ref init_tile_types().
 *
 * @note Should be called when exiting the game or unloading a world.
 */
void unload_tile_types(void);

/**
 * @brief Retrieves a pointer to a specific tile type definition.
 *
 * @param[in] id Unique identifier of the tile type to retrieve.
 * @return Pointer to the corresponding @ref TileType structure,
 *         or `NULL` if the ID is invalid or undefined.
 *
 * @note The returned pointer refers to a static definition and
 *       must not be freed or modified directly.
 */
TileType* get_tile_type(TileTypeID id);

#endif /* TILE_H */
