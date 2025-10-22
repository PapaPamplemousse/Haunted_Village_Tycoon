#ifndef MAP_H
#define MAP_H

#include "raylib.h"
#include "tile.h"

#define MAP_WIDTH 100
#define MAP_HEIGHT 100
#define TILE_SIZE 32

// Définition du type de tuile (par ex. pour futur pathfinding)
typedef struct
{
    bool      walkable;
    Texture2D texture;
} Tile;

// Grille principale visible par d'autres modules
extern int grid[MAP_HEIGHT][MAP_WIDTH];

// Fonctions exportées
void map_init(void);
void map_unload(void);
void draw_map(Camera2D* camera);
void update_map(Camera2D* camera);

// Fonction appelée par le module building
void occupy_tiles(Vector2 origin, int id);

#endif
