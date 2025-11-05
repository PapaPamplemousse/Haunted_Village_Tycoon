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

struct Entity;
struct EntitySystem;

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// QUERIES
// -----------------------------------------------------------------------------

/** Maximum number of generated structures tracked simultaneously. */
#define MAX_GENERATED_BUILDINGS MAX_BUILDINGS
/** Maximum number of player-created structures tracked simultaneously. */
#define MAX_PLAYER_BUILDINGS MAX_BUILDINGS

/** Returns the number of procedurally generated structures currently tracked. */
int building_generated_count(void);

/** Returns the number of player-created buildings currently tracked. */
int building_player_count(void);

/** Returns the total number of buildings (generated + player-built). */
int building_total_count(void);

/** Retrieves a read-only pointer to a building by global index. */
const Building* building_get(int index);

/** Retrieves a mutable pointer to a building by global index. */
Building* building_get_mutable(int index);

/** Retrieves a read-only pointer to a generated structure by index. */
const Building* building_get_generated(int index);

/** Retrieves a read-only pointer to a player-created building by index. */
const Building* building_get_player(int index);

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
 * @param worldRegion World-space rectangle describing the area that needs to be rescanned.
 *                    The detector automatically pads this region slightly to capture
 *                    room boundaries. Passing a zero-sized rectangle triggers a full rebuild.
 *
 * @note This function rebuilds the generated/player registries in-place.
 *       It should be called whenever the world layout changes (e.g., walls added/removed).
 */
void update_building_detection(Map* map, Rectangle worldRegion);

/**
 * @brief Marks a generated structure's footprint so it can be categorized later.
 *
 * @param[in,out] map Pointer to the current world map.
 * @param bounds Axis-aligned rectangle delimiting the building interior in tiles.
 */
void register_building_from_bounds(Map* map, Rectangle bounds, StructureKind kind);

void register_building_with_metadata(Map* map, Rectangle bounds, StructureKind kind, int speciesId, int villageId);

void building_add_resident(Building* b, struct Entity* e);
void building_remove_resident(Building* b, uint16_t entityId);
Building* entity_get_home(const struct Entity* e);
Building* building_get_for_species(const char* species, int villageId);
int building_active_residents(const Building* b, const struct EntitySystem* sys);
Building* building_get_at_tile(int tileX, int tileY);
void building_debug_print(const Building* b, const struct EntitySystem* sys);

/**
 * @brief Clears structure kind markers stored on the map grid.
 */
void building_clear_structure_markers(void);

/**
 * @brief Marks that a reserved resident has been instantiated for a building.
 */
void building_on_reservation_spawn(int buildingId);

/**
 * @brief Marks that a reserved resident has been hibernated for a building.
 */
void building_on_reservation_hibernate(int buildingId);

#endif /* BUILDING_H */
