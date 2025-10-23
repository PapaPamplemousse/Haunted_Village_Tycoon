#ifndef MAP_H
#define MAP_H

#include "raylib.h"
#include "tile.h"
#include "object.h"

#define MAP_WIDTH 100
#define MAP_HEIGHT 100
#define TILE_SIZE 32

typedef struct
{
    int        width;
    int        height;
    TileTypeID tiles[MAP_HEIGHT][MAP_WIDTH];
    Object*    objects[MAP_HEIGHT][MAP_WIDTH];
} Map;

extern Map G_MAP;
// Exported functions
void map_init(void);
void map_unload(void);
void draw_map(Camera2D* camera);
bool update_map(Camera2D* camera);

#endif
