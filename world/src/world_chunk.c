/**
 * @file world_chunk.c
 * @brief Implements chunk-based caching to accelerate world rendering.
 */

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
            MapChunk* c   = &cg->chunks[cy * cg->chunksX + cx];
            c->cx         = cx;
            c->cy         = cy;
            c->rt.id      = 0;    // lazy GPU allocation
            c->dirty      = true; // needs first build
            c->buildTimer = 0;
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

void chunkgrid_redraw_cell(ChunkGrid* cg, Map* map, int x, int y)
{
    if (!cg || !map)
        return;

    // Trouve le chunk correspondant à cette tuile
    int cx = x / CHUNK_W;
    int cy = y / CHUNK_H;

    if (cx < 0 || cy < 0 || cx >= cg->chunksX || cy >= cg->chunksY)
        return;

    MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];

    // Si le chunk n’a pas encore de texture (premier build)
    if (c->rt.id == 0)
    {
        c->dirty = true;
        return;
    }

    const int chunkPixelWidth  = CHUNK_W * TILE_SIZE;
    const int chunkPixelHeight = CHUNK_H * TILE_SIZE;
    const int originTileX      = c->cx * CHUNK_W;
    const int originTileY      = c->cy * CHUNK_H;
    const int originPixelX     = originTileX * TILE_SIZE;
    const int originPixelY     = originTileY * TILE_SIZE;

    Object*   o              = map->objects[y][x];
    bool      drawObject     = false;
    Rectangle objectSrc      = {0};
    Vector2   objectLocalPos = {0};

    if (o && o->type && !o->type->activatable)
    {
        drawObject = true;

        int frameIndex = object_static_frame(o);
        if (o->type->texture.id != 0)
            objectSrc = object_type_frame_rect(o->type, frameIndex);
        else
        {
            float fw  = (float)(o->type->spriteFrameWidth > 0 ? o->type->spriteFrameWidth : TILE_SIZE);
            float fh  = (float)(o->type->spriteFrameHeight > 0 ? o->type->spriteFrameHeight : TILE_SIZE);
            objectSrc = (Rectangle){0.0f, 0.0f, fw, fh};
        }

        Vector2 drawPos = object_frame_draw_position(o, (int)objectSrc.width, (int)objectSrc.height);
        objectLocalPos  = (Vector2){drawPos.x - originPixelX, drawPos.y - originPixelY};

        if (objectLocalPos.x < -1.0f || objectLocalPos.y < -1.0f || objectLocalPos.x + objectSrc.width > (float)chunkPixelWidth + 1.0f || objectLocalPos.y + objectSrc.height > (float)chunkPixelHeight + 1.0f)
        {
            c->dirty = true;
            return;
        }
    }
    else if (o && o->type && o->type->activatable)
    {
        drawObject = false;
    }
    // Coordonnées locales dans la texture du chunk
    int localX = (x % CHUNK_W) * TILE_SIZE;
    int localY = (y % CHUNK_H) * TILE_SIZE;

    BeginTextureMode(c->rt);

    // --- Active un "clip" pour limiter le dessin à la tuile ---
    BeginScissorMode(localX, localY, TILE_SIZE, TILE_SIZE);

    // Efface juste cette zone (fond transparent)
    DrawRectangle(localX, localY, TILE_SIZE, TILE_SIZE, BLANK);

    // --- Redessine la tuile ---
    const TileType* tt = get_tile_type(map->tiles[y][x]);
    if (tt)
        tile_draw(tt, x, y, (float)localX, (float)localY);

    // --- Redessine l’objet éventuel ---
    if (drawObject)
    {
        // On redessine normalement, mais tout ce qui dépasse la tuile sera "coupé"
        if (o->type->texture.id)
            DrawTextureRec(o->type->texture, objectSrc, objectLocalPos, WHITE);
        else
        {
            Rectangle fill = {
                .x      = objectLocalPos.x + 2.0f,
                .y      = objectLocalPos.y + 2.0f,
                .width  = objectSrc.width - 4.0f,
                .height = objectSrc.height - 4.0f,
            };
            DrawRectangleRec(fill, o->type->color);
        }
    }

    EndScissorMode();
    EndTextureMode();
    // // Coordonnées locales dans la texture du chunk
    // int localX = (x % CHUNK_W) * TILE_SIZE;
    // int localY = (y % CHUNK_H) * TILE_SIZE;

    // // Prépare le dessin dans le RenderTexture existant
    // BeginTextureMode(c->rt);

    // // Efface juste cette zone (fond transparent)
    // DrawRectangle(localX, localY, TILE_SIZE, TILE_SIZE, BLANK);

    // // --- Redessine la tuile ---
    // const TileType* tt = get_tile_type(map->tiles[y][x]);
    // if (tt)
    //     tile_draw(tt, x, y, (float)localX, (float)localY);

    // // --- Redessine l’objet éventuel ---
    // if (drawObject)
    // {
    //     DrawRectangleRec((Rectangle){objectLocalPos.x, objectLocalPos.y, objectSrc.width, objectSrc.height}, BLANK);
    //     if (o->type->texture.id)
    //         DrawTextureRec(o->type->texture, objectSrc, objectLocalPos, WHITE);
    //     else
    //     {
    //         Rectangle fill = {
    //             .x      = objectLocalPos.x + 2.0f,
    //             .y      = objectLocalPos.y + 2.0f,
    //             .width  = objectSrc.width - 4.0f,
    //             .height = objectSrc.height - 4.0f,
    //         };
    //         DrawRectangleRec(fill, o->type->color);
    //     }
    // }

    // EndTextureMode();

    c->dirty = false;
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

    // Render into a temporary texture first
    RenderTexture2D temp = LoadRenderTexture(CHUNK_W * TILE_SIZE, CHUNK_H * TILE_SIZE);

    BeginTextureMode(temp);
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

            tile_draw(tt, x, y, (float)px, (float)py);
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
            if (!o || !o->type || o->type->activatable)
                continue;

            int       frameIndex = object_static_frame(o);
            Rectangle src;
            if (o->type->texture.id)
                src = object_type_frame_rect(o->type, frameIndex);
            else
            {
                float fw = (float)(o->type->spriteFrameWidth > 0 ? o->type->spriteFrameWidth : TILE_SIZE);
                float fh = (float)(o->type->spriteFrameHeight > 0 ? o->type->spriteFrameHeight : TILE_SIZE);
                src      = (Rectangle){0.0f, 0.0f, fw, fh};
            }

            Vector2 drawPos  = object_frame_draw_position(o, (int)src.width, (int)src.height);
            Vector2 localPos = {
                drawPos.x - (float)(x0 * TILE_SIZE),
                drawPos.y - (float)(y0 * TILE_SIZE),
            };

            if (o->type->texture.id)
                DrawTextureRec(o->type->texture, src, localPos, WHITE);
            else
            {
                Rectangle fill = {
                    .x      = localPos.x + 2.0f,
                    .y      = localPos.y + 2.0f,
                    .width  = src.width - 4.0f,
                    .height = src.height - 4.0f,
                };
                DrawRectangleRec(fill, o->type->color);
            }
        }
    }

    EndTextureMode();

    // Swap textures atomically (no black flash)
    if (c->rt.id != 0)
        UnloadRenderTexture(c->rt);

    c->rt         = temp;
    c->dirty      = false;
    c->buildTimer = 0.0001f; // used for fade-in animation
}

// ---------------------------------------------------------------
//  Cull + rebuild visible chunks only
// ---------------------------------------------------------------

void chunkgrid_draw_visible(ChunkGrid* cg, Map* map, Camera2D* cam)
{
    if (!cg)
        return;

    const float invZoom = 1.0f / cam->zoom;
    Rectangle   view    = {cam->target.x - cam->offset.x * invZoom, cam->target.y - cam->offset.y * invZoom, GetScreenWidth() * invZoom, GetScreenHeight() * invZoom};

    // Increase preload margin to prepare chunks off-screen
    const int preloadMargin = 2; // 2 chunks around viewport
    int       x0            = clampi((int)floorf(view.x / (CHUNK_W * TILE_SIZE)) - preloadMargin, 0, cg->chunksX - 1);
    int       y0            = clampi((int)floorf(view.y / (CHUNK_H * TILE_SIZE)) - preloadMargin, 0, cg->chunksY - 1);
    int       x1            = clampi((int)ceilf((view.x + view.width) / (CHUNK_W * TILE_SIZE)) + preloadMargin, 0, cg->chunksX - 1);
    int       y1            = clampi((int)ceilf((view.y + view.height) / (CHUNK_H * TILE_SIZE)) + preloadMargin, 0, cg->chunksY - 1);

    // Only rebuild a few chunks per frame to avoid stutter
    const int rebuildBudget = 3;
    int       rebuilt       = 0;

    // PASS 1 – rebuild missing/dirty chunks (off-screen work)
    for (int cy = y0; cy <= y1 && rebuilt < rebuildBudget; ++cy)
    {
        for (int cx = x0; cx <= x1 && rebuilt < rebuildBudget; ++cx)
        {
            MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];
            if ((c->rt.id == 0 || c->dirty) && rebuilt < rebuildBudget)
            {
                rebuild_chunk(c, map);
                rebuilt++;
            }
        }
    }

    // PASS 2 – draw only chunks that have a valid texture
    const int drawMargin = 1; // actual visible area
    x0                   = clampi((int)floorf(view.x / (CHUNK_W * TILE_SIZE)) - drawMargin, 0, cg->chunksX - 1);
    y0                   = clampi((int)floorf(view.y / (CHUNK_H * TILE_SIZE)) - drawMargin, 0, cg->chunksY - 1);
    x1                   = clampi((int)ceilf((view.x + view.width) / (CHUNK_W * TILE_SIZE)) + drawMargin, 0, cg->chunksX - 1);
    y1                   = clampi((int)ceilf((view.y + view.height) / (CHUNK_H * TILE_SIZE)) + drawMargin, 0, cg->chunksY - 1);

    for (int cy = y0; cy <= y1; ++cy)
    {
        for (int cx = x0; cx <= x1; ++cx)
        {
            MapChunk* c = &cg->chunks[cy * cg->chunksX + cx];
            if (c->rt.id == 0)
                continue;

            const float wx = (float)(cx * CHUNK_W * TILE_SIZE);
            const float wy = (float)(cy * CHUNK_H * TILE_SIZE);

            // Optional fade - in handled below(#4) Color tint = WHITE;
            Color tint = WHITE;
            // if (c->buildTimer > 0.0f)
            // {
            float alpha = fminf(c->buildTimer / 0.3f, 1.0f);
            tint.a      = (unsigned char)(alpha * 255);
            c->buildTimer += GetFrameTime();
            //}

            DrawTextureRec(c->rt.texture, (Rectangle){0, 0, (float)c->rt.texture.width, -(float)c->rt.texture.height}, (Vector2){wx, wy}, tint);
        }
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
