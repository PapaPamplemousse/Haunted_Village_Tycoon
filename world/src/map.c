#include "map.h"
#include "tile.h"
#include <stdio.h>
#include <stdlib.h>
TileTypeID          tiles[MAP_HEIGHT][MAP_WIDTH];
static TileTypeID   selectedTile   = TILE_GRASS;
static ObjectTypeID selectedObject = OBJ_NONE;

Map G_MAP = {0}; // Tous les membres sont mis à zéro.

// --- Utilitaires internes ---
static inline int wrap_x(int x)
{
    return (x % MAP_WIDTH + MAP_WIDTH) % MAP_WIDTH;
}
static inline int wrap_y(int y)
{
    return (y % MAP_HEIGHT + MAP_HEIGHT) % MAP_HEIGHT;
}

void map_init(void)
{
    G_MAP.width  = MAP_WIDTH;
    G_MAP.height = MAP_HEIGHT;
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            G_MAP.tiles[y][x]   = TILE_GRASS;
            G_MAP.objects[y][x] = NULL;
        }
}

void map_unload(void)
{
}

// Conversion écran → monde
static Vector2 screen_to_world(Vector2 screen, Camera2D* camera)
{
    return GetScreenToWorld2D(screen, *camera);
}

// --- Lecture / écriture avec wrap ---
static TileTypeID get_tile_at(int x, int y)
{
    if (x < 0)
        x = (x % G_MAP.width + G_MAP.width) % G_MAP.width;
    if (y < 0)
        y = (y % G_MAP.height + G_MAP.height) % G_MAP.height;
    if (x >= G_MAP.width)
        x = x % G_MAP.width;
    if (y >= G_MAP.height)
        y = y % G_MAP.height;
    return G_MAP.tiles[y][x];
}

static void set_tile_at(int x, int y, TileTypeID id)
{
    if (x < 0)
        x = (x % G_MAP.width + G_MAP.width) % G_MAP.width;
    if (y < 0)
        y = (y % G_MAP.height + G_MAP.height) % G_MAP.height;
    if (x >= G_MAP.width)
        x = x % G_MAP.width;
    if (y >= G_MAP.height)
        y = y % G_MAP.height;

    G_MAP.tiles[y][x] = id;
}

// --- Mise à jour de la carte ---
bool update_map(Camera2D* camera)
{
    bool isChanged = false;
    if (IsKeyPressed(KEY_ONE))
    {
        selectedTile   = TILE_GRASS;
        selectedObject = OBJ_NONE;
    }
    if (IsKeyPressed(KEY_TWO))
    {
        selectedTile   = TILE_WATER;
        selectedObject = OBJ_NONE;
    }
    if (IsKeyPressed(KEY_THREE))
    {
        selectedTile   = TILE_LAVA;
        selectedObject = OBJ_NONE;
    }
    if (IsKeyPressed(KEY_FOUR))
    {
        selectedObject = OBJ_WALL_STONE;
        printf("Selected object: WALL\n");
    }
    if (IsKeyPressed(KEY_FIVE))
    {
        selectedObject = OBJ_DOOR_WOOD;
        printf("Selected object: DOOR\n");
    }
    if (IsKeyPressed(KEY_SIX))
    {
        selectedObject = OBJ_BED_SMALL;
        printf("Selected object: BED\n");
    }

    Vector2 mouse = GetMousePosition();
    Vector2 world = screen_to_world(mouse, camera);

    int cellX = (int)(world.x / TILE_SIZE);
    int cellY = (int)(world.y / TILE_SIZE);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        isChanged = true;
        if (selectedObject != OBJ_NONE)
        {
            // Place un objet au-dessus de la tuile
            if (G_MAP.objects[cellY][cellX])
                free(G_MAP.objects[cellY][cellX]);
            G_MAP.objects[cellY][cellX] = create_object(selectedObject, cellX, cellY);
        }
        else
        {
            // Place une tuile de sol
            set_tile_at(cellX, cellY, selectedTile);
        }
    }

    else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        isChanged = true;
        set_tile_at(cellX, cellY, TILE_GRASS);
        if (G_MAP.objects[cellY][cellX])
        {
            free(G_MAP.objects[cellY][cellX]);
            G_MAP.objects[cellY][cellX] = NULL;
        }
    }

    return isChanged;
}

void draw_map(Camera2D* camera)
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
            int        wx   = wrap_x(x);
            int        wy   = wrap_y(y);
            TileTypeID id   = G_MAP.tiles[wy][wx];
            TileType*  type = get_tile_type(id);

            Rectangle rect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};

            if (type->texture.id != 0)
                DrawTextureEx(type->texture, (Vector2){rect.x, rect.y}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);
            else
                DrawRectangleRec(rect, type->color);
        }
    }

    Vector2   mouse     = GetMousePosition();
    Vector2   world     = screen_to_world(mouse, camera);
    int       hoverX    = (int)(world.x / TILE_SIZE);
    int       hoverY    = (int)(world.y / TILE_SIZE);
    Rectangle highlight = {(float)hoverX * TILE_SIZE, (float)hoverY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
    DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
}
