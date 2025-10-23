#include "map.h"
#include "tile.h"
#include "object.h"
#include <stdlib.h>
#include "world_generation.h"

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

    generate_world(map, seed);
    // for (int y = 0; y < MAP_HEIGHT; ++y)
    // {
    //     for (int x = 0; x < MAP_WIDTH; ++x)
    //     {
    //         map->tiles[y][x]   = TILE_GRASS;
    //         map->objects[y][x] = NULL;
    //     }
    // }
}

void map_unload(Map* map)
{
}

TileTypeID map_get_tile(Map* map, int x, int y)
{
    return map->tiles[wrap_y(y)][wrap_x(x)];
}

void map_set_tile(Map* map, int x, int y, TileTypeID id)
{
    map->tiles[wrap_y(y)][wrap_x(x)] = id;
}

void map_place_object(Map* map, ObjectTypeID id, int x, int y)
{
    int wx = wrap_x(x);
    int wy = wrap_y(y);

    if (map->objects[wy][wx])
        free(map->objects[wy][wx]);
    map->objects[wy][wx] = create_object(id, x, y);
}

void map_remove_object(Map* map, int x, int y)
{
    int wx = wrap_x(x);
    int wy = wrap_y(y);

    if (map->objects[wy][wx])
    {
        free(map->objects[wy][wx]);
        map->objects[wy][wx] = NULL;
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

    Vector2   mouse     = GetMousePosition();
    Vector2   world     = GetScreenToWorld2D(mouse, *camera);
    int       hoverX    = (int)(world.x / TILE_SIZE);
    int       hoverY    = (int)(world.y / TILE_SIZE);
    Rectangle highlight = {(float)hoverX * TILE_SIZE, (float)hoverY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
    DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
}
