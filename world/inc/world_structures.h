/**
 * @file world_structures.h
 * @brief Defines data structures and functions for world structure generation.
 *
 * This header file provides the blueprint for defining and managing different
 * types of structures (like huts, crypts, temples) that can be generated
 * within a game world, associating them with specific biomes.
 */
#ifndef WORLD_STRUCTURES_H
#define WORLD_STRUCTURES_H

#include <stdint.h>
#include "world.h"
#include "map.h"
#include "object.h"

/**
 * @brief Selects a random structure definition appropriate for a given biome.
 *
 * The selection is weighted by the @c rarity field of the StructureDef.
 * @param biome The type of biome for which a structure is being sought.
 * @param rng Pointer to the random number generator state.
 * @param structureCounts Array tracking how many instances of each structure
 *                        kind have already been placed (may be NULL for no
 *                        limit checks).
 * @return A constant pointer to the selected StructureDef, or NULL if no
 * structure is defined for the biome.
 */
const StructureDef* pick_structure_for_biome(BiomeKind biome, uint64_t* rng, const int* structureCounts);

/**
 * @brief Retrieves the immutable definition associated with a structure kind.
 */
/**
 * @brief Retrieves the immutable definition associated with a structure kind.
 *
 * @param kind Structure identifier to look up.
 * @return Pointer to the definition or NULL if the kind is unknown.
 */
const StructureDef* get_structure_def(StructureKind kind);

/**
 * @brief Converts a textual structure identifier into its enumeration value.
 *
 * @param name Case-insensitive string representation.
 * @return Matching structure kind, or STRUCT_COUNT if not recognized.
 */
StructureKind structure_kind_from_string(const char* name);

/**
 * @brief Converts a structure enumeration value to its textual identifier.
 *
 * @param kind Structure kind to stringify.
 * @return Static string describing the kind, or "UNKNOWN" for invalid values.
 */
const char* structure_kind_to_string(StructureKind kind);

/**
 * @brief Loads metadata overrides for structures from an STV file.
 */
void load_structure_metadata(const char* path);

/// @name Concrete Structure Generators
/// @brief Functions that implement the actual map modifications for specific structures.
/// @note These functions are implemented in @c world_structures.c and are
/// typically assigned to the @c build callback in a @c StructureDef.
/// @{
void build_hut_cannibal(Map* map, int x, int y, uint64_t* rng);
void build_crypt(Map* map, int x, int y, uint64_t* rng);
void build_ruin(Map* map, int x, int y, uint64_t* rng);
void build_village_house(Map* map, int x, int y, uint64_t* rng);
void build_temple(Map* map, int x, int y, uint64_t* rng);
void build_witch_hovel(Map* map, int x, int y, uint64_t* rng);
void build_gallows(Map* map, int x, int y, uint64_t* rng);
void build_blood_garden(Map* map, int x, int y, uint64_t* rng);
void build_flesh_pit(Map* map, int x, int y, uint64_t* rng);
void build_void_obelisk(Map* map, int x, int y, uint64_t* rng);
void build_plague_nursery(Map* map, int x, int y, uint64_t* rng);

#endif // WORLD_STRUCTURES_H
