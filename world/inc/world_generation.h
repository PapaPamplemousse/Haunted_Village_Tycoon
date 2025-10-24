/**
 * @file world_generation.h
 * @brief Functions and configuration for the main world generation process.
 *
 * This header defines the high-level interface for setting up and executing
 * the generation of a game world (Map) based on configurable parameters
 * and a given seed.
 */

#ifndef WORLD_GENERATION_H
#define WORLD_GENERATION_H

#include <stdint.h>
#include "world.h"
#include "map.h"

// ---------------------- Biome Generation via Voronoi Centers --------------------------
/** @name Configuration and Initialization */
/// @{

/**
 * @brief Initializes the random number generator for world generation.
 *
 * This function should be called first to ensure deterministic generation
 * based on the provided seed.
 * @param seed The 64-bit seed value to use for generation.
 */
void worldgen_seed(uint64_t seed);

/**
 * @brief Sets the high-level configuration parameters for world generation.
 *
 * This function must be called before @ref generate_world to define the
 * characteristics (like biome distribution and structure density) of the
 * world to be generated.
 * @param params Pointer to the structure containing all generation parameters.
 */
void worldgen_config(const WorldGenParams* params);

/// @}

/**
 * @brief Generates the entire world map.
 *
 * This is the main entry point for the world generation process. It handles
 * placing tiles, generating biomes (potentially via Voronoi centers), and
 * spawning objects and structures.
 *
 * @note @ref worldgen_seed and @ref worldgen_config must be called prior
 * to this function.
 *
 * @param map Pointer to the Map structure where the world will be generated.
 */
void generate_world(Map* map);

#endif // WORLD_GENERATION_H
