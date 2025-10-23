// world_generation.c

#include "world_generation.h"
#include <stdlib.h>

void generate_world(Map* map, unsigned int seed)
{
    if (!map)
        return;

    // Seed fixe pour reproductibilité ; remplacer 12345 pour un monde différent
    srand(seed);

    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            // Génère deux valeurs (élévation et humidité) normalisées [0..1]
            float e = (float)rand() / RAND_MAX;
            float m = (float)rand() / RAND_MAX;

            // Définition du type de tuile selon e et m

            map->tiles[y][x]   = TILE_FOREST;
            map->objects[y][x] = NULL;
        }
    }
}
