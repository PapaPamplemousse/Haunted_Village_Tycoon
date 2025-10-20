// SPDX-License-Identifier: MIT
//
// building.c
//
// Implementation of the building subsystem for Containment Tycoon.  This
// module is responsible for describing the various structures the player
// can construct, managing which type is currently selected for placement
// via debug keybinds, determining whether a structure can be placed at a
// given location, marking map tiles as occupied and drawing tinted
// previews.  It collaborates closely with the map module which owns
// the global occupancy grid and performs the actual rendering of
// existing structures.

#include "building.h"
#include "map.h"
#include <stdio.h>

/*
 * Internal array storing metadata about each building type.  The
 * BuildingID enum values defined in building.h map directly into
 * this array.  BUILDING_NONE is used to indicate an empty tile and
 * has no associated texture or footprint.  Actual building types
 * describe how many tiles they occupy and which sprite to draw.
 */
static BuildingType buildingTypes[BUILDING_COUNT];

/*
 * The currently selected building type.  By default the house is
 * selected.  Players can change the selection via handle_building_input()
 * or by directly calling set_selected_building().
 */
static BuildingID selectedBuilding = BUILDING_HOUSE;

/*
 * Helper used internally to convert a grid coordinate to an isometric
 * world position.  This function is replicated here to avoid a
 * dependency back on map.c for preview rendering.  It mirrors the
 * implementation in map.c.
 */
static Vector2 to_iso_internal(int x, int y)
{
    return (Vector2){(float)(x - y) * ((float)TILE_SIZE / 2.0f), (float)(x + y) * ((float)TILE_SIZE / 4.0f)};
}

/*
 * Initialise the building subsystem.  This function must be called
 * exactly once during application startup.  It loads the textures
 * associated with each concrete building type and populates the
 * metadata table.  If a texture fails to load the building will
 * still be present but will not be visible when rendered.
 */
void building_system_init(void)
{
    // Initialise the reserved none entry
    buildingTypes[BUILDING_NONE].name    = "None";
    buildingTypes[BUILDING_NONE].texture = (Texture2D){0};
    buildingTypes[BUILDING_NONE].width   = 1;
    buildingTypes[BUILDING_NONE].height  = 1;
    buildingTypes[BUILDING_HOUSE].scale  = 0.25f;

    // Configure the house.  The house occupies a 2x2 footprint on the
    // isometric grid and uses assets/building_house.png for its sprite.
    buildingTypes[BUILDING_HOUSE].name    = "House";
    buildingTypes[BUILDING_HOUSE].width   = 2;
    buildingTypes[BUILDING_HOUSE].height  = 2;
    buildingTypes[BUILDING_HOUSE].texture = LoadTexture("assets/building_house.png");
    buildingTypes[BUILDING_HOUSE].scale   = 0.25f;

    // Configure the town hall.  It occupies a 4x2 footprint (eight tiles)
    // and uses assets/building_townhall.png for its sprite.
    buildingTypes[BUILDING_TOWNHALL].name    = "Townhall";
    buildingTypes[BUILDING_TOWNHALL].width   = 4;
    buildingTypes[BUILDING_TOWNHALL].height  = 2;
    buildingTypes[BUILDING_TOWNHALL].texture = LoadTexture("assets/building_townhall.png");
    buildingTypes[BUILDING_HOUSE].scale      = 0.25f;

    // Default selection when the game starts
    selectedBuilding = BUILDING_HOUSE;
}

/*
 * Unload any textures associated with building types.  Call this
 * exactly once at application shutdown to free GPU resources.
 */
void building_system_unload(void)
{
    for (int i = 0; i < BUILDING_COUNT; i++)
    {
        if (buildingTypes[i].texture.id != 0)
        {
            UnloadTexture(buildingTypes[i].texture);
            buildingTypes[i].texture = (Texture2D){0};
        }
    }
}

/*
 * Return a pointer to the metadata for a particular building ID.
 * Returns NULL if the id is out of range.  Callers should not
 * modify the returned struct fields directly.
 */
BuildingType* get_building_type(BuildingID id)
{
    if (id >= 0 && id < BUILDING_COUNT)
    {
        return &buildingTypes[id];
    }
    return NULL;
}

/*
 * Get the identifier for the building currently selected by the
 * player.
 */
BuildingID get_selected_building(void)
{
    return selectedBuilding;
}

/*
 * Change the currently selected building.  Passing an invalid id
 * results in no change.  This function does not validate that
 * assets for the new selection have been loaded; that is the
 * responsibility of building_system_init().
 */
void set_selected_building(BuildingID id)
{
    if (id >= 0 && id < BUILDING_COUNT)
    {
        selectedBuilding = id;
    }
}

/*
 * Handle debug keybinds for changing the selected building.  This
 * routine polls for KEY_ONE, KEY_TWO and KEY_ZERO (house, town hall
 * and none respectively) and updates the selection accordingly.  In
 * future these bindings can be replaced with UI buttons or other
 * control schemes.
 */
void handle_building_input(void)
{
    if (IsKeyPressed(KEY_ONE))
    {
        set_selected_building(BUILDING_HOUSE);
        printf("Selected: HOUSE\n");
    }
    else if (IsKeyPressed(KEY_TWO))
    {
        set_selected_building(BUILDING_TOWNHALL);
        printf("Selected: TOWNHALL\n");
    }
    else if (IsKeyPressed(KEY_ZERO))
    {
        set_selected_building(BUILDING_NONE);
        printf("Selected: NONE\n");
    }
}

/*
 * Determine whether the currently selected building can be placed at
 * the specified grid coordinate.  Returns false if the footprint
 * would extend outside the map bounds or if any of the tiles are
 * already occupied.  The coordinate should be the top‑left tile of
 * the proposed footprint.  Fractional components are truncated to
 * integer tile indices.
 */
bool can_place_building(Vector2 gridPos)
{
    BuildingType* type = get_building_type(selectedBuilding);
    if (!type)
        return false;
    int gx = (int)gridPos.x;
    int gy = (int)gridPos.y;
    for (int dy = 0; dy < type->height; dy++)
    {
        for (int dx = 0; dx < type->width; dx++)
        {
            int x = gx + dx;
            int y = gy + dy;
            // Out of bounds check
            if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
            {
                return false;
            }
            // Collision check
            if (grid[y][x] != BUILDING_NONE)
            {
                return false;
            }
        }
    }
    return true;
}

/*
 * Attempt to place the currently selected building at the given grid
 * coordinate.  On success the footprint tiles are marked with the
 * selected building ID and the structure will persist on the map.
 * If placement is not possible the grid is unchanged and a message
 * is printed to stdout for debugging.
 */
void place_building(Vector2 gridPos)
{
    BuildingType* type = get_building_type(selectedBuilding);
    if (!type || selectedBuilding == BUILDING_NONE)
    {
        return;
    }
    if (!can_place_building(gridPos))
    {
        printf("Cannot place building here!\n");
        return;
    }
    occupy_tiles(gridPos, selectedBuilding);
    printf("Placed %s at (%d,%d) occupying %dx%d tiles\n", type->name, (int)gridPos.x, (int)gridPos.y, type->width, type->height);
}

/*
 * Remove any building occupying the given grid coordinate.  At
 * present this simply clears the specific tile back to
 * BUILDING_NONE.  Removing an entire multi‑tile structure when
 * clicked on any part could be implemented by scanning the
 * footprint or tracking origin tiles.
 */
void remove_building(Vector2 gridPos)
{
    int gx = (int)gridPos.x;
    int gy = (int)gridPos.y;
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT)
    {
        return;
    }
    grid[gy][gx] = BUILDING_NONE;
    printf("Removed building from (%d,%d)\n", gx, gy);
}

/*
 * Draw a semi‑transparent preview of the currently selected
 * structure at the supplied grid coordinate.  The preview is
 * coloured green if the placement would succeed or red if any
 * collisions or bounds violations would occur.  The preview
 * aligns its bottom centre roughly with the centre of the
 * footprint's top‑left tile.  This function does not check map
 * bounds; the caller should ensure the coordinate is within
 * range before drawing.
 */
void draw_building_preview(Vector2 gridPos)
{
    BuildingType* type = get_building_type(selectedBuilding);
    if (!type || type->texture.id == 0 || selectedBuilding == BUILDING_NONE)
    {
        return;
    }
    // Determine iso position of the top‑left tile
    Vector2 iso = to_iso_internal((int)gridPos.x, (int)gridPos.y);
    // Compute scale so that the building spans its width in tiles
    float scale = ((float)type->width * (float)TILE_SIZE) / (float)type->texture.width;
    // Position the sprite so its bottom centre sits at the centre of
    // the first tile.  The vertical offset aligns the base on the
    // diagonally shaped tile.  Adjustments could be made based on
    // sprite artwork if necessary.
    Vector2 drawPos;
    drawPos.x = iso.x + (float)TILE_SIZE / 2.0f - ((float)type->texture.width * scale) / 2.0f;
    drawPos.y = iso.y + (float)TILE_SIZE / 2.0f - ((float)type->texture.height * scale);
    // Tint green if allowed to place, otherwise red
    Color tint = can_place_building(gridPos) ? (Color){0, 255, 0, 128} : (Color){255, 0, 0, 128};
    DrawTextureEx(type->texture, drawPos, 0.0f, scale, tint);
}
