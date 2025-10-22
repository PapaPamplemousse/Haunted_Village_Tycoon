#include "tile.h"
#include <stddef.h>

static TileType tileTypes[TILE_COUNT];

void init_tile_types(void)
{
    tileTypes[TILE_GRASS] = (TileType){
        .name = "Grass", .color = (Color){34, 139, 34, 255}, .passable = true, .texture = (Texture2D){0} // pas de texture pour l'instant
    };
    tileTypes[TILE_WALL]  = (TileType){.name = "Wall", .color = (Color){105, 105, 105, 255}, .passable = false, .texture = (Texture2D){0}};
    tileTypes[TILE_WATER] = (TileType){.name = "Water", .color = (Color){70, 130, 180, 255}, .passable = false, .texture = (Texture2D){0}};
}

void unload_tile_types(void)
{
    // Si plus tard tu charges des textures, pense à UnloadTexture() ici.
}

TileType* get_tile_type(TileTypeID id)
{
    if (id >= 0 && id < TILE_COUNT)
    {
        return &tileTypes[id];
    }
    return NULL;
}
