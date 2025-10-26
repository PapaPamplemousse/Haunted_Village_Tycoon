#include "tile.h"
#include <stddef.h>
#include "tiles_loader.h"
#include <stdio.h>

TileType tileTypes[TILE_MAX] = {0};

void init_tile_types(void)
{
    (void)load_tiles_from_stv("data/tiles.stv", tileTypes, TILE_MAX);
    for (int i = 0; i < TILE_MAX; ++i)
    {
        if (tileTypes[i].texturePath != NULL)
        {
            printf("Loading tile %d: %s (%s)\n", i, tileTypes[i].name ? tileTypes[i].name : "(null)", tileTypes[i].texturePath ? tileTypes[i].texturePath : "(null)");
            fflush(stdout);
            tileTypes[i].texture = LoadTexture(tileTypes[i].texturePath);
        }
    }
}

void unload_tile_types(void)
{
}

TileType* get_tile_type(TileTypeID id)
{
    if (id >= 0 && id < TILE_MAX)
    {
        return &tileTypes[id];
    }
    return NULL;
}
