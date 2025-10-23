#include "tile.h"
#include <stddef.h>

// Nécessite : #include "tile_types.h"
// Assurez-vous que TILE_COUNT est bien le dernier élément de TileTypeID (valant 12)

TileType tileTypes[TILE_COUNT] = {
    [TILE_GRASS] = {.name         = "Grass",
                    .id           = TILE_GRASS,
                    .category     = TILE_CATEGORY_GROUND,
                    .walkable     = true,
                    .color        = (Color){34, 139, 34, 255},
                    .texture      = {0},
                    .isBreakable  = false,
                    .durability   = 100,
                    .movementCost = 1.0f,
                    .humidity     = 0.7f,
                    .fertility    = 0.8f,
                    .temperature  = 18},

    [TILE_WATER] = {.name         = "Water",
                    .id           = TILE_WATER,
                    .category     = TILE_CATEGORY_WATER,
                    .walkable     = false,
                    .color        = (Color){70, 130, 180, 255},
                    .texture      = {0},
                    .isBreakable  = false,
                    .durability   = 100,
                    .movementCost = 100.0f,
                    .humidity     = 1.0f,
                    .fertility    = 0.0f,
                    .temperature  = 12},

    [TILE_LAVA] = {.name         = "Lava",
                   .id           = TILE_LAVA,
                   .category     = TILE_CATEGORY_HAZARD,
                   .walkable     = false,
                   .color        = (Color){255, 69, 0, 255},
                   .texture      = {0},
                   .isBreakable  = false,
                   .durability   = 100,
                   .movementCost = 100.0f,
                   .humidity     = 0.0f,
                   .fertility    = 0.0f,
                   .temperature  = 1000},

    [TILE_FOREST] = {.name         = "Forest",
                     .id           = TILE_FOREST,
                     .category     = TILE_CATEGORY_GROUND,
                     .walkable     = true,
                     .color        = (Color){34, 100, 34, 255},
                     .texture      = {0},
                     .isBreakable  = false,
                     .durability   = 100,
                     .movementCost = 1.5f,
                     .humidity     = 0.8f,
                     .fertility    = 0.7f,
                     .temperature  = 15},

    [TILE_PLAIN] = {.name         = "Plain",
                    .id           = TILE_PLAIN,
                    .category     = TILE_CATEGORY_GROUND,
                    .walkable     = true,
                    .color        = (Color){124, 252, 0, 255},
                    .texture      = {0},
                    .isBreakable  = false,
                    .durability   = 100,
                    .movementCost = 1.0f,
                    .humidity     = 0.5f,
                    .fertility    = 0.6f,
                    .temperature  = 20},

    [TILE_SAVANNA] = {.name         = "Savanna",
                      .id           = TILE_SAVANNA,
                      .category     = TILE_CATEGORY_GROUND,
                      .walkable     = true,
                      .color        = (Color){189, 183, 107, 255},
                      .texture      = {0},
                      .isBreakable  = false,
                      .durability   = 100,
                      .movementCost = 1.2f,
                      .humidity     = 0.3f,
                      .fertility    = 0.3f,
                      .temperature  = 28},

    [TILE_TUNDRA] = {.name         = "Tundra",
                     .id           = TILE_TUNDRA,
                     .category     = TILE_CATEGORY_GROUND,
                     .walkable     = true,
                     .color        = (Color){176, 196, 222, 255}, // Bleu Givré
                     .texture      = {0},
                     .isBreakable  = false,
                     .durability   = 100,
                     .movementCost = 2.0f,
                     .humidity     = 0.2f,
                     .fertility    = 0.1f,
                     .temperature  = -10},

    [TILE_HELL] = {.name         = "Hell",
                   .id           = TILE_HELL,
                   .category     = TILE_CATEGORY_HAZARD,
                   .walkable     = false,
                   .color        = (Color){139, 0, 0, 255},
                   .texture      = {0},
                   .isBreakable  = false,
                   .durability   = 100,
                   .movementCost = 3.0f,
                   .humidity     = 0.0f,
                   .fertility    = 0.0f,
                   .temperature  = 50},

    [TILE_CURSED_FOREST] = {.name         = "Cursed Forest",
                            .id           = TILE_CURSED_FOREST,
                            .category     = TILE_CATEGORY_GROUND,
                            .walkable     = true,
                            .color        = (Color){85, 107, 47, 255},
                            .texture      = {0},
                            .isBreakable  = false,
                            .durability   = 100,
                            .movementCost = 2.5f,
                            .humidity     = 0.7f,
                            .fertility    = 0.5f,
                            .temperature  = 8},

    // TILE_SWAMP (ID 9) - NOUVEAU
    [TILE_SWAMP] = {.name         = "Swamp",
                    .id           = TILE_SWAMP,
                    .category     = TILE_CATEGORY_GROUND,
                    .walkable     = true,
                    .color        = (Color){102, 102, 51, 255},
                    .texture      = {0},
                    .isBreakable  = false,
                    .durability   = 100,
                    .movementCost = 3.0f,
                    .humidity     = 0.9f,
                    .fertility    = 0.8f,
                    .temperature  = 10},

    [TILE_DESERT] = {.name         = "Desert",
                     .id           = TILE_DESERT,
                     .category     = TILE_CATEGORY_GROUND,
                     .walkable     = true,
                     .color        = (Color){244, 164, 96, 255},
                     .texture      = {0},
                     .isBreakable  = false,
                     .durability   = 100,
                     .movementCost = 1.3f,
                     .humidity     = 0.1f,
                     .fertility    = 0.05f,
                     .temperature  = 40},

    [TILE_MOUNTAIN] = {.name         = "Mountain",
                       .id           = TILE_MOUNTAIN,
                       .category     = TILE_CATEGORY_OBSTACLE,
                       .walkable     = false,
                       .color        = (Color){105, 105, 105, 255},
                       .texture      = {0},
                       .isBreakable  = true,
                       .durability   = 500,
                       .movementCost = 100.0f,
                       .humidity     = 0.2f,
                       .fertility    = 0.05f,
                       .temperature  = 0},

};

void init_tile_types(void)
{
    // Example: Loading textures (if Texture2D is a handle)
    // tileTypes[TILE_GRASS].texture = LoadTexture("textures/grass.png");
    // tileTypes[TILE_WATER].texture = LoadTexture("textures/water.png");
}

void unload_tile_types(void)
{
}

TileType* get_tile_type(TileTypeID id)
{
    if (id >= 0 && id < TILE_COUNT)
    {
        return &tileTypes[id];
    }
    return NULL;
}
