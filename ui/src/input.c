/**
 * @file input.c
 * @brief Implements input collection and conversion helpers.
 */

#include "input.h"
#include "tile.h"
#include "object.h"
#include <raylib.h>
#include <stdio.h>
#include "ui.h"
#include "localization.h"

#define MAX_TILES (TILE_MAX)
#define MAX_OBJECTS (OBJ_COUNT)

static KeyboardKey* binding_field(KeyBindings* bindings, InputAction action)
{
    if (!bindings)
        return NULL;
    switch (action)
    {
        case INPUT_ACTION_MOVE_UP:
            return &bindings->moveUp;
        case INPUT_ACTION_MOVE_DOWN:
            return &bindings->moveDown;
        case INPUT_ACTION_MOVE_LEFT:
            return &bindings->moveLeft;
        case INPUT_ACTION_MOVE_RIGHT:
            return &bindings->moveRight;
        case INPUT_ACTION_TOGGLE_INVENTORY:
            return &bindings->toggleInventory;
        case INPUT_ACTION_TOGGLE_BUILDING_NAMES:
            return &bindings->toggleBuildingNames;
        case INPUT_ACTION_TOGGLE_PAUSE:
            return &bindings->togglePause;
        default:
            return NULL;
    }
}

void input_bindings_reset_default(KeyBindings* bindings)
{
    if (!bindings)
        return;

    bindings->moveUp             = KEY_W;
    bindings->moveDown           = KEY_S;
    bindings->moveLeft           = KEY_A;
    bindings->moveRight          = KEY_D;
    bindings->toggleInventory    = KEY_I;
    bindings->toggleBuildingNames = KEY_TAB;
    bindings->togglePause        = KEY_ESCAPE;
}

const char* input_action_display_name(InputAction action)
{
    switch (action)
    {
        case INPUT_ACTION_MOVE_UP:
            return localization_get("input.action.move_up");
        case INPUT_ACTION_MOVE_DOWN:
            return localization_get("input.action.move_down");
        case INPUT_ACTION_MOVE_LEFT:
            return localization_get("input.action.move_left");
        case INPUT_ACTION_MOVE_RIGHT:
            return localization_get("input.action.move_right");
        case INPUT_ACTION_TOGGLE_INVENTORY:
            return localization_get("input.action.toggle_inventory");
        case INPUT_ACTION_TOGGLE_BUILDING_NAMES:
            return localization_get("input.action.toggle_building_names");
        case INPUT_ACTION_TOGGLE_PAUSE:
            return localization_get("input.action.toggle_pause");
        default:
            return localization_get("input.action.unknown");
    }
}

KeyboardKey input_get_binding(const KeyBindings* bindings, InputAction action)
{
    if (!bindings)
        return KEY_NULL;

    switch (action)
    {
        case INPUT_ACTION_MOVE_UP:
            return bindings->moveUp;
        case INPUT_ACTION_MOVE_DOWN:
            return bindings->moveDown;
        case INPUT_ACTION_MOVE_LEFT:
            return bindings->moveLeft;
        case INPUT_ACTION_MOVE_RIGHT:
            return bindings->moveRight;
        case INPUT_ACTION_TOGGLE_INVENTORY:
            return bindings->toggleInventory;
        case INPUT_ACTION_TOGGLE_BUILDING_NAMES:
            return bindings->toggleBuildingNames;
        case INPUT_ACTION_TOGGLE_PAUSE:
            return bindings->togglePause;
        default:
            return KEY_NULL;
    }
}

void input_set_binding(KeyBindings* bindings, InputAction action, KeyboardKey key)
{
    KeyboardKey* slot = binding_field(bindings, action);
    if (slot)
        *slot = key;
}

bool input_is_key_already_bound(const KeyBindings* bindings, KeyboardKey key, InputAction* outAction)
{
    if (!bindings || key == KEY_NULL)
        return false;

    for (int action = 0; action < INPUT_ACTION_COUNT; ++action)
    {
        if (input_get_binding(bindings, (InputAction)action) == key)
        {
            if (outAction)
                *outAction = (InputAction)action;
            return true;
        }
    }
    return false;
}

void input_init(InputState* input)
{
    // Default to the first tile so the player can immediately paint terrain.
    input->selectedTile      = TILE_MAX;
    input->selectedObject    = OBJ_NONE;
    input->selectedEntity    = ENTITY_TYPE_INVALID;
    input->showBuildingNames = false;
    input->camera.moveDir    = (Vector2){0};
    input->camera.zoomDelta  = 0.0f;
    input->currentMode       = MODE_TILE;
    input->tileIndex         = 0;
    input->objectIndex       = 0;
    input_bindings_reset_default(&input->bindings);
}

void input_update(InputState* input)
{
    // Reset per-frame input accumulation.
    input->camera.moveDir   = (Vector2){0};
    input->camera.zoomDelta = 0.0f;

    // --- Camera movement ---
    if (!ui_is_input_blocked())
    {
        KeyboardKey left  = input_get_binding(&input->bindings, INPUT_ACTION_MOVE_LEFT);
        KeyboardKey right = input_get_binding(&input->bindings, INPUT_ACTION_MOVE_RIGHT);
        KeyboardKey up    = input_get_binding(&input->bindings, INPUT_ACTION_MOVE_UP);
        KeyboardKey down  = input_get_binding(&input->bindings, INPUT_ACTION_MOVE_DOWN);

        if ((left != KEY_NULL && IsKeyDown(left)) || IsKeyDown(KEY_LEFT))
            input->camera.moveDir.x -= 1.0f;
        if ((right != KEY_NULL && IsKeyDown(right)) || IsKeyDown(KEY_RIGHT))
            input->camera.moveDir.x += 1.0f;
        if ((up != KEY_NULL && IsKeyDown(up)) || IsKeyDown(KEY_UP))
            input->camera.moveDir.y -= 1.0f;
        if ((down != KEY_NULL && IsKeyDown(down)) || IsKeyDown(KEY_DOWN))
            input->camera.moveDir.y += 1.0f;
    }

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
