/**
 * @file tile_loader.h
 * @brief Function for loading tile definitions from an external file.
 */
#ifndef TILE_LOADER_H
#define TILE_LOADER_H

#include "world.h"

/**
 * @brief Loads tile definitions from an .stv file.
 *
 * This function parses the specified file, expecting it to contain definitions
 * for various tile types, and stores them in the provided output array.
 *
 * @param path Path to the tiles definition file (e.g., tiles.stv).
 * @param outArray Output array to store the loaded TileType structures.
 * @param maxTiles Maximum number of tiles the output array can hold.
 * @return The number of tile types successfully loaded. Returns -1 on error.
 */
int load_tiles_from_stv(const char* path, TileType* outArray, int maxTiles);

#endif // TILE_LOADER_H
