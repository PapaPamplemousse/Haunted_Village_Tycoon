// SPDX-License-Identifier: MIT
#ifndef BUILDING_H
#define BUILDING_H

#include "raylib.h"

/*
 * Enumeration of the different building types.  The first entry
 * (BUILDING_NONE) is reserved to indicate that no building is
 * present on a given map tile.  Additional entries define the
 * various concrete structures the player can construct.  Keep the
 * values contiguous as the count is used for array sizing.
 */
typedef enum BuildingID {
  BUILDING_NONE = 0,
  BUILDING_HOUSE,
  BUILDING_TOWNHALL,
  BUILDING_COUNT
} BuildingID;

/*
 * Metadata for a single type of building.  A BuildingType
 * describes how many tiles a structure occupies on the map as
 * well as the texture to draw and a friendly name for debugging
 * or UI purposes.
 */
typedef struct BuildingType {
  const char *name;  // Human readable name of the building
  Texture2D texture; // Loaded sprite texture used for rendering
  int width;         // Width in tiles that this building occupies
  int height;        // Height in tiles that this building occupies
  float scale;
} BuildingType;

/*
 * Initialise the building subsystem.  This function should be
 * called once at application startup.  It loads the textures
 * associated with each building type and populates internal
 * metadata.  If textures cannot be found the game will still run
 * but the buildings will not be visible.
 */
void building_system_init(void);

/*
 * Free any resources allocated by the building subsystem.  Call
 * this at application shutdown to properly unload textures.
 */
void building_system_unload(void);

/*
 * Obtain a pointer to the BuildingType description for a given
 * BuildingID.  Returns NULL if the id is outside the valid
 * range.
 */
BuildingType *get_building_type(BuildingID id);

/*
 * Get or set the currently selected building.  When the user
 * presses a key to change what they intend to place next, use
 * these functions to update the selection.  The selected value
 * defaults to BUILDING_HOUSE at startup.
 */
BuildingID get_selected_building(void);
void set_selected_building(BuildingID id);

/*
 * Poll for debug keyboard input and update the selected building
 * accordingly.  Currently KEY_ONE selects BUILDING_HOUSE, KEY_TWO
 * selects BUILDING_TOWNHALL and KEY_ZERO clears the selection
 * (BUILDING_NONE).  This behaviour can be changed or removed
 * later when a proper UI is implemented.
 */
void handle_building_input(void);

/*
 * Determine whether a building of the currently selected type
 * could be placed at the given grid coordinate.  The check will
 * return false if any of the footprint tiles are outside the map
 * bounds or already occupied by another building.  The grid
 * coordinate should be supplied as integer values (although
 * expressed as a Vector2 for convenience).  Fractional parts are
 * ignored.
 */
bool can_place_building(Vector2 gridPos);

/*
 * Attempt to place the currently selected building at the given
 * grid coordinate.  If the footprint overlaps the edge of the map
 * or another structure, the placement fails and nothing is
 * modified.  On success all affected cells are marked with the
 * building ID and the structure will remain drawn on future
 * frames.
 */
void place_building(Vector2 gridPos);

/*
 * Remove any building occupying the given grid coordinate.  For
 * now this simply clears the specific tile back to BUILDING_NONE
 * and does not consider multi‑tile footprints.  Improvements can
 * be made here to remove the entire structure when any part is
 * clicked.
 */
void remove_building(Vector2 gridPos);

/*
 * Draw a semi‑transparent preview of the currently selected
 * building at the specified grid coordinate.  The preview is
 * tinted green if placement is possible at that location or red if
 * not.  It is up to the caller to ensure the supplied
 * coordinate lies within the map bounds before calling this
 * function.  The preview uses the BuildingType metadata to
 * compute a simple scaling so the footprint roughly matches the
 * intended number of tiles.
 */
void draw_building_preview(Vector2 gridPos);

#endif // BUILDING_H
