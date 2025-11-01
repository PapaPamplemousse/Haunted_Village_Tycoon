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
    ObjectType current   = {0};
    int        count     = 0;
    bool       inSection = false;

    while (fgets(line, sizeof(line), f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (line[0] == '[')
        {
            if (inSection && count < maxObjects)
                outArray[count++] = current;
            memset(&current, 0, sizeof(ObjectType));
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
                current.walkable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "flammable") == 0)
                current.flammable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "is_wall") == 0)
                current.isWall = (strcmp(value, "true") == 0);
            else if (strcmp(key, "is_door") == 0)
                current.isDoor = (strcmp(value, "true") == 0);
            else if (strcmp(key, "color") == 0)
                parse_color(value, &current.color);
            else if (strcmp(key, "texture") == 0)
            {
                trim(value);

                // Retire les guillemets si présents
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
        outArray[count++] = current;

    fclose(f);
    return count;
}

