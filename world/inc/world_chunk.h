#ifndef WORLD_CHUNK_H
#define WORLD_CHUNK_H

#include "raylib.h"
#include "map.h"
#include <stdbool.h>
#include "world.h"

// ---------------------------------------------------------------------------
//  Global access
// ---------------------------------------------------------------------------

extern ChunkGrid* gChunks; ///< Global pointer to the active chunk grid

// ---------------------------------------------------------------------------
//  API
// ---------------------------------------------------------------------------

/**
 * @brief Create a new chunk grid for the given map.
 * @param map Pointer to the world map (width/height must be valid).
 * @return Newly allocated ChunkGrid.
 */
ChunkGrid* chunkgrid_create(Map* map);

/**
 * @brief Free all GPU resources and memory used by the chunk grid.
 */
void chunkgrid_destroy(ChunkGrid* cg);

/**
 * @brief Mark the chunk containing a specific tile as dirty.
 * @details Forces that chunk to be rebuilt next time it becomes visible.
 */
void chunkgrid_mark_dirty_tile(ChunkGrid* cg, int tileX, int tileY);

/**
 * @brief Mark all chunks dirty (for example, after world regeneration).
 */
void chunkgrid_mark_all(ChunkGrid* cg, Map* map);

/**
 * @brief Draw only chunks currently visible by the camera.
 *
 * This function lazily rebuilds missing or dirty chunks within a small
 * per-frame budget and draws their cached textures.  It should be called
 * once per frame during world rendering.
 */
void chunkgrid_draw_visible(ChunkGrid* cg, Map* map, Camera2D* cam);

/**
 * @brief Manually unload chunks that are far from the camera to save VRAM.
 *
 * @param maxDistancePx Distance in pixels beyond which a chunk will be freed.
 * @note Call this occasionally (e.g., every few seconds) — not every frame.
 */
void chunkgrid_evict_far(ChunkGrid* cg, const Camera2D* cam, float maxDistancePx);

#endif // WORLD_CHUNK_H
