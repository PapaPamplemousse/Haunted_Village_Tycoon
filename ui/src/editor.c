#include "editor.h"
#include <raylib.h>

bool editor_update(Map* map, Camera2D* camera, InputState* input)
{
    bool changed = false;

    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, *camera);
    int     cellX = (int)(world.x / TILE_SIZE);
    int     cellY = (int)(world.y / TILE_SIZE);

    if (cellX < 0 || cellY < 0 || cellX >= MAP_WIDTH || cellY >= MAP_HEIGHT)
        return false;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        changed = true;
        if (input->selectedObject != OBJ_NONE)
            map_place_object(map, input->selectedObject, cellX, cellY);
        else
            map_set_tile(map, cellX, cellY, input->selectedTile);
    }
    else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        changed = true;
        map_set_tile(map, cellX, cellY, TILE_GRASS);
        map_remove_object(map, cellX, cellY);
    }

    return changed;
}
