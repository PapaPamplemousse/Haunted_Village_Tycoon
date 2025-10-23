#ifndef BUILDING_H
#define BUILDING_H

#include "map.h"
#include "raylib.h"
#include "object.h"

// Building information structure
typedef struct Building
{
    int                 id;
    Rectangle           bounds;      // bounding box of interior (in tile coordinates)
    Vector2             center;      // center of the building (in tile coordinates)
    int                 area;        // area (number of tiles inside)
    char                name[64];    // generic or inferred name of the building
    int                 objectCount; // number of objects inside
    Object**            objects;     // pointer to list of object instances (dynamic)
    const RoomTypeRule* roomType;    // detected room type (optional)
} Building;

// Maximum number of buildings we can track
#define MAX_BUILDINGS 100

// Array to store detected buildings and count
extern Building buildings[MAX_BUILDINGS];
extern int      buildingCount;

// Detect buildings enclosed by walls on the map and update the buildings list
void update_building_detection(void);

#endif
