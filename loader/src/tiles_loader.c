#include "tiles_loader.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
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

static TileCategory parse_tile_category(const char* s)
{
    if (strcmp(s, "ground") == 0)
        return TILE_CATEGORY_GROUND;
    if (strcmp(s, "water") == 0)
        return TILE_CATEGORY_WATER;
    if (strcmp(s, "hazard") == 0)
        return TILE_CATEGORY_HAZARD;
    if (strcmp(s, "obstacle") == 0)
        return TILE_CATEGORY_OBSTACLE;
    return TILE_CATEGORY_GROUND;
}

int load_tiles_from_stv(const char* path, TileType* outArray, int maxTiles)
{
    FILE* f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "❌ Cannot open tile STV: %s\n", path);
        return 0;
    }

    char     line[512];
    TileType current   = {0};
    int      count     = 0;
    bool     inSection = false;

    while (fgets(line, sizeof(line), f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (line[0] == '[')
        {
            if (inSection && count < maxTiles)
                outArray[count++] = current;
            memset(&current, 0, sizeof(TileType));
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
            else if (strcmp(key, "category") == 0)
                current.category = parse_tile_category(value);
            else if (strcmp(key, "walkable") == 0)
                current.walkable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "is_breakable") == 0)
                current.isBreakable = (strcmp(value, "true") == 0);
            else if (strcmp(key, "durability") == 0)
                current.durability = atoi(value);
            else if (strcmp(key, "movement") == 0)
                current.movementCost = atof(value);
            else if (strcmp(key, "humidity") == 0)
                current.humidity = atof(value);
            else if (strcmp(key, "fertility") == 0)
                current.fertility = atof(value);
            else if (strcmp(key, "temperature") == 0)
                current.temperature = atof(value);
            else if (strcmp(key, "texture") == 0)
                current.texturePath = str_dup(value);
            else if (strcmp(key, "color") == 0)
            {
                int r, g, b, a;
                if (sscanf(value, "%d,%d,%d,%d", &r, &g, &b, &a) == 4)
                    current.color = (Color){r, g, b, a};
            }
        }
    }

    if (inSection && count < maxTiles)
        outArray[count++] = current;

    fclose(f);
    return count;
}
