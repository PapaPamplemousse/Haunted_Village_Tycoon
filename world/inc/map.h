
#ifndef MAP_H
#define MAP_H

#include "building.h"
#include "raylib.h"

/**
 * Map dimensions and tile size used for the isometric grid.  The width
 * and height specify the number of cells along the x and y axes of
 * the underlying map.  TILE_SIZE defines the horizontal size of a
 * single isometric diamond.  The vertical size is half of this
 * value when rendering.  Feel free to adjust these values to suit
 * your level design.
 */
#define MAP_WIDTH  40
#define MAP_HEIGHT 40
#define TILE_SIZE  128

/*
 * Global map state.  Each element of the grid stores the
 * BuildingID occupying that cell.  A value of BUILDING_NONE
 * indicates that the tile is empty.  This array is defined in
 * map.c and declared here for external access by other modules
 * (notably building.c).  Use the provided helper functions to
 * query or modify occupancy rather than writing to this array
 * directly.
 */
extern BuildingID grid[MAP_HEIGHT][MAP_WIDTH];

/*
 * Initialise any resources needed by the map.  This should be
 * called once at application startup.  Currently there is no
 * dynamic allocation involved, but the call exists for symmetry
 * with map_unload() and future expansion.
 */
void map_init(void);

/*
 * Release any resources acquired by map_init().  Call this once
 * before shutting down the application.  At present this is a
 * no‑op but is provided for completeness.
 */
void map_unload(void);

/*
 * Render the entire isometric map.  This function draws the
 * underlying ground tiles, highlights the tile currently under
 * the mouse cursor, shows a tinted preview of the building
 * selected for placement, and renders all buildings that have
 * been placed on the grid.  The supplied camera controls the
 * visible area and zoom.
 */
void draw_map(Camera2D camera);

/*
 * Update the map in response to user interaction.  This
 * function converts the mouse position to map coordinates and
 * dispatches build or remove operations based on mouse button
 * presses.  It should be called once per frame prior to
 * drawing the map.
 */
void update_map(Camera2D camera);

/*
 * Convert a world space position (typically obtained via
 * GetScreenToWorld2D()) into map grid coordinates.  The
 * returned Vector2 contains floating point values which should
 * be truncated or rounded to integer indices before use.  This
 * conversion is used both when placing structures and when
 * determining which tile to highlight.
 */
Vector2 world_to_grid(Vector2 worldMouse);

/*
 * Mark a rectangular region of the grid as occupied by the
 * specified building.  The origin indicates the top‑left tile
 * of the building footprint.  Calling this function does not
 * perform any boundary or collision checks; callers should
 * ensure the footprint is valid before invoking it.  It is
 * primarily used internally by the building subsystem.
 */
void occupy_tiles(Vector2 origin, BuildingID id);
#endif
