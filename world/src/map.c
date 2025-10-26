#include "map.h"
#include "tile.h"
#include "object.h"
#include <stdlib.h>
#include "world_generation.h"
#include "world_chunk.h"
#include "input.h"

static inline int wrap_x(int x)
{
    return (x % MAP_WIDTH + MAP_WIDTH) % MAP_WIDTH;
}
static inline int wrap_y(int y)
{
    return (y % MAP_HEIGHT + MAP_HEIGHT) % MAP_HEIGHT;
}

void map_init(Map* map, unsigned int seed)
{
    if (!map)
        return;
    map->width  = MAP_WIDTH;
    map->height = MAP_HEIGHT;

    worldgen_seed(seed);
    WorldGenParams cfg = {
        .min_biome_radius           = (MAP_WIDTH + MAP_HEIGHT) / 8,
        .weight_forest              = 1.0f,
        .weight_plain               = 1.0f,
        .weight_savanna             = 0.8f,
        .weight_tundra              = 0.6f,
        .weight_desert              = 0.7f,
        .weight_swamp               = 0.5f,
        .weight_mountain            = 0.45f,
        .weight_cursed              = 0.08f,
        .weight_hell                = 0.04f,
        .feature_density            = 0.08f,
        .structure_chance           = 0.0003f,
        .structure_min_spacing      = (MAP_WIDTH + MAP_HEIGHT) / 32,
        .biome_struct_mult_forest   = 0.4f,
        .biome_struct_mult_plain    = 1.0f,
        .biome_struct_mult_savanna  = 1.2f,
        .biome_struct_mult_tundra   = 0.5f,
        .biome_struct_mult_desert   = 0.3f,
        .biome_struct_mult_swamp    = 0.6f,
        .biome_struct_mult_mountain = 0.4f,
        .biome_struct_mult_cursed   = 0.8f,
        .biome_struct_mult_hell     = 0.1f,
    };
    worldgen_config(&cfg);

    generate_world(map);
}

void map_unload(Map* map)
{
    (void)map;
}

TileTypeID map_get_tile(Map* map, int x, int y)
{
    return map->tiles[wrap_y(y)][wrap_x(x)];
}

void map_set_tile(Map* map, int x, int y, TileTypeID id)
{
    map->tiles[wrap_y(y)][wrap_x(x)] = id;
    chunkgrid_mark_dirty_tile(gChunks, x, y);
}

void map_place_object(Map* map, ObjectTypeID id, int x, int y)
{
    int wx = wrap_x(x);
    int wy = wrap_y(y);

    if (map->objects[wy][wx])
        free(map->objects[wy][wx]);
    map->objects[wy][wx] = create_object(id, x, y);

    chunkgrid_mark_dirty_tile(gChunks, wx, wy);
}

void map_remove_object(Map* map, int x, int y)
{
    int wx = wrap_x(x);
    int wy = wrap_y(y);

    if (map->objects[wy][wx])
    {
        free(map->objects[wy][wx]);
        map->objects[wy][wx] = NULL;

        chunkgrid_mark_dirty_tile(gChunks, wx, wy);
    }
}

void draw_map(Map* map, Camera2D* camera)
{
    Rectangle view = {.x      = camera->target.x - (GetScreenWidth() / 2) / camera->zoom,
                      .y      = camera->target.y - (GetScreenHeight() / 2) / camera->zoom,
                      .width  = GetScreenWidth() / camera->zoom,
                      .height = GetScreenHeight() / camera->zoom};

    int startX = (int)(view.x / TILE_SIZE) - 1;
    int startY = (int)(view.y / TILE_SIZE) - 1;
    int endX   = (int)((view.x + view.width) / TILE_SIZE) + 1;
    int endY   = (int)((view.y + view.height) / TILE_SIZE) + 1;

    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            int       wx   = wrap_x(x);
            int       wy   = wrap_y(y);
            TileType* type = get_tile_type(map->tiles[wy][wx]);
            Rectangle rect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};

            if (type->texture.id != 0)
                DrawTextureEx(type->texture, (Vector2){rect.x, rect.y}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);
            else
                DrawRectangleRec(rect, type->color);
        }
    }
}
