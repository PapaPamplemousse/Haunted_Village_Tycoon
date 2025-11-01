/**
 * @file tile.c
 * @brief Manages tile definitions and associated textures.
 */

#include "tile.h"
#include <stddef.h>
#include "tiles_loader.h"
#include <stdio.h>
#include <stdint.h>

TileType tileTypes[TILE_MAX] = {0};

static uint32_t tile_hash_coords(int x, int y, TileTypeID id)
{
    // Large primes to mix coordinates; ensures deterministic but varied results.
    uint32_t hx = (uint32_t)(x * 73856093);
    uint32_t hy = (uint32_t)(y * 19349663);
    uint32_t hi = (uint32_t)(id * 83492791);
    return hx ^ hy ^ hi;
}

void init_tile_types(void)
{
    // Load tile metadata from disk before uploading textures.
    (void)load_tiles_from_stv("data/tiles.stv", tileTypes, TILE_MAX);
    for (int i = 0; i < TILE_MAX; ++i)
    {
        if (tileTypes[i].textureVariations <= 0)
            tileTypes[i].textureVariations = 1;

        if (tileTypes[i].texturePath != NULL)
        {
            printf("Loading tile %d: %s (%s)\n", i, tileTypes[i].name ? tileTypes[i].name : "(null)", tileTypes[i].texturePath ? tileTypes[i].texturePath : "(null)");
            fflush(stdout);
            tileTypes[i].texture = LoadTexture(tileTypes[i].texturePath);

            int variations = tileTypes[i].textureVariations;
            if (variations < 1)
                variations = 1;
            int width = tileTypes[i].texture.width;
            int height = tileTypes[i].texture.height;

            int columns = tileTypes[i].variationColumns;
            int rows    = tileTypes[i].variationRows;

            if (columns > 0 && rows > 0)
            {
                int expected = columns * rows;
                if (tileTypes[i].textureVariations <= 1)
                {
                    tileTypes[i].textureVariations = expected;
                }
                else if (tileTypes[i].textureVariations != expected)
                {
                    printf("⚠️  Tile '%s' declares %d variations but grid %dx%d = %d. Using grid count.\n",
                           tileTypes[i].name ? tileTypes[i].name : "(unnamed)",
                           tileTypes[i].textureVariations,
                           columns,
                           rows,
                           expected);
                    tileTypes[i].textureVariations = expected;
                }

                if (columns > 0)
                    tileTypes[i].variationFrameWidth = width / columns;
                if (rows > 0)
                    tileTypes[i].variationFrameHeight = height / rows;

                if ((columns > 0 && width % columns != 0) || (rows > 0 && height % rows != 0))
                {
                    printf("⚠️  Texture '%s' (%dx%d) not evenly divisible by grid %dx%d.\n",
                           tileTypes[i].texturePath,
                           width,
                           height,
                           columns,
                           rows);
                }
            }
            else
            {
                if (variations > 1 && (variations > width || width % variations != 0))
                {
                    printf("⚠️  Texture '%s' width (%d) not divisible by %d variations, using frame width fallback.\n",
                           tileTypes[i].texturePath,
                           width,
                           tileTypes[i].textureVariations);
                }

                tileTypes[i].variationFrameWidth =
                    (variations > 0 && width >= variations) ? (width / variations) : width;
                tileTypes[i].variationFrameHeight = height;

                if (columns <= 0)
                    tileTypes[i].variationColumns = variations;
                if (rows <= 0)
                    tileTypes[i].variationRows = 1;
            }

            if (tileTypes[i].variationFrameWidth <= 0)
                tileTypes[i].variationFrameWidth = width;
            if (tileTypes[i].variationFrameHeight <= 0)
                tileTypes[i].variationFrameHeight = height;
        }
        else
        {
            tileTypes[i].variationFrameWidth = 0;
            tileTypes[i].variationFrameHeight = 0;
        }
    }
}

void unload_tile_types(void)
{
    // Release textures that were created during initialization.
    for (int i = 0; i < TILE_MAX; ++i)
    {
        if (tileTypes[i].texture.id != 0)
        {
            UnloadTexture(tileTypes[i].texture);
            tileTypes[i].texture.id = 0;
        }
    }
}

TileType* get_tile_type(TileTypeID id)
{
    if (id >= 0 && id < TILE_MAX)
    {
        return &tileTypes[id];
    }
    return NULL;
}

Rectangle tile_get_source_rect(const TileType* type, int tileX, int tileY)
{
    if (!type || type->texture.id == 0)
        return (Rectangle){0, 0, 0, 0};

    int variations = type->textureVariations > 0 ? type->textureVariations : 1;
    float frameW   = (type->variationFrameWidth > 0) ? (float)type->variationFrameWidth : 0.0f;
    float frameH   = (type->variationFrameHeight > 0) ? (float)type->variationFrameHeight : 0.0f;

    if (variations <= 1 || frameW <= 0.0f || frameH <= 0.0f)
    {
        return (Rectangle){0.0f, 0.0f, (float)type->texture.width, (float)type->texture.height};
    }

    uint32_t hash    = tile_hash_coords(tileX, tileY, type->id);
    int      variant = (int)(hash % (uint32_t)variations);

    int columns = type->variationColumns > 0 ? type->variationColumns : variations;
    int rows    = type->variationRows > 0 ? type->variationRows : 1;
    if (columns <= 0)
        columns = 1;
    if (rows <= 0)
        rows = (variations + columns - 1) / columns;

    int col = variant % columns;
    int row = variant / columns;

    float srcX = frameW * (float)col;
    float srcY = frameH * (float)row;

    return (Rectangle){srcX, srcY, frameW, frameH};
}

void tile_draw(const TileType* type, int tileX, int tileY, float destX, float destY)
{
    if (!type)
        return;

    if (type->texture.id != 0)
    {
        Rectangle src  = tile_get_source_rect(type, tileX, tileY);
        Rectangle dest = {destX, destY, (float)TILE_SIZE, (float)TILE_SIZE};
        Vector2   origin = {0.0f, 0.0f};
        DrawTexturePro(type->texture, src, dest, origin, 0.0f, WHITE);
    }
    else
    {
        DrawRectangle((int)destX, (int)destY, TILE_SIZE, TILE_SIZE, type->color);
    }
}
