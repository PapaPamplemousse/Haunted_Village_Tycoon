#include "input.h"
#include "tile.h"
#include "object.h"
#include <raylib.h>
#include <stdio.h>

#define MAX_TILES (TILE_MAX)
#define MAX_OBJECTS (OBJ_COUNT)

void input_init(InputState* input)
{
    input->selectedTile      = TILE_GRASS;
    input->selectedObject    = OBJ_NONE;
    input->showBuildingNames = false;
    input->camera.moveDir    = (Vector2){0};
    input->camera.zoomDelta  = 0.0f;
}

void input_update(InputState* input)
{
    input->camera.moveDir   = (Vector2){0};
    input->camera.zoomDelta = 0.0f;

    // --- Map / object selection ---
    if (IsKeyPressed(KEY_ONE))
    {
        input->selectedTile   = TILE_GRASS;
        input->selectedObject = OBJ_NONE;
    }
    if (IsKeyPressed(KEY_TWO))
    {
        input->selectedTile   = TILE_WATER;
        input->selectedObject = OBJ_NONE;
    }
    if (IsKeyPressed(KEY_THREE))
    {
        input->selectedObject = OBJ_STDBUSH;
        printf("Selected object: Standard bush\n");
    }

    if (IsKeyPressed(KEY_FOUR))
    {
        input->selectedObject = OBJ_WALL_STONE;
        printf("Selected object: WALL\n");
    }
    if (IsKeyPressed(KEY_FIVE))
    {
        input->selectedObject = OBJ_DOOR_WOOD;
        printf("Selected object: DOOR\n");
    }
    if (IsKeyPressed(KEY_SIX))
    {
        input->selectedObject = OBJ_BED_SMALL;
        printf("Selected object: BED\n");
    }

    // --- Building name toggle ---
    if (IsKeyPressed(KEY_TAB))
    {
        input->showBuildingNames = !input->showBuildingNames;
        printf("Show building names: %s\n", input->showBuildingNames ? "ON" : "OFF");
    }

    // --- Camera movement ---
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        input->camera.moveDir.x -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        input->camera.moveDir.x += 1.0f;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        input->camera.moveDir.y -= 1.0f;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        input->camera.moveDir.y += 1.0f;

    // --- Camera zoom (mouse wheel) ---
    input->camera.zoomDelta = GetMouseWheelMove();
}

/**
 * @brief Updates mouse state (screen → world → tile coordinates).
 */
void input_update_mouse(MouseState* mouse, const Camera2D* camera, const Map* map)
{
    if (!mouse || !camera || !map)
        return;

    mouse->screen = GetMousePosition();
    mouse->world  = GetScreenToWorld2D(mouse->screen, *camera);

    mouse->tileX = (int)(mouse->world.x / TILE_SIZE);
    mouse->tileY = (int)(mouse->world.y / TILE_SIZE);

    mouse->insideMap = mouse->tileX >= 0 && mouse->tileY >= 0 && mouse->tileX < map->width && mouse->tileY < map->height;
}
