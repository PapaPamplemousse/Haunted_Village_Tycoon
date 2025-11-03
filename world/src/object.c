/**
 * @file object.c
 * @brief Provides object lifecycle management and room analysis helpers.
 */

#include "object.h"
#include "object_loader.h"
#include "building.h"
#include "map.h"
#include "tile.h"
#include "raylib.h"
#include "world_structures.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

// Static and constant global array containing all object type definitions.
// It uses the ObjectTypeID enumeration (e.g., [OBJ_BED_SMALL]) for indexing.
static ObjectType G_OBJECT_TYPES[OBJ_COUNT] = {0};
static Object*    G_DYNAMIC_OBJECTS         = NULL;
static bool       G_ENVIRONMENT_DIRTY       = true;

static bool object_type_is_dynamic(const ObjectType* type)
{
    if (!type)
        return false;
    return type->activatable;
}

static int clamp_int(int value, int minimum, int maximum)
{
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static void environment_reset(Map* map)
{
    if (!map)
        return;

    for (int y = 0; y < map->height; ++y)
    {
        memset(map->lightField[y], 0, (size_t)map->width * sizeof(float));
        memset(map->heatField[y], 0, (size_t)map->width * sizeof(float));
    }
}

static void environment_apply_object(Map* map, const Object* obj)
{
    if (!map || !obj || !obj->type)
        return;

    const ObjectType* type        = obj->type;
    bool              isActive    = !type->activatable || obj->isActive;
    int               lightRadius = type->lightRadius;
    int               heatRadius  = type->heatRadius;

    if (!isActive)
        return;

    if ((lightRadius <= 0 || type->lightLevel <= 0) && (heatRadius <= 0 || type->warmth <= 0))
        return;

    int maxRadius = lightRadius > heatRadius ? lightRadius : heatRadius;
    if (maxRadius <= 0)
        return;

    float centerX = obj->position.x + (float)type->width * 0.5f;
    float centerY = obj->position.y + (float)type->height * 0.5f;

    int minX = clamp_int((int)floorf(centerX - (float)maxRadius), 0, map->width - 1);
    int maxX = clamp_int((int)ceilf(centerX + (float)maxRadius), 0, map->width - 1);
    int minY = clamp_int((int)floorf(centerY - (float)maxRadius), 0, map->height - 1);
    int maxY = clamp_int((int)ceilf(centerY + (float)maxRadius), 0, map->height - 1);

    float lightIntensity = (float)type->lightLevel;
    float heatIntensity  = (float)type->warmth;

    for (int ty = minY; ty <= maxY; ++ty)
    {
        for (int tx = minX; tx <= maxX; ++tx)
        {
            float dx       = ((float)tx + 0.5f) - centerX;
            float dy       = ((float)ty + 0.5f) - centerY;
            float distance = sqrtf(dx * dx + dy * dy);

            if (lightRadius > 0 && lightIntensity > 0.0f && distance <= (float)lightRadius)
            {
                float falloff = 1.0f - (distance / (float)lightRadius);
                if (falloff < 0.0f)
                    falloff = 0.0f;
                map->lightField[ty][tx] += lightIntensity * falloff;
            }

            if (heatRadius > 0 && heatIntensity > 0.0f && distance <= (float)heatRadius)
            {
                float falloff = 1.0f - (distance / (float)heatRadius);
                if (falloff < 0.0f)
                    falloff = 0.0f;
                map->heatField[ty][tx] += heatIntensity * falloff;
            }
        }
    }
}

static void rebuild_environment_fields(Map* map)
{
    if (!map)
        return;

    environment_reset(map);

    for (int y = 0; y < map->height; ++y)
    {
        for (int x = 0; x < map->width; ++x)
        {
            Object* obj = map->objects[y][x];
            if (!obj || (int)obj->position.x != x || (int)obj->position.y != y)
                continue;
            environment_apply_object(map, obj);
        }
    }
}

static void draw_object_environment_effect(const Object* obj, Rectangle viewRect)
{
    if (!obj || !obj->type)
        return;

    const ObjectType* type = obj->type;

    if (type->activatable && !obj->isActive)
        return;

    bool hasLight = (type->lightRadius > 0 && type->lightLevel > 0);
    bool hasHeat  = (type->heatRadius > 0 && type->warmth > 0);
    if (!hasLight && !hasHeat)
        return;

    int maxRadius = type->lightRadius > type->heatRadius ? type->lightRadius : type->heatRadius;
    if (maxRadius <= 0)
        return;

    float centerX      = (obj->position.x + (float)type->width * 0.5f) * (float)TILE_SIZE;
    float centerY      = (obj->position.y + (float)type->height * 0.5f) * (float)TILE_SIZE;
    float radiusPixels = (float)maxRadius * (float)TILE_SIZE;

    Rectangle bounds = {
        .x      = centerX - radiusPixels,
        .y      = centerY - radiusPixels,
        .width  = radiusPixels * 2.0f,
        .height = radiusPixels * 2.0f,
    };

    if (!CheckCollisionRecs(viewRect, bounds))
        return;

    if (hasHeat)
    {
        float radius = (float)type->heatRadius * (float)TILE_SIZE;
        Color inner  = Fade((Color){255, 140, 60, 255}, 0.35f + fminf(0.25f, (float)type->warmth / 10.0f));
        Color outer  = Fade((Color){255, 140, 60, 255}, 0.0f);
        DrawCircleGradient((int)centerX, (int)centerY, radius, inner, outer);
    }

    if (hasLight)
    {
        float radius = (float)type->lightRadius * (float)TILE_SIZE;
        Color inner  = Fade((Color){255, 230, 150, 255}, 0.45f + fminf(0.35f, (float)type->lightLevel / 10.0f));
        Color outer  = Fade((Color){255, 230, 150, 255}, 0.0f);
        DrawCircleGradient((int)centerX, (int)centerY, radius, inner, outer);
    }
}

Rectangle object_type_frame_rect(const ObjectType* type, int frameIndex)
{
    if (!type || type->texture.id == 0)
        return (Rectangle){0.0f, 0.0f, (float)TILE_SIZE, (float)TILE_SIZE};

    int frameCount = type->spriteFrameCount > 0 ? type->spriteFrameCount : 1;
    if (frameIndex < 0)
        frameIndex = 0;
    if (frameIndex >= frameCount)
        frameIndex = frameCount - 1;

    int columns = type->spriteColumns > 0 ? type->spriteColumns : frameCount;
    if (columns <= 0)
        columns = 1;

    int rows = type->spriteRows > 0 ? type->spriteRows : ((frameCount + columns - 1) / columns);
    if (rows <= 0)
        rows = 1;

    int frameWidth  = type->spriteFrameWidth > 0 ? type->spriteFrameWidth : type->texture.width;
    int frameHeight = type->spriteFrameHeight > 0 ? type->spriteFrameHeight : type->texture.height;

    int spacingX = type->spriteSpacingX;
    int spacingY = type->spriteSpacingY;

    int col = frameIndex % columns;
    int row = frameIndex / columns;

    float srcX = (float)col * (float)(frameWidth + spacingX);
    float srcY = (float)row * (float)(frameHeight + spacingY);

    return (Rectangle){srcX, srcY, (float)frameWidth, (float)frameHeight};
}

static int object_state_frame(const ObjectType* type, bool active)
{
    if (!type)
        return 0;

    int frameCount = type->spriteFrameCount > 0 ? type->spriteFrameCount : 1;
    int frame      = active ? type->activationFrameActive : type->activationFrameInactive;

    if (frame < 0)
        frame = 0;
    if (frame >= frameCount)
        frame = frameCount - 1;
    return frame;
}

Vector2 object_frame_draw_position(const Object* obj, int frameWidth, int frameHeight)
{
    if (!obj || !obj->type)
        return (Vector2){0.0f, 0.0f};

    const ObjectType* type        = obj->type;
    float             widthTiles  = type->width > 0 ? (float)type->width : 1.0f;
    float             heightTiles = type->height > 0 ? (float)type->height : 1.0f;

    float destX;
    if (frameWidth <= 0)
        frameWidth = TILE_SIZE;

    if (frameWidth <= TILE_SIZE)
    {
        float tileLeft = obj->position.x * (float)TILE_SIZE;
        destX          = tileLeft + ((float)TILE_SIZE - (float)frameWidth) * 0.5f;
    }
    else
    {
        float baseCenterX = (obj->position.x + widthTiles * 0.5f) * (float)TILE_SIZE;
        destX             = baseCenterX - (float)frameWidth * 0.5f;
    }

    float destY;
    if (frameHeight <= 0)
        frameHeight = TILE_SIZE;

    if (frameHeight <= TILE_SIZE)
    {
        float tileTop = obj->position.y * (float)TILE_SIZE;
        destY         = tileTop + ((float)TILE_SIZE - (float)frameHeight) * 0.5f;
    }
    else
    {
        float anchorBottom = (obj->position.y + heightTiles) * (float)TILE_SIZE;
        destY              = anchorBottom - (float)frameHeight;
    }

    return (Vector2){destX, destY};
}

static int object_pick_variant_frame(const ObjectType* type, int tileX, int tileY)
{
    if (!type)
        return 0;

    int frameCount = type->spriteFrameCount > 0 ? type->spriteFrameCount : 1;
    if (frameCount <= 1)
        return 0;

    uint32_t hash = (uint32_t)(tileX * 73856093u) ^ (uint32_t)(tileY * 19349663u) ^ ((uint32_t)type->id * 83492791u);
    return (int)(hash % (uint32_t)frameCount);
}

static void object_start_animation(Object* obj)
{
    if (!obj || !obj->type || !obj->type->activatable)
        return;

    int targetFrame            = object_state_frame(obj->type, obj->isActive);
    obj->animation.targetFrame = targetFrame;

    float frameTime = obj->type->activationFrameTime;
    if (frameTime <= 0.0f || obj->animation.currentFrame == targetFrame)
    {
        obj->animation.currentFrame = targetFrame;
        obj->animation.accumulator  = 0.0f;
        obj->animation.playing      = false;
        obj->animation.forward      = true;
        obj->variantFrame           = obj->animation.currentFrame;
        return;
    }

    obj->animation.forward     = (obj->animation.currentFrame < targetFrame);
    obj->animation.playing     = true;
    obj->animation.accumulator = 0.0f;
}

static void dynamic_list_add(Object* obj)
{
    if (!obj)
        return;
    obj->nextDynamic  = G_DYNAMIC_OBJECTS;
    G_DYNAMIC_OBJECTS = obj;
}

static void dynamic_list_remove(Object* obj)
{
    if (!obj)
        return;

    if (G_DYNAMIC_OBJECTS == obj)
    {
        G_DYNAMIC_OBJECTS = obj->nextDynamic;
        obj->nextDynamic  = NULL;
        return;
    }

    Object* cursor = G_DYNAMIC_OBJECTS;
    while (cursor && cursor->nextDynamic != obj)
        cursor = cursor->nextDynamic;

    if (cursor && cursor->nextDynamic == obj)
    {
        cursor->nextDynamic = obj->nextDynamic;
        obj->nextDynamic    = NULL;
    }
}

static void finalize_sprite_info(ObjectType* type)
{
    if (!type)
        return;

    if (type->texture.id == 0)
    {
        if (type->spriteFrameWidth <= 0)
            type->spriteFrameWidth = TILE_SIZE;
        if (type->spriteFrameHeight <= 0)
            type->spriteFrameHeight = TILE_SIZE;
        if (type->spriteFrameCount <= 0)
            type->spriteFrameCount = 1;
        if (type->activationFrameInactive < 0)
            type->activationFrameInactive = 0;
        if (type->activationFrameActive < 0)
            type->activationFrameActive = type->activationFrameInactive;
        return;
    }

    if (type->spriteColumns <= 0 && type->spriteFrameWidth > 0)
    {
        int step = type->spriteFrameWidth + type->spriteSpacingX;
        if (step > 0)
            type->spriteColumns = (type->texture.width + type->spriteSpacingX) / step;
    }

    if (type->spriteColumns <= 0)
        type->spriteColumns = 1;

    if (type->spriteRows <= 0 && type->spriteFrameHeight > 0)
    {
        int step = type->spriteFrameHeight + type->spriteSpacingY;
        if (step > 0)
            type->spriteRows = (type->texture.height + type->spriteSpacingY) / step;
    }

    if (type->spriteRows <= 0)
        type->spriteRows = 1;

    if (type->spriteFrameWidth <= 0)
    {
        int totalSpacing       = (type->spriteColumns - 1) * type->spriteSpacingX;
        type->spriteFrameWidth = (type->texture.width - totalSpacing) / (type->spriteColumns > 0 ? type->spriteColumns : 1);
    }

    if (type->spriteFrameHeight <= 0)
    {
        int totalSpacing        = (type->spriteRows - 1) * type->spriteSpacingY;
        type->spriteFrameHeight = (type->texture.height - totalSpacing) / (type->spriteRows > 0 ? type->spriteRows : 1);
    }

    if (type->spriteFrameWidth <= 0)
        type->spriteFrameWidth = type->texture.width;
    if (type->spriteFrameHeight <= 0)
        type->spriteFrameHeight = type->texture.height;

    if (type->spriteFrameCount <= 0)
        type->spriteFrameCount = type->spriteColumns * type->spriteRows;

    if (type->spriteFrameCount <= 0)
        type->spriteFrameCount = 1;

    if (type->activationFrameInactive < 0)
        type->activationFrameInactive = 0;
    if (type->activationFrameInactive >= type->spriteFrameCount)
        type->activationFrameInactive = type->spriteFrameCount - 1;

    if (type->activationFrameActive < 0)
        type->activationFrameActive = type->activationFrameInactive;
    if (type->activationFrameActive >= type->spriteFrameCount)
        type->activationFrameActive = type->spriteFrameCount - 1;

    if (type->activationFrameTime <= 0.0f)
        type->activationFrameTime = 0.12f;
}

void init_objects(void)
{
    G_DYNAMIC_OBJECTS = NULL;
    int objCount      = load_objects_from_stv("data/objects.stv", G_OBJECT_TYPES, OBJ_COUNT);

    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            G_OBJECT_TYPES[i].texture = LoadTexture(G_OBJECT_TYPES[i].texturePath);
        finalize_sprite_info(&G_OBJECT_TYPES[i]);
    }
    debug_print_objects(G_OBJECT_TYPES, OBJ_COUNT);
}

void unload_object_textures(void)
{
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            UnloadTexture(G_OBJECT_TYPES[i].texture);
    }
    G_DYNAMIC_OBJECTS = NULL;
}

const ObjectType* get_object_type(ObjectTypeID id)
{
    // Sanity check
    if (id <= OBJ_NONE)
        return &G_OBJECT_TYPES[0]; // fallback on [OBJ_NONE]

    // Linear search (IDs are not guaranteed to be sequential anymore)
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].id == id)
            return &G_OBJECT_TYPES[i];
    }

    // If not found, fallback
    return &G_OBJECT_TYPES[0];
}

ObjectTypeID object_type_id_from_name(const char* name)
{
    if (!name)
        return OBJ_NONE;

    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].name && strcmp(G_OBJECT_TYPES[i].name, name) == 0)
            return G_OBJECT_TYPES[i].id;
    }

    return OBJ_NONE;
}

// analyse all structures (with objects and size)
const StructureDef* analyze_building_type(const Building* b)
{
    printf("\n[ANALYZE] Analyzing Building (Area: %d, Object Count: %d)\n", b->area, b->objectCount);
    for (StructureKind kind = 0; kind < STRUCT_COUNT; ++kind)
    {
        const StructureDef* def = get_structure_def(kind);
        if (!def)
            continue;

        if (def->roomId == ROOM_NONE)
            continue;

        if (b->area < def->minArea)
            continue;
        if (def->maxArea && b->area > def->maxArea)
            continue;

        bool valid = true;
        for (int j = 0; j < def->requirementCount; j++)
        {
            const ObjectRequirement* req    = &def->requirements[j];
            int                      count  = 0;
            const ObjectType*        reqObj = get_object_type(req->objectId);
            printf("[ANALYZE] Checking requirement: %s, min: %d\n", reqObj ? reqObj->name : "(unknown)", req->minCount);
            for (int k = 0; k < b->objectCount; k++)
            {
                if (b->objects[k]->type->id == req->objectId)
                    count++;
            }

            if (count < req->minCount)
            {
                valid = false;
                break;
            }
        }

        if (valid)
            return def;
    }
    return NULL;
}

Object* create_object(ObjectTypeID id, int x, int y)
{
    Object*           obj  = malloc(sizeof(Object));
    const ObjectType* type = get_object_type(id);
    if (!obj || !type)
    {
        free(obj);
        return NULL;
    }

    obj->type     = type;
    obj->position = (Vector2){(float)x, (float)y};
    obj->hp       = type->maxHP;
    obj->isActive = type->activatable ? type->activationDefaultActive : true;

    obj->animation.currentFrame = object_state_frame(type, obj->isActive);
    obj->animation.targetFrame  = obj->animation.currentFrame;
    obj->animation.accumulator  = 0.0f;
    obj->animation.playing      = false;
    obj->animation.forward      = true;
    obj->variantFrame           = type->activatable ? obj->animation.currentFrame : object_pick_variant_frame(type, x, y);
    obj->nextDynamic            = NULL;

    if (object_type_is_dynamic(type))
        dynamic_list_add(obj);

    G_ENVIRONMENT_DIRTY = true;
    return obj;
}

void object_destroy(Object* obj)
{
    if (!obj)
        return;

    if (object_type_is_dynamic(obj->type))
        dynamic_list_remove(obj);

    G_ENVIRONMENT_DIRTY = true;
    free(obj);
}

bool object_has_activation(const Object* obj)
{
    return obj && obj->type && obj->type->activatable;
}

bool object_set_active(Object* obj, bool active)
{
    if (!object_has_activation(obj))
        return false;

    if (obj->isActive == active)
        return false;

    obj->isActive = active;
    object_start_animation(obj);
    G_ENVIRONMENT_DIRTY = true;
    return true;
}

bool object_toggle(Object* obj)
{
    if (!object_has_activation(obj))
        return false;
    return object_set_active(obj, !obj->isActive);
}

bool object_is_walkable(const Object* obj)
{
    if (!obj || !obj->type)
        return true;

    if (!obj->type->activatable)
        return obj->type->walkable;

    return obj->isActive ? obj->type->activationWalkableOn : obj->type->activationWalkableOff;
}

int object_static_frame(const Object* obj)
{
    if (!obj || !obj->type)
        return 0;

    if (obj->type->activatable)
    {
        int frame = obj->animation.playing ? obj->animation.currentFrame : object_state_frame(obj->type, obj->isActive);
        if (frame < 0)
            frame = 0;
        int frameCount = obj->type->spriteFrameCount > 0 ? obj->type->spriteFrameCount : 1;
        if (frame >= frameCount)
            frame = frameCount - 1;
        return frame;
    }

    int frameCount = obj->type->spriteFrameCount > 0 ? obj->type->spriteFrameCount : 1;
    if (frameCount <= 1)
        return 0;

    int frame = obj->variantFrame;
    if (frame < 0)
        frame = 0;
    if (frame >= frameCount)
        frame = frameCount - 1;
    return frame;
}

void object_update_system(Map* map, float dt)
{
    if (dt <= 0.0f)
        dt = 0.0f;

    for (Object* obj = G_DYNAMIC_OBJECTS; obj; obj = obj->nextDynamic)
    {
        if (!obj->animation.playing || !obj->type || obj->type->activationFrameTime <= 0.0f)
        {
            if (obj->animation.playing)
            {
                obj->animation.currentFrame = obj->animation.targetFrame;
                obj->animation.playing      = false;
                obj->animation.accumulator  = 0.0f;
                obj->variantFrame           = obj->animation.currentFrame;
            }
            continue;
        }

        obj->animation.accumulator += dt;
        float frameTime = obj->type->activationFrameTime;

        while (obj->animation.accumulator >= frameTime)
        {
            obj->animation.accumulator -= frameTime;

            if (obj->animation.forward)
                obj->animation.currentFrame++;
            else
                obj->animation.currentFrame--;

            if (obj->animation.currentFrame == obj->animation.targetFrame)
            {
                obj->animation.playing     = false;
                obj->animation.accumulator = 0.0f;
                obj->variantFrame          = obj->animation.currentFrame;
                break;
            }

            int maxFrame = obj->type->spriteFrameCount > 0 ? obj->type->spriteFrameCount - 1 : 0;
            if (obj->animation.currentFrame < 0)
                obj->animation.currentFrame = 0;
            if (obj->animation.currentFrame > maxFrame)
                obj->animation.currentFrame = maxFrame;

            obj->variantFrame = obj->animation.currentFrame;
        }
    }

    if (map && G_ENVIRONMENT_DIRTY)
    {
        rebuild_environment_fields(map);
        G_ENVIRONMENT_DIRTY = false;
    }
}

void object_draw_dynamic(const Map* map, const Camera2D* camera)
{
    (void)map;
    if (!camera)
        return;

    float     invZoom = 1.0f / camera->zoom;
    Rectangle view    = {
           .x      = camera->target.x - camera->offset.x * invZoom,
           .y      = camera->target.y - camera->offset.y * invZoom,
           .width  = GetScreenWidth() * invZoom,
           .height = GetScreenHeight() * invZoom,
    };

    for (Object* obj = G_DYNAMIC_OBJECTS; obj; obj = obj->nextDynamic)
    {
        if (!obj->type)
            continue;

        if (obj->type->texture.id != 0)
        {
            Rectangle src     = object_type_frame_rect(obj->type, obj->animation.currentFrame);
            Vector2   drawPos = object_frame_draw_position(obj, (int)src.width, (int)src.height);
            Rectangle bounds  = {.x = drawPos.x, .y = drawPos.y, .width = src.width, .height = src.height};

            if (!CheckCollisionRecs(view, bounds))
                continue;

            DrawTextureRec(obj->type->texture, src, drawPos, WHITE);
        }
        else
        {
            Vector2   drawPos = object_frame_draw_position(obj, TILE_SIZE, TILE_SIZE);
            Rectangle bounds  = {.x = drawPos.x, .y = drawPos.y, .width = (float)TILE_SIZE, .height = (float)TILE_SIZE};

            if (!CheckCollisionRecs(view, bounds))
                continue;

            DrawRectangle(drawPos.x + 2.0f, drawPos.y + 2.0f, bounds.width - 4.0f, bounds.height - 4.0f, obj->type->color);
        }
    }
}

void object_draw_environment(const Map* map, const Camera2D* camera)
{
    if (!map || !camera)
        return;

    Rectangle view = {.x      = camera->target.x - (GetScreenWidth() / 2) / camera->zoom,
                      .y      = camera->target.y - (GetScreenHeight() / 2) / camera->zoom,
                      .width  = GetScreenWidth() / camera->zoom,
                      .height = GetScreenHeight() / camera->zoom};

    float     invZoom   = 1.0f / camera->zoom;
    Rectangle pixelView = {
        .x      = camera->target.x - camera->offset.x * invZoom,
        .y      = camera->target.y - camera->offset.y * invZoom,
        .width  = GetScreenWidth() * invZoom,
        .height = GetScreenHeight() * invZoom,
    };

    int startX = (int)(view.x / TILE_SIZE) - 1;
    int startY = (int)(view.y / TILE_SIZE) - 1;
    int endX   = (int)((view.x + view.width) / TILE_SIZE) + 1;
    int endY   = (int)((view.y + view.height) / TILE_SIZE) + 1;

    BeginBlendMode(BLEND_ADDITIVE);
    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            int wx = (x % map->width + map->width) % map->width;
            int wy = (y % map->height + map->height) % map->height;

            Object* obj = map->objects[wy][wx];
            if (!obj || !obj->type)
                continue;
            if ((int)obj->position.x != wx || (int)obj->position.y != wy)
                continue;

            draw_object_environment_effect(obj, pixelView);
        }
    }
    EndBlendMode();
}

void draw_objects(Map* map, Camera2D* camera)
{
    if (!map || !camera)
        return;

    Rectangle view = {.x      = camera->target.x - (GetScreenWidth() / 2) / camera->zoom,
                      .y      = camera->target.y - (GetScreenHeight() / 2) / camera->zoom,
                      .width  = GetScreenWidth() / camera->zoom,
                      .height = GetScreenHeight() / camera->zoom};

    int startX = (int)(view.x / TILE_SIZE) - 1;
    int startY = (int)(view.y / TILE_SIZE) - 1;
    int endX   = (int)((view.x + view.width) / TILE_SIZE) + 1;
    int endY   = (int)((view.y + view.height) / TILE_SIZE) + 1;

    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            int wx = (x % map->width + map->width) % map->width;
            int wy = (y % map->height + map->height) % map->height;

            Object* obj = map->objects[wy][wx];
            if (!obj)
                continue;

            const ObjectType* type = obj->type;
            if (!type)
                continue;

            if (object_type_is_dynamic(type))
                continue; // drawn separately

            // --- If object had a texture ---
            if (type->texture.id != 0)
            {
                Rectangle src     = object_type_frame_rect(type, object_static_frame(obj));
                Vector2   drawPos = object_frame_draw_position(obj, (int)src.width, (int)src.height);
                DrawTextureRec(type->texture, src, drawPos, WHITE);
            }
            else
            {
                // --- otherwise colored rectangle ---
                Vector2 drawPos = object_frame_draw_position(obj, TILE_SIZE, TILE_SIZE);
                float   size    = TILE_SIZE * 0.6f; // plus petit que la tuile
                float   offsetX = ((float)TILE_SIZE - size) * 0.5f;
                float   offsetY = ((float)TILE_SIZE - size) * 0.5f;
                DrawRectangle(drawPos.x + offsetX, drawPos.y + offsetY, size, size, type->color);
            }
        }
    }
}

bool is_wall_object(const Object* o)
{
    if (!o)
    {
        return false;
    }
    return o->type->isWall;
}

bool is_door_object(const Object* o)
{
    if (!o)
    {
        return false;
    }
    return o->type->isDoor;
}

bool is_blocking_object(const Object* o)
{
    if (!o)
        return false;
    if (is_door_object(o))
        return false; // the door does not "block" the fill (it counts as a border)
    if (is_wall_object(o))
        return true; // mur = blocking
    return !object_is_walkable(o);
}
