/**
 * @file object_loader.h
 * @brief Functions for loading object and room definitions from .stv files.
 */
#ifndef OBJECT_LOADER_H
#define OBJECT_LOADER_H

#include "world.h"
#include "object.h"
#include <stdbool.h>

/**
 * @brief Loads object definitions from an .stv file.
 * @param path Path to the objects.stv file.
 * @param outArray Output array for the loaded object types.
 * @param maxObjects Maximum size of the output array.
 * @return The number of objects successfully loaded.
 */
int load_objects_from_stv(const char* path, ObjectType* outArray, int maxObjects);

/**
 * @brief Loads room type definitions/rules from an .stv file.
 * @param path Path to the structures.stv file containing room sections.
 * @param outArray Output array for the loaded room type rules.
 * @param maxRooms Maximum size of the output array.
 * @param objects Array of available object types (required for room definitions).
 * @param objectCount Number of object types in the 'objects' array.
 * @return The number of room type rules successfully loaded.
 */
int load_rooms_from_stv(const char* path, RoomTypeRule* outArray, int maxRooms, const ObjectType* objects, int objectCount);

/**
 * @brief Helper function to find an object type by its name (used during room loading).
 * @param objects Array of object types to search within.
 * @param count Number of object types in the array.
 * @param name The name of the object to find.
 * @return A pointer to the found ObjectType, or NULL if not found.
 */
const ObjectType* find_object_by_name(const ObjectType* objects, int count, const char* name);

/**
 * @brief Prints the loaded room type rules to standard output for debugging purposes.
 * @param rooms Array of room type rules.
 * @param roomCount Number of room type rules in the array.
 * @param objects Array of available object types.
 * @param objectCount Number of object types in the 'objects' array.
 */
void debug_print_rooms(const RoomTypeRule* rooms, int roomCount, const ObjectType* objects, int objectCount);

/**
 * @brief Prints the loaded object types to standard output for debugging purposes.
 * @param objects Array of object types.
 * @param count Number of object types in the array.
 */
void debug_print_objects(const ObjectType* objects, int count);

#endif // OBJECT_LOADER_H
