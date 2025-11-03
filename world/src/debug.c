/**
 * @file debug.c
 * @brief Implements debug visualizations for biome distribution.
 */

#include "debug.h"
#include "raylib.h"
#include "tile.h"
#include "world_generation.h"
#include <stdio.h>

void debug_biome_draw(Map* map, Camera2D* cam, bool* showDebug)
{
    if (!(*showDebug))
        return;

    BeginMode2D(*cam);

    for (int y = 0; y < map->height; ++y)
    {
        for (int x = 0; x < map->width; ++x)
        {
            TileTypeID t = map->tiles[y][x];

            // Assign each biome / tile family a color
            Color c = WHITE;
            switch (t)
            {
                case TILE_FOREST:
                    c = (Color){34, 139, 34, 180};
                    break; // green
                case TILE_PLAIN:
                    c = (Color){100, 220, 100, 180};
                    break;
                case TILE_SAVANNA:
                    c = (Color){200, 180, 70, 180};
                    break;
                case TILE_TUNDRA:
                    c = (Color){180, 220, 255, 180};
                    break;
                case TILE_DESERT:
                    c = (Color){240, 230, 140, 180};
                    break;
                case TILE_SWAMP:
                    c = (Color){70, 120, 70, 180};
                    break;
                case TILE_MOUNTAIN:
                    c = (Color){180, 180, 180, 180};
                    break;
                case TILE_CURSED_FOREST:
                    c = (Color){90, 0, 90, 180};
                    break;
                case TILE_HELL:
                    c = (Color){220, 60, 60, 180};
                    break;
                case TILE_WATER:
                    c = (Color){30, 100, 255, 150};
                    break;
                case TILE_LAVA:
                    c = (Color){255, 80, 0, 150};
                    break;
                case TILE_POISON:
                    c = (Color){120, 0, 150, 150};
                    break;
                default:
                    c = (Color){255, 255, 255, 100};
                    break;
            }

            DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, c);
        }
    }

    EndMode2D();

    DrawText("Biome Debug Overlay", 20, 20, 20, YELLOW);
}
