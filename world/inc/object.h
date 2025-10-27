/**
 * @file object.h
 * @brief Object management, rendering, and classification system.
 *
 * This module defines functions for creating, retrieving, drawing,
 * and classifying in-game objects placed on the world map.
 * It provides an abstraction layer over the raw `Object` and
 * `ObjectType` data structures, used for interaction, rendering,
 * and room-type analysis.
 *
 * @date 2025-10-23
 * @author Hugo
 */

#ifndef OBJECT_H
#define OBJECT_H

#include "world.h"

// -----------------------------------------------------------------------------
// FUNCTION DECLARATIONS
// -----------------------------------------------------------------------------

/**
 * @brief Initializes object definitions and loads their textures.
 */
void init_objects(void);

/**
 * @brief Releases textures and resources associated with object definitions.
 */
void unload_object_textures(void);

/**
 * @brief Retrieves a pointer to the definition of an object type by its ID.
 *
 * @param[in] id Unique identifier of the object type to look up.
 * @return Pointer to the corresponding @ref ObjectType definition,
 *         or `NULL` if the ID is invalid or not registered.
 */
const ObjectType* get_object_type(ObjectTypeID id);

/**
 * @brief Analyzes a building to determine its most likely room type.
 *
 * This function evaluates the content of a given building (furniture,
 * area, and structure) and compares it against known
 * @ref RoomTypeRule definitions to classify it
 * (e.g., bedroom, workshop, storage room, etc.).
 *
 * @param[in] b Pointer to the building to analyze.
 * @return Pointer to the detected @ref RoomTypeRule, or `NULL` if no match is found.
 */
const RoomTypeRule* analyze_building_type(const Building* b);

/**
 * @brief Creates a new object instance and places it on the map.
 *
 * This function allocates and initializes an @ref Object
 * of the specified type at the given tile coordinates.
 *
 * @param[in] id Type identifier of the object to create.
 * @param[in] x  X coordinate in tile units.
 * @param[in] y  Y coordinate in tile units.
 * @return Pointer to the created @ref Object instance, or `NULL` on failure.
 *
 * @note The created object must later be managed (stored or freed)
 *       by the map or object system.
 */
Object* create_object(ObjectTypeID id, int x, int y);

/**
 * @brief Draws all active objects on the map using the given camera view.
 *
 * This function iterates through all object instances stored
 * in the map grid and renders them according to their type’s
 * texture or color.
 *
 * @param[in] map Pointer to the map structure containing placed objects.
 * @param[in] camera Pointer to the active camera used for world-to-screen projection.
 */
void draw_objects(Map* map, Camera2D* camera);

/**
 * @brief Determines whether the given object is considered a wall structure.
 *
 * @param[in] o Pointer to the object to check.
 * @return `true` if the object represents a wall-type structure, `false` otherwise.
 */
bool is_wall_object(const Object* o);

/**
 * @brief Determines whether the given object is a door element.
 *
 * @param[in] o Pointer to the object to check.
 * @return `true` if the object is a door, `false` otherwise.
 */
bool is_door_object(const Object* o);

/**
 * @brief Determines whether the given object blocks movement or visibility.
 *
 * This includes walls, closed doors, or any non-walkable
 * furniture items that obstruct player or NPC movement.
 *
 * @param[in] o Pointer to the object to check.
 * @return `true` if the object is blocking, `false` otherwise.
 */
bool is_blocking_object(const Object* o);

#endif /* OBJECT_H */
