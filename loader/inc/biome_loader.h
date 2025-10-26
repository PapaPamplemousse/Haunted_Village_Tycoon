/**
 * @file biome_loader.h
 * @brief Functions and global declarations for loading and managing biome definitions.
 */
#ifndef BIOME_LOADER_H
#define BIOME_LOADER_H

#include "world.h"

/**
 * @brief Global array storing all loaded biome definitions.
 */
extern BiomeDef gBiomeDefs[];
/**
 * @brief Global variable storing the number of biome definitions loaded into gBiomeDefs.
 */
extern int gBiomeCount;

/**
 * @brief Loads biome definitions from a specified file.
 * @param path The file path to the biomes definition file (e.g., biomes.stv).
 */
void load_biome_definitions(const char* path);

/**
 * @brief Retrieves the descriptive name of a biome given its kind.
 * @param k The BiomeKind identifier.
 * @return The name of the biome as a constant string.
 */
const char* get_biome_name(BiomeKind k);

/**
 * @brief Retrieves the complete BiomeDef structure for a given biome kind.
 * @param kind The BiomeKind identifier.
 * @return A constant pointer to the BiomeDef structure, or NULL if the kind is invalid.
 */
const BiomeDef* get_biome_def(BiomeKind kind);

/**
 * @brief Converts a string representation of a biome kind to its BiomeKind enum value.
 * @param s The string to convert (e.g., "FOREST", "DESERT").
 * @return The corresponding BiomeKind enum value. Returns a default/invalid kind if the string is not recognized.
 */
BiomeKind biome_kind_from_string(const char* s);

/**
 * @brief Converts a BiomeKind enum value to its string representation.
 * @param k The BiomeKind identifier.
 * @return The string representation of the biome kind (e.g., "FOREST").
 */
const char* biome_kind_to_string(BiomeKind k);

#endif
