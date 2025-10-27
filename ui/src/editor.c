/**
 * @file editor.c
 * @brief Connects input events with map editing actions.
 */

#include "editor.h"
#include "ui.h"
#include <raylib.h>

bool editor_update(Map* map, Camera2D* camera, InputState* input, EntitySystem* entities)
{
    if (!ui_is_inventory_open())
    {
        // Convert the mouse cursor to a tile coordinate inside the grid.
        Vector2 mouse = GetMousePosition();
        Vector2 world = GetScreenToWorld2D(mouse, *camera);
        int     cellX = (int)(world.x / TILE_SIZE);
        int     cellY = (int)(world.y / TILE_SIZE);

        if (cellX < 0 || cellY < 0 || cellX >= MAP_WIDTH || cellY >= MAP_HEIGHT)
            return false;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (input->selectedEntity != ENTITY_TYPE_INVALID && entities)
            {
                const EntityType* type = entity_find_type(entities, input->selectedEntity);
                if (type)
                {
                    Vector2 spawnPos = {(cellX + 0.5f) * TILE_SIZE, (cellY + 0.5f) * TILE_SIZE};
                    if (entity_position_is_walkable(map, spawnPos, type->radius))
                    {
                        uint16_t id = entity_spawn(entities, input->selectedEntity, spawnPos);
                        if (id != ENTITY_ID_INVALID)
                        {
                            Entity* ent = entity_acquire(entities, id);
                            if (ent)
                            {
                                ent->home = spawnPos;
                                if (type->referredStructure != STRUCT_COUNT)
                                    ent->homeStructure = type->referredStructure;
                            }
                        }
                    }
                }
                return false;
            }
            if (input->selectedObject != OBJ_NONE)
                map_place_object(map, input->selectedObject, cellX, cellY);
            else if (input->selectedTile != TILE_MAX)
                map_set_tile(map, cellX, cellY, input->selectedTile);
            return true;
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
        {
            // Right click clears any object occupying the cell.
            map_remove_object(map, cellX, cellY);
            return true;
        }
    }

    return false;
}
