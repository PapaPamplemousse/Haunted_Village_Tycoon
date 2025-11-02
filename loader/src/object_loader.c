/**
 * @file object_loader.c
 * @brief Deserializes object type definitions from STV files.
 */

#include "object_loader.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

/**
 * @brief Removes leading and trailing whitespace from a string.
 */
static void trim(char* s)
{
    if (!s)
        return;
    char* start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

/**
 * @brief Local strdup helper to avoid portability issues.
 */
static char* str_dup(const char* s)
{
    if (!s)
        return NULL;
    size_t len  = strlen(s) + 1;
    char*  copy = malloc(len);
    if (copy)
        memcpy(copy, s, len);
    return copy;
}

static void finalize_object_definition(ObjectType* obj,
                                       int          walkableOnRaw,
                                       int          walkableOffRaw,
                                       int          frameInactiveRaw,
                                       int          frameActiveRaw,
                                       float        frameTimeRaw)
{
    if (!obj)
        return;

    if (walkableOnRaw == -1)
        obj->activationWalkableOn = obj->walkable;
    else
        obj->activationWalkableOn = (walkableOnRaw != 0);

    if (walkableOffRaw == -1)
        obj->activationWalkableOff = obj->walkable;
    else
        obj->activationWalkableOff = (walkableOffRaw != 0);

    if (frameInactiveRaw >= 0)
        obj->activationFrameInactive = frameInactiveRaw;
    else if (obj->activationFrameInactive < 0)
        obj->activationFrameInactive = 0;

    if (frameActiveRaw >= 0)
        obj->activationFrameActive = frameActiveRaw;
    else if (obj->activationFrameActive < 0)
        obj->activationFrameActive = obj->activationFrameInactive;

    if (frameTimeRaw > 0.0f)
        obj->activationFrameTime = frameTimeRaw;

    if (obj->activationFrameTime <= 0.0f)
        obj->activationFrameTime = 0.12f;

    if (!obj->activatable)
    {
        obj->activationDefaultActive = true;
        obj->activationWalkableOn    = obj->walkable;
        obj->activationWalkableOff   = obj->walkable;
    }
}

void debug_print_objects(const ObjectType* objects, int count)
{
    TraceLog(LOG_INFO, "=== OBJECT TABLE CHECK (%d entries) ===", count);
    for (int i = 0; i < count; i++)
    {
        const ObjectType* o = &objects[i];
        TraceLog(LOG_INFO, "[%02d] %-16s  ID=%-3d  Cat=%-10s  Tex=%p  Path=%s", i, o->name ? o->name : "(null)", o->id, o->category ? o->category : "(null)", (void*)o->texture.id,
                 o->texturePath ? o->texturePath : "(null)");
    }
}

/**
 * @brief Parses an RGBA color in "r,g,b,a" format.
 */
static bool parse_color(const char* value, Color* out)
{
    int r, g, b, a;
    if (sscanf(value, "%d,%d,%d,%d", &r, &g, &b, &a) == 4)
    {
        *out = (Color){r, g, b, a};
        return true;
    }
    return false;
}

int load_objects_from_stv(const char* path, ObjectType* outArray, int maxObjects)
{
    FILE* f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "❌ Cannot open %s\n", path);
        return 0;
    }

    char       line[512];
    ObjectType current   = (ObjectType){0};
    int        count     = 0;
    bool       inSection = false;
    int        walkableOnRaw        = -1;
    int        walkableOffRaw       = -1;
    int        frameInactiveRaw     = -1;
    int        frameActiveRaw       = -1;
    float      frameTimeRaw         = -1.0f;

    while (fgets(line, sizeof(line), f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (line[0] == '[')
        {
            if (inSection && count < maxObjects)
            {
                finalize_object_definition(&current, walkableOnRaw, walkableOffRaw, frameInactiveRaw, frameActiveRaw, frameTimeRaw);
                outArray[count++] = current;
            }

            memset(&current, 0, sizeof(ObjectType));
            current.activationFrameTime     = 0.12f;
            current.activationDefaultActive = true;
            current.activationFrameInactive = -1;
            current.activationFrameActive   = -1;

            walkableOnRaw    = -1;
            walkableOffRaw   = -1;
            frameInactiveRaw = -1;
            frameActiveRaw   = -1;
            frameTimeRaw     = -1.0f;

            inSection = true;
            continue;
        }

        char key[64], value[256];
        if (sscanf(line, "%63[^=]=%255[^\n]", key, value) == 2)
        {
            trim(key);
            trim(value);

            if (strcmp(key, "id") == 0)
                current.id = atoi(value);
            else if (strcmp(key, "name") == 0)
                current.name = str_dup(value);
            else if (strcmp(key, "display_name") == 0)
                current.displayName = str_dup(value);
            else if (strcmp(key, "category") == 0)
                current.category = str_dup(value);
            else if (strcmp(key, "max_hp") == 0)
                current.maxHP = atoi(value);
            else if (strcmp(key, "comfort") == 0)
                current.comfort = atoi(value);
            else if (strcmp(key, "warmth") == 0)
                current.warmth = atoi(value);
            else if (strcmp(key, "light") == 0)
                current.lightLevel = atoi(value);
            else if (strcmp(key, "width") == 0)
                current.width = atoi(value);
            else if (strcmp(key, "height") == 0)
                current.height = atoi(value);
            else if (strcmp(key, "walkable") == 0)
            {
                current.walkable = (strcmp(value, "true") == 0);
                if (walkableOnRaw == -1)
                    current.activationWalkableOn = current.walkable;
                if (walkableOffRaw == -1)
                    current.activationWalkableOff = current.walkable;
            }
            else if (strcmp(key, "flammable") == 0)
                current.flammable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "is_wall") == 0)
                current.isWall = (strcmp(value, "true") == 0);
            else if (strcmp(key, "is_door") == 0)
                current.isDoor = (strcmp(value, "true") == 0);
            else if (strcmp(key, "activatable") == 0)
                current.activatable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "activation_default") == 0 || strcmp(key, "activation_default_active") == 0 || strcmp(key, "activation_default_state") == 0)
                current.activationDefaultActive = (strcmp(value, "true") == 0);
            else if (strcmp(key, "activation_walkable_active") == 0 || strcmp(key, "activation_walkable_on") == 0)
            {
                walkableOnRaw                = (strcmp(value, "true") == 0) ? 1 : 0;
                current.activationWalkableOn = (walkableOnRaw != 0);
            }
            else if (strcmp(key, "activation_walkable_inactive") == 0 || strcmp(key, "activation_walkable_off") == 0)
            {
                walkableOffRaw                = (strcmp(value, "true") == 0) ? 1 : 0;
                current.activationWalkableOff = (walkableOffRaw != 0);
            }
            else if (strcmp(key, "sprite_frame_width") == 0 || strcmp(key, "frame_width") == 0)
                current.spriteFrameWidth = atoi(value);
            else if (strcmp(key, "sprite_frame_height") == 0 || strcmp(key, "frame_height") == 0)
                current.spriteFrameHeight = atoi(value);
            else if (strcmp(key, "sprite_columns") == 0 || strcmp(key, "frames_per_row") == 0)
                current.spriteColumns = atoi(value);
            else if (strcmp(key, "sprite_rows") == 0 || strcmp(key, "frames_per_column") == 0)
                current.spriteRows = atoi(value);
            else if (strcmp(key, "sprite_frame_count") == 0 || strcmp(key, "frame_count") == 0)
                current.spriteFrameCount = atoi(value);
            else if (strcmp(key, "sprite_spacing_x") == 0 || strcmp(key, "frame_spacing_x") == 0)
                current.spriteSpacingX = atoi(value);
            else if (strcmp(key, "sprite_spacing_y") == 0 || strcmp(key, "frame_spacing_y") == 0)
                current.spriteSpacingY = atoi(value);
            else if (strcmp(key, "activation_frame_time") == 0 || strcmp(key, "animation_frame_time") == 0 || strcmp(key, "activation_animation_ms") == 0)
                frameTimeRaw = (float)atof(value);
            else if (strcmp(key, "activation_frame_inactive") == 0 || strcmp(key, "activation_frame_start") == 0 || strcmp(key, "inactive_frame") == 0)
            {
                int idx           = atoi(value);
                if (idx > 0)
                    idx -= 1;
                frameInactiveRaw             = idx;
                current.activationFrameInactive = (idx < 0) ? 0 : idx;
            }
            else if (strcmp(key, "activation_frame_active") == 0 || strcmp(key, "activation_frame_end") == 0 || strcmp(key, "active_frame") == 0)
            {
                int idx         = atoi(value);
                if (idx > 0)
                    idx -= 1;
                frameActiveRaw            = idx;
                current.activationFrameActive = (idx < 0) ? 0 : idx;
            }
            else if (strcmp(key, "color") == 0)
                parse_color(value, &current.color);
            else if (strcmp(key, "texture") == 0)
            {
                trim(value);

                if (value[0] == '"' && value[strlen(value) - 1] == '"')
                {
                    value[strlen(value) - 1] = '\0';
                    memmove(value, value + 1, strlen(value));
                }

                current.texturePath = str_dup(value);
            }
        }
    }

    if (inSection && count < maxObjects)
    {
        finalize_object_definition(&current, walkableOnRaw, walkableOffRaw, frameInactiveRaw, frameActiveRaw, frameTimeRaw);
        outArray[count++] = current;
    }

    fclose(f);
    return count;
}
