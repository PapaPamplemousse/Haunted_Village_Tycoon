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
 * @ref StructureDef classification rules to determine the best match
 * (e.g., bedroom, workshop, storage room, etc.).
 *
 * @param[in] b Pointer to the building to analyze.
 * @return Pointer to the detected @ref StructureDef, or `NULL` if no match is found.
 */
const StructureDef* analyze_building_type(const Building* b);

/**
 * @brief Retrieves the object type identifier for the given internal name.
 *
 * @param[in] name Internal object name as defined in objects.stv.
 * @return Matching ObjectTypeID, or OBJ_NONE if no match exists.
 */
ObjectTypeID object_type_id_from_name(const char* name);

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
 * @brief Releases memory associated with an object instance.
 *
 * @param[in,out] obj Pointer to the object to destroy.
 */
void object_destroy(Object* obj);

/**
 * @brief Returns whether the object supports activation toggling.
 */
bool object_has_activation(const Object* obj);

/**
 * @brief Sets the activation state of an object, triggering animations if defined.
 *
 * @param[in,out] obj Pointer to the object instance.
 * @param[in] active Desired activation state.
 * @return true if the state changed, false otherwise.
 */
bool object_set_active(Object* obj, bool active);

/**
 * @brief Toggles the activation state of an object.
 *
 * @param[in,out] obj Pointer to the object instance.
 * @return true if the state changed, false otherwise.
 */
bool object_toggle(Object* obj);

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
 * @brief Draws dynamic (activatable/animated) objects each frame.
 */
void object_draw_dynamic(const Map* map, const Camera2D* camera);

/**
 * @brief Advances activation animations for dynamic objects.
 *
 * @param[in] dt Delta time in seconds.
 */
void object_update_system(float dt);

/**
 * @brief Computes the source rectangle for the given frame of an object type.
 *
 * @param[in] type Object type definition.
 * @param[in] frameIndex 0-based frame index.
 * @return Source rectangle in texture coordinates.
 */
Rectangle object_type_frame_rect(const ObjectType* type, int frameIndex);

/**
 * @brief Computes the screen-space draw position for an object's frame.
 *
 * The position is aligned so that the sprite rests on the object's footprint.
 *
 * @param[in] obj Object instance.
 * @param[in] frameWidth Width of the frame in pixels.
 * @param[in] frameHeight Height of the frame in pixels.
 * @return Screen position where the frame should be drawn.
 */
Vector2 object_frame_draw_position(const Object* obj, int frameWidth, int frameHeight);

/**
 * @brief Returns the currently visible frame index for a static object.
 *
 * @param[in] obj Object instance.
 * @return Frame index clamped to the available frames.
 */
int object_static_frame(const Object* obj);

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

/**
 * @brief Computes whether an object currently allows entities to walk through.
 *
 * @param[in] o Pointer to the object to evaluate.
 * @return true if the object is non-blocking for entities.
 */
bool object_is_walkable(const Object* o);

#endif /* OBJECT_H */
