// #include "world_chunk.h"
// #include "tile.h"
// #include "object.h"
// #include "raymath.h"
// #include <stdlib.h>
// #include <string.h>
// #include <stdio.h>

// ChunkGrid* gChunks = NULL;

// static inline int div_floor(int a, int b)
// {
//     return (a >= 0) ? (a / b) : ((a - b + 1) / b);
// }
// static inline int clampi(int v, int lo, int hi)
// {
//     return v < lo ? lo : (v > hi ? hi : v);
// }

// ChunkGrid* chunkgrid_create(Map* map)
// {
//     ChunkGrid* cg = (ChunkGrid*)calloc(1, sizeof(ChunkGrid));
//     cg->chunksX   = (map->width + CHUNK_W - 1) / CHUNK_W;
//     cg->chunksY   = (map->height + CHUNK_H - 1) / CHUNK_H;
//     cg->chunks    = (MapChunk*)calloc((size_t)(cg->chunksX * cg->chunksY), sizeof(MapChunk));
//     for (int cy = 0; cy < cg->chunksY; ++cy)
//     {
//         for (int cx = 0; cx < cg->chunksX; ++cx)
//         {
//             MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];
//             c->cx       = cx;
//             c->cy       = cy;
//             c->rt       = (RenderTexture2D){0}; // LoadRenderTexture(CHUNK_W * TILE_SIZE, CHUNK_H * TILE_SIZE);
//             c->rt.id    = 0;
//             c->dirty    = true;
//         }
//     }
//     return cg;
// }

// static int count_dirty_chunks(ChunkGrid* cg)
// {
//     int c = 0;
//     for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
//         if (cg->chunks[i].dirty)
//             c++;
//     return c;
// }

// void chunkgrid_destroy(ChunkGrid* cg)
// {
//     if (!cg)
//         return;
//     for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
//     {
//         if (cg->chunks[i].rt.id != 0)
//         {
//             printf("[FREE] Chunk (%d,%d) freed\n", cg->chunks[i].cx, cg->chunks[i].cy);
//             UnloadRenderTexture(cg->chunks[i].rt);
//         }
//     }
//     free(cg->chunks);
//     free(cg);
// }

// void chunkgrid_mark_dirty_tile(ChunkGrid* cg, int x, int y)
// {
//     if (!cg)
//         return;
//     int cx = x / CHUNK_W;
//     int cy = y / CHUNK_H;
//     if (cx < 0 || cy < 0 || cx >= cg->chunksX || cy >= cg->chunksY)
//         return;
//     cg->chunks[cy * cg->chunksX + cx].dirty = true;
// }

// void chunkgrid_mark_dirty_rect(ChunkGrid* cg, Rectangle tileRect)
// {
//     if (!cg)
//         return;
//     int x0 = (int)tileRect.x, y0 = (int)tileRect.y;
//     int x1  = x0 + (int)tileRect.width - 1;
//     int y1  = y0 + (int)tileRect.height - 1;
//     int cxl = div_floor(x0, CHUNK_W), cxh = div_floor(x1, CHUNK_W);
//     int cyl = div_floor(y0, CHUNK_H), cyh = div_floor(y1, CHUNK_H);
//     cxl = clampi(cxl, 0, cg->chunksX - 1);
//     cxh = clampi(cxh, 0, cg->chunksX - 1);
//     cyl = clampi(cyl, 0, cg->chunksY - 1);
//     cyh = clampi(cyh, 0, cg->chunksY - 1);
//     for (int cy = cyl; cy <= cyh; ++cy)
//         for (int cx = cxl; cx <= cxh; ++cx)
//             cg->chunks[cy * cg->chunksX + cx].dirty = true;
// }

// void chunkgrid_mark_all(ChunkGrid* cg, Map* map)
// {
//     if (!cg)
//         return;
//     chunkgrid_mark_dirty_rect(cg, (Rectangle){0, 0, (float)map->width, (float)map->height});
// }

// // Rebuild a single chunk’s cached texture
// static void rebuild_chunk(MapChunk* c, Map* map)
// {
//     const int tileX0 = c->cx * CHUNK_W;
//     const int tileY0 = c->cy * CHUNK_H;

//     if (c->rt.id == 0)
//     {
//         c->rt = LoadRenderTexture(CHUNK_W * TILE_SIZE, CHUNK_H * TILE_SIZE);
//         printf("[ALLOC] Chunk (%d,%d) allocated RenderTexture\n", c->cx, c->cy);
//     }

//     BeginTextureMode(c->rt);
//     ClearBackground(BLANK);

//     // --- Static background: tiles
//     for (int ty = 0; ty < CHUNK_H; ++ty)
//     {
//         int y = tileY0 + ty;
//         if (y >= map->height)
//             break;
//         for (int tx = 0; tx < CHUNK_W; ++tx)
//         {
//             int x = tileX0 + tx;
//             if (x >= map->width)
//                 break;
//             TileTypeID      id = map->tiles[y][x];
//             const TileType* tt = get_tile_type(id);
//             const int       px = tx * TILE_SIZE;
//             const int       py = ty * TILE_SIZE;

//             if (tt->texture.id > 0)
//             {
//                 // If you use a tileset atlas, draw the proper region here instead.
//                 DrawTexture(tt->texture, px, py, WHITE);
//             }
//             else
//             {
//                 DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, tt->color);
//             }
//         }
//     }

//     // --- Static objects (non-moving props, walls, doors, trees, rocks)
//     // NOTE: Dynamic entities (units, projectiles) should be drawn outside the cache every frame.
//     for (int ty = 0; ty < CHUNK_H; ++ty)
//     {
//         int y = tileY0 + ty;
//         if (y >= map->height)
//             break;
//         for (int tx = 0; tx < CHUNK_W; ++tx)
//         {
//             int x = tileX0 + tx;
//             if (x >= map->width)
//                 break;
//             Object* o = map->objects[y][x];
//             if (!o)
//                 continue;

//             const int         px   = tx * TILE_SIZE;
//             const int         py   = ty * TILE_SIZE;
//             const ObjectType* type = o->type;

//             DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, o->type->color);
//             if (type->texture.id != 0)
//             {
//                 // DrawTextureEx(type->texture, (Vector2){worldX, worldY}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);
//                 DrawTextureEx(type->texture, (Vector2){(float)px, (float)py}, 0.0f, 1.0f, WHITE);
//             }
//             else
//             {
//                 // --- otherwise colored rectangle ---
//                 float size   = TILE_SIZE * 0.6f; // plus petit que la tuile
//                 float offset = (TILE_SIZE - size) / 2.0f;
//                 DrawRectangle(px, py, size, size, o->type->color);
//             }
//         }
//     }
//     c->dirty = false;
//     EndTextureMode();
// }

// void chunkgrid_rebuild_dirty(ChunkGrid* cg, Map* map)
// {
//     if (!cg)
//         return;
//     for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
//     {
//         if (cg->chunks[i].dirty)
//         {
//             rebuild_chunk(&cg->chunks[i], map);
//         }
//     }
// }

// // Unload chunk textures that are far away from the camera
// void chunkgrid_evict_far(ChunkGrid* cg, const Camera2D* cam, float unloadDistancePx)
// {
//     if (!cg)
//         return;

//     const float camX = cam->target.x;
//     const float camY = cam->target.y;

//     for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
//     {
//         MapChunk* c = &cg->chunks[i];
//         if (c->rt.id == 0)
//             continue; // not allocated yet

//         // Compute the chunk’s world-space center (in pixels)
//         float wx = (c->cx + 0.5f) * CHUNK_W * TILE_SIZE;
//         float wy = (c->cy + 0.5f) * CHUNK_H * TILE_SIZE;

//         float dx     = wx - camX;
//         float dy     = wy - camY;
//         float dist2  = dx * dx + dy * dy;
//         float limit2 = unloadDistancePx * unloadDistancePx;

//         // If the chunk is well outside the safe radius → free its texture
//         if (dist2 > limit2)
//         {
//             UnloadRenderTexture(c->rt);
//             c->rt.id = 0;
//             c->dirty = true; // mark for rebuild if we ever come back
//         }
//     }
// }

// void chunkgrid_draw_visible(ChunkGrid* cg, Map* map, Camera2D* cam)
// {
//     if (!cg)
//         return;

//     static int drawnCount = 0;
//     drawnCount++;

//     Rectangle view = {cam->target.x - cam->offset.x / cam->zoom, cam->target.y - cam->offset.y / cam->zoom, GetScreenWidth() / cam->zoom, GetScreenHeight() / cam->zoom};

//     int x0 = (int)floorf(view.x / (CHUNK_W * TILE_SIZE)) - 1;
//     int y0 = (int)floorf(view.y / (CHUNK_H * TILE_SIZE)) - 1;
//     int x1 = (int)ceilf((view.x + view.width) / (CHUNK_W * TILE_SIZE)) + 1;
//     int y1 = (int)ceilf((view.y + view.height) / (CHUNK_H * TILE_SIZE)) + 1;

//     x0 = clampi(x0, 0, cg->chunksX - 1);
//     x1 = clampi(x1, 0, cg->chunksX - 1);
//     y0 = clampi(y0, 0, cg->chunksY - 1);
//     y1 = clampi(y1, 0, cg->chunksY - 1);

//     // Limit how many chunks can be rebuilt per frame
//     int rebuildBudget = 2; // tweak if needed

//     for (int cy = y0; cy <= y1; ++cy)
//     {
//         for (int cx = x0; cx <= x1; ++cx)
//         {
//             MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];

//             // Lazy creation with rebuild budget
//             if ((c->rt.id == 0 || c->dirty) && rebuildBudget > 0)
//             {
//                 extern void rebuild_chunk(MapChunk * c, Map * map);
//                 rebuild_chunk(c, map);
//                 c->dirty = false;
//                 rebuildBudget--;
//             }

//             if (c->rt.id == 0)
//                 continue; // still not ready, skip drawing

//             const float wx = (float)(cx * CHUNK_W * TILE_SIZE);
//             const float wy = (float)(cy * CHUNK_H * TILE_SIZE);

//             DrawTextureRec(c->rt.texture, (Rectangle){0, 0, (float)c->rt.texture.width, -(float)c->rt.texture.height}, (Vector2){wx, wy}, WHITE);
//         }
//     }
// }

// // void chunkgrid_draw_visible(ChunkGrid* cg, Map* map, Camera2D* cam)
// // {
// //     if (!cg)
// //         return;

// //     // Compute visible world rect in pixels
// //     Rectangle view = {cam->target.x - cam->offset.x / cam->zoom, cam->target.y - cam->offset.y / cam->zoom, GetScreenWidth() / cam->zoom, GetScreenHeight() / cam->zoom};

// //     // Add a margin of one chunk to avoid pop-in at edges while panning
// //     int x0 = (int)floorf(view.x / (CHUNK_W * TILE_SIZE)) - 1;
// //     int y0 = (int)floorf(view.y / (CHUNK_H * TILE_SIZE)) - 1;
// //     int x1 = (int)ceilf((view.x + view.width) / (CHUNK_W * TILE_SIZE)) + 1;
// //     int y1 = (int)ceilf((view.y + view.height) / (CHUNK_H * TILE_SIZE)) + 1;

// //     x0 = clampi(x0, 0, cg->chunksX - 1);
// //     x1 = clampi(x1, 0, cg->chunksX - 1);
// //     y0 = clampi(y0, 0, cg->chunksY - 1);
// //     y1 = clampi(y1, 0, cg->chunksY - 1);

// //     for (int cy = y0; cy <= y1; ++cy)
// //     {
// //         for (int cx = x0; cx <= x1; ++cx)
// //         {
// //             MapChunk*   c  = &cg->chunks[cy * cg->chunksX + cx];
// //             const float wx = (float)(cx * CHUNK_W * TILE_SIZE);
// //             const float wy = (float)(cy * CHUNK_H * TILE_SIZE);

// //             // RenderTexture2D has inverted Y, draw using a flipped source rect
// //             DrawTextureRec(c->rt.texture, (Rectangle){0, 0, (float)c->rt.texture.width, -(float)c->rt.texture.height}, (Vector2){wx, wy}, WHITE);
// //         }
// //     }
// // }
#include "world_chunk.h"
#include "tile.h"
#include "object.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ChunkGrid* gChunks = NULL;

static inline int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// ---------------------------------------------------------------
//  Creation / destruction
// ---------------------------------------------------------------

ChunkGrid* chunkgrid_create(Map* map)
{
    ChunkGrid* cg = calloc(1, sizeof(ChunkGrid));
    cg->chunksX   = (map->width + CHUNK_W - 1) / CHUNK_W;
    cg->chunksY   = (map->height + CHUNK_H - 1) / CHUNK_H;
    cg->chunks    = calloc((size_t)cg->chunksX * cg->chunksY, sizeof(MapChunk));

    for (int cy = 0; cy < cg->chunksY; ++cy)
        for (int cx = 0; cx < cg->chunksX; ++cx)
        {
            MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];
            c->cx       = cx;
            c->cy       = cy;
            c->rt.id    = 0;    // lazy GPU allocation
            c->dirty    = true; // needs first build
        }

    return cg;
}

void chunkgrid_destroy(ChunkGrid* cg)
{
    if (!cg)
        return;

    for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
        if (cg->chunks[i].rt.id != 0)
            UnloadRenderTexture(cg->chunks[i].rt);

    free(cg->chunks);
    free(cg);
}

// ---------------------------------------------------------------
//  Marking dirty regions
// ---------------------------------------------------------------

void chunkgrid_mark_dirty_tile(ChunkGrid* cg, int x, int y)
{
    if (!cg)
        return;
    int cx = x / CHUNK_W;
    int cy = y / CHUNK_H;
    if (cx < 0 || cy < 0 || cx >= cg->chunksX || cy >= cg->chunksY)
        return;
    cg->chunks[cy * cg->chunksX + cx].dirty = true;
}

// ---------------------------------------------------------------
//  Internal: draw a tile/object chunk into its RenderTexture
// ---------------------------------------------------------------

static void rebuild_chunk(MapChunk* c, Map* map)
{
    const int x0 = c->cx * CHUNK_W;
    const int y0 = c->cy * CHUNK_H;

    // Lazy GPU texture allocation
    if (c->rt.id == 0)
        c->rt = LoadRenderTexture(CHUNK_W * TILE_SIZE, CHUNK_H * TILE_SIZE);

    BeginTextureMode(c->rt);
    ClearBackground(BLANK);

    // --- Tiles ---
    for (int ty = 0; ty < CHUNK_H; ++ty)
    {
        int y = y0 + ty;
        if (y >= map->height)
            break;

        for (int tx = 0; tx < CHUNK_W; ++tx)
        {
            int x = x0 + tx;
            if (x >= map->width)
                break;

            const TileType* tt = get_tile_type(map->tiles[y][x]);
            int             px = tx * TILE_SIZE;
            int             py = ty * TILE_SIZE;

            if (tt->texture.id)
                DrawTexture(tt->texture, px, py, WHITE);
            else
                DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, tt->color);
        }
    }

    // --- Static objects ---
    for (int ty = 0; ty < CHUNK_H; ++ty)
    {
        int y = y0 + ty;
        if (y >= map->height)
            break;

        for (int tx = 0; tx < CHUNK_W; ++tx)
        {
            int x = x0 + tx;
            if (x >= map->width)
                break;

            Object* o = map->objects[y][x];
            if (!o)
                continue;

            int px = tx * TILE_SIZE;
            int py = ty * TILE_SIZE;

            if (o->type->texture.id)
                DrawTextureEx(o->type->texture, (Vector2){(float)px, (float)py}, 0.0f, 1.0f, WHITE);
            else
                DrawRectangle(px + 2, py + 2, TILE_SIZE - 4, TILE_SIZE - 4, o->type->color);
        }
    }

    EndTextureMode();
    c->dirty = false;
}

// ---------------------------------------------------------------
//  Cull + rebuild visible chunks only
// ---------------------------------------------------------------

void chunkgrid_draw_visible(ChunkGrid* cg, Map* map, Camera2D* cam)
{
    if (!cg)
        return;

    // Compute camera view rect in world pixels
    const float invZoom = 1.0f / cam->zoom;
    Rectangle   view    = {cam->target.x - cam->offset.x * invZoom, cam->target.y - cam->offset.y * invZoom, GetScreenWidth() * invZoom, GetScreenHeight() * invZoom};

    // Visible chunk range + one margin chunk around to avoid pop-in
    int x0 = clampi((int)floorf(view.x / (CHUNK_W * TILE_SIZE)) - 1, 0, cg->chunksX - 1);
    int y0 = clampi((int)floorf(view.y / (CHUNK_H * TILE_SIZE)) - 1, 0, cg->chunksY - 1);
    int x1 = clampi((int)ceilf((view.x + view.width) / (CHUNK_W * TILE_SIZE)) + 1, 0, cg->chunksX - 1);
    int y1 = clampi((int)ceilf((view.y + view.height) / (CHUNK_H * TILE_SIZE)) + 1, 0, cg->chunksY - 1);

    const int rebuildPerFrame = 2; // budgeted rebuilds
    int       rebuilt         = 0;

    for (int cy = y0; cy <= y1; ++cy)
        for (int cx = x0; cx <= x1; ++cx)
        {
            MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];

            // Lazy build within frame budget
            if ((c->rt.id == 0 || c->dirty) && rebuilt < rebuildPerFrame)
            {
                rebuild_chunk(c, map);
                rebuilt++;
            }

            if (c->rt.id == 0)
                continue; // not ready yet

            const float wx = (float)(cx * CHUNK_W * TILE_SIZE);
            const float wy = (float)(cy * CHUNK_H * TILE_SIZE);

            // Flip Y because RenderTexture is upside-down
            DrawTextureRec(c->rt.texture, (Rectangle){0, 0, (float)c->rt.texture.width, -(float)c->rt.texture.height}, (Vector2){wx, wy}, WHITE);
        }
}

// ---------------------------------------------------------------
//  Optional eviction of far chunks (manual call)
// ---------------------------------------------------------------

void chunkgrid_evict_far(ChunkGrid* cg, const Camera2D* cam, float maxDistancePx)
{
    if (!cg)
        return;

    const float camX   = cam->target.x;
    const float camY   = cam->target.y;
    const float limit2 = maxDistancePx * maxDistancePx;

    for (int i = 0; i < cg->chunksX * cg->chunksY; ++i)
    {
        MapChunk* c = &cg->chunks[i];
        if (c->rt.id == 0)
            continue;

        float wx = (c->cx + 0.5f) * CHUNK_W * TILE_SIZE;
        float wy = (c->cy + 0.5f) * CHUNK_H * TILE_SIZE;
        float dx = wx - camX;
        float dy = wy - camY;
        if ((dx * dx + dy * dy) > limit2)
        {
            UnloadRenderTexture(c->rt);
            c->rt.id = 0;
            c->dirty = true;
        }
    }
}
