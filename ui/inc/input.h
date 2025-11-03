/**
 * @file input.h
 * @brief Handles keyboard input and user selection state.
 *
 * This module centralizes all input logic, keeping the rest of the code
 * independent from direct Raylib input calls.
 *
 * It tracks which tile or object type is currently selected, and whether
 * optional UI toggles (like building name display) are active.
 */

#ifndef INPUT_H
#define INPUT_H

#include "world.h"
#include "entity.h"

/**
 * @brief Represents user camera control input (movement and zoom).
 */
typedef struct
{
    Vector2 moveDir;   /**< Normalized movement direction (-1..1 on each axis). */
    float   zoomDelta; /**< Zoom variation (positive = zoom in, negative = zoom out). */
} CameraInput;

typedef enum
{
    MODE_TILE,
    MODE_OBJECT,
    MODE_ENTITY
} SelectionMode;

/**
 * @brief Logical input actions that can be rebound from the settings menu.
 */
typedef enum
{
    INPUT_ACTION_MOVE_UP = 0,
    INPUT_ACTION_MOVE_DOWN,
    INPUT_ACTION_MOVE_LEFT,
    INPUT_ACTION_MOVE_RIGHT,
    INPUT_ACTION_TOGGLE_INVENTORY,
    INPUT_ACTION_TOGGLE_BUILDING_NAMES,
    INPUT_ACTION_TOGGLE_PAUSE,
    INPUT_ACTION_COUNT
} InputAction;

/**
 * @brief Keyboard bindings associated with each logical action.
 */
typedef struct
{
    KeyboardKey moveUp;
    KeyboardKey moveDown;
    KeyboardKey moveLeft;
    KeyboardKey moveRight;
    KeyboardKey toggleInventory;
    KeyboardKey toggleBuildingNames;
    KeyboardKey togglePause;
} KeyBindings;

/**
 * @brief Stores the current input and editor selection state.
 */
typedef struct
{
    TileTypeID     selectedTile;      /**< Currently selected tile type (for ground painting). */
    ObjectTypeID   selectedObject;    /**< Currently selected object type (for placement). */
    EntitiesTypeID selectedEntity;    /**< Currently selected entity type for spawning. */
    bool           showBuildingNames; /**< Whether building names are displayed (toggled by TAB). */
    CameraInput    camera;            /**< Camera movement & zoom input. */
    SelectionMode  currentMode;
    int            tileIndex;
    int            objectIndex;
    KeyBindings    bindings;          /**< Rebindable keyboard actions. */
} InputState;

/**
 * @brief Stores mouse information relative to the world.
 */
typedef struct
{
    Vector2 screen;    /**< Mouse position in screen coordinates. */
    Vector2 world;     /**< Mouse position in world-space coordinates (affected by camera). */
    int     tileX;     /**< Tile coordinate under the mouse (world.x / TILE_SIZE). */
    int     tileY;     /**< Tile coordinate under the mouse (world.y / TILE_SIZE). */
    bool    insideMap; /**< True if within map bounds. */
} MouseState;

/**
 * @brief Initializes the input state to default values.
 *
 * Sets:
 *  - selectedTile = TILE_GRASS
 *  - selectedObject = OBJ_NONE
 *  - showBuildingNames = false
 *
 * @param[out] input Pointer to the InputState structure to initialize.
 */
void input_init(InputState* input);

/**
 * @brief Polls keyboard input and updates the selection state accordingly.
 *
 * Supported controls:
 *  - [1–3]: Select ground tile types
 *  - [4–6]: Select objects (walls, doors, beds)
 *  - [TAB]: Toggle building name display
 *
 * @param[in,out] input Pointer to the InputState to update.
 */
void input_update(InputState* input);

/**
 * @brief Updates mouse world/tile position based on the camera.
 *
 * @param[out] mouse Pointer to MouseState to fill.
 * @param[in] camera Current camera.
 * @param[in] map Pointer to the Map (for bounds checking).
 */
void input_update_mouse(MouseState* mouse, const Camera2D* camera, const Map* map);

/**
 * @brief Restores the provided bindings structure to the default layout.
 */
void input_bindings_reset_default(KeyBindings* bindings);

/**
 * @brief Returns a localized display name for an input action.
 */
const char* input_action_display_name(InputAction action);

/**
 * @brief Retrieves the key bound to the provided action.
 */
KeyboardKey input_get_binding(const KeyBindings* bindings, InputAction action);

/**
 * @brief Updates the key bound to the provided action.
 */
void input_set_binding(KeyBindings* bindings, InputAction action, KeyboardKey key);

/**
 * @brief Prevents duplicate bindings by checking whether a key is already in use.
 */
bool input_is_key_already_bound(const KeyBindings* bindings, KeyboardKey key, InputAction* outAction);

#endif // INPUT_H
