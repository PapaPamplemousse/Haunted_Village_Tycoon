#include "tile.h"
#include <stddef.h>

static TileType tileTypes[TILE_COUNT];

void init_tile_types(void)
{
    // TILE_GRASS
    tileTypes[TILE_GRASS] = (TileType){.name        = "Grass",
                                       .id          = TILE_GRASS,           // ID basée sur l'enum
                                       .category    = TILE_CATEGORY_GROUND, // Catégorie définie
                                       .walkable    = true,
                                       .color       = (Color){34, 139, 34, 255}, // Vert forêt
                                       .isBreakable = false,
                                       .durability  = 100,
                                       .texture     = (Texture2D){0}};

    // // TILE_STONE_WALL
    // tileTypes[TILE_STONE_WALL] = (TileType){.name        = "Stone Wall",
    //                                         .id          = TILE_STONE_WALL,
    //                                         .category    = TILE_CATEGORY_WALL, // Catégorie définie
    //                                         .walkable    = false,
    //                                         .color       = (Color){105, 105, 105, 255}, // Gris ardoise
    //                                         .isBreakable = true,
    //                                         .durability  = 100,
    //                                         .texture     = (Texture2D){0}};

    // TILE_WATER
    tileTypes[TILE_WATER] = (TileType){.name        = "Water",
                                       .id          = TILE_WATER,
                                       .category    = TILE_CATEGORY_WATER, // Catégorie définie
                                       .walkable    = false,
                                       .isBreakable = false,
                                       .durability  = 100,
                                       .color       = (Color){70, 130, 180, 255}, // Bleu acier
                                       .texture     = (Texture2D){0}};

    // TILE_WOOD_DOOR
    // tileTypes[TILE_WOOD_DOOR] = (TileType){.name        = "Wood door",
    //                                        .id          = TILE_WOOD_DOOR,     // ID basée sur l'enum
    //                                        .category    = TILE_CATEGORY_DOOR, // Catégorie définie
    //                                        .walkable    = false,
    //                                        .isBreakable = false,
    //                                        .durability  = 50,
    //                                        .color       = (Color){165, 42, 42, 255},
    //                                        .texture     = (Texture2D){0}};

    tileTypes[TILE_LAVA] = (TileType){.name        = "Lava",
                                      .id          = TILE_LAVA,
                                      .category    = TILE_CATEGORY_GROUND,     // Souvent traitée comme un sol dangereux
                                      .walkable    = false,                    // Généralement non-marchable (ou marchable avec dégâts)
                                      .color       = (Color){255, 69, 0, 255}, // Rouge-orange vif
                                      .isBreakable = false,
                                      .durability  = 100,
                                      .texture     = (Texture2D){0}};
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
