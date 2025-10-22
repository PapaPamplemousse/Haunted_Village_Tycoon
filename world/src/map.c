#include "map.h"
#include "tile.h"

#include "map.h"

TileTypeID tiles[MAP_HEIGHT][MAP_WIDTH];

static TileTypeID selectedTile = TILE_GRASS;

void map_init(void)
{
    // Remplit toute la carte avec de l’herbe au départ
    for (int y = 0; y < MAP_HEIGHT; y++)
    {
        for (int x = 0; x < MAP_WIDTH; x++)
        {
            tiles[y][x] = TILE_GRASS;
        }
    }
}

void map_unload(void)
{
    // Rien pour l’instant
}

static Vector2 screen_to_world(Vector2 screen, Camera2D* camera)
{
    // convertit une coordonnée écran en coordonnée du monde (avant de divisier par TILE_SIZE)
    Vector2 world = GetScreenToWorld2D(screen, *camera);
    return world;
}

void update_map(Camera2D* camera)
{
    // Mise à jour de la sélection (touche 1 / 2 / 3)
    if (IsKeyPressed(KEY_ONE))
        selectedTile = TILE_GRASS;
    if (IsKeyPressed(KEY_TWO))
        selectedTile = TILE_WALL;
    if (IsKeyPressed(KEY_THREE))
        selectedTile = TILE_WATER;

    // Récupère la case survolée
    Vector2 mouse = GetMousePosition();
    Vector2 world = screen_to_world(mouse, camera);
    int     cellX = (int)(world.x / TILE_SIZE);
    int     cellY = (int)(world.y / TILE_SIZE);

    if (cellX >= 0 && cellX < MAP_WIDTH && cellY >= 0 && cellY < MAP_HEIGHT)
    {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            // Place la tuile sélectionnée
            tiles[cellY][cellX] = selectedTile;
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            // Remet à l’herbe
            tiles[cellY][cellX] = TILE_GRASS;
        }
    }
}

void draw_map(Camera2D* camera)
{
    // On calcule quelle portion de la carte est visible pour optimiser (optionnel)
    Rectangle view   = (Rectangle){.x      = camera->target.x - (GetScreenWidth() / 2) / camera->zoom,
                                   .y      = camera->target.y - (GetScreenHeight() / 2) / camera->zoom,
                                   .width  = (GetScreenWidth() / camera->zoom),
                                   .height = (GetScreenHeight() / camera->zoom)};
    int       startX = (int)(view.x / TILE_SIZE) - 1;
    int       startY = (int)(view.y / TILE_SIZE) - 1;
    int       endX   = (int)((view.x + view.width) / TILE_SIZE) + 1;
    int       endY   = (int)((view.y + view.height) / TILE_SIZE) + 1;
    if (startX < 0)
        startX = 0;
    if (startY < 0)
        startY = 0;
    if (endX >= MAP_WIDTH)
        endX = MAP_WIDTH - 1;
    if (endY >= MAP_HEIGHT)
        endY = MAP_HEIGHT - 1;

    // Dessine chaque case visible
    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            TileType* type = get_tile_type(tiles[y][x]);
            Rectangle rect = {x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            if (type->texture.id != 0)
            {
                DrawTextureEx(type->texture, (Vector2){rect.x, rect.y}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);
            }
            else
            {
                DrawRectangleRec(rect, type->color);
            }
        }
    }

    // Dessine la case survolée en surbrillance
    Vector2 mouse  = GetMousePosition();
    Vector2 world  = screen_to_world(mouse, camera);
    int     hoverX = (int)(world.x / TILE_SIZE);
    int     hoverY = (int)(world.y / TILE_SIZE);
    if (hoverX >= 0 && hoverX < MAP_WIDTH && hoverY >= 0 && hoverY < MAP_HEIGHT)
    {
        Rectangle highlight = {hoverX * TILE_SIZE, hoverY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
    }
}
