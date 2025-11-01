/**
 * @file object_loader.h
 * @brief Functions for loading object definitions from .stv files.
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
 * @brief Prints the loaded object types to standard output for debugging purposes.
 * @param objects Array of object types.
 * @param count Number of object types in the array.
 */
void debug_print_objects(const ObjectType* objects, int count);

#endif // OBJECT_LOADER_H
