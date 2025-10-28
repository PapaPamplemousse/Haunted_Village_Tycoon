/**
 * @file object_loader.c
 * @brief Deserializes object and room type definitions from STV files.
 */

#include "object_loader.h"
#include <stdio.h>
#include <string.h>
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

void debug_print_rooms(const RoomTypeRule* rooms, int roomCount, const ObjectType* objects, int objectCount)
{
    TraceLog(LOG_INFO, "=== ROOM TYPE RULES CHECK (%d entries) ===", roomCount);

    for (int i = 0; i < roomCount; i++)
    {
        const RoomTypeRule* r = &rooms[i];
        TraceLog(LOG_INFO, "[%02d] %-12s  ID=%d  Area=[%d..%d]  ReqCount=%d", i, r->name ? r->name : "(null)", r->id, r->minArea, r->maxArea, r->requirementCount);

        if (r->requirementCount > 0 && r->requirements)
        {
            for (int j = 0; j < r->requirementCount; j++)
            {
                ObjectTypeID      oid = r->requirements[j].objectId;
                const ObjectType* o   = NULL;
                for (int k = 0; k < objectCount; k++)
                    if (objects[k].id == oid)
                        o = &objects[k];

                TraceLog(LOG_INFO, "      - %-16s  (ID=%d, Min=%d)", o ? o->name : "(unknown)", oid, r->requirements[j].minCount);
            }
        }
    }
}

const ObjectType* find_object_by_name(const ObjectType* objects, int count, const char* name)
{
    for (int i = 0; i < count; i++)
    {
        if (objects[i].name && strcmp(objects[i].name, name) == 0)
            return &objects[i];
    }
    return NULL;
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

typedef struct RoomTypeEntry
{
    const char* name;
    RoomTypeID  id;
} RoomTypeEntry;

// Table de correspondance nom → enum
static const RoomTypeEntry ROOM_TYPE_NAMES[] = {{"Bedroom", ROOM_BEDROOM},
                                               {"Kitchen", ROOM_KITCHEN},
                                               {"Hut", ROOM_HUT},
                                               {"Crypt", ROOM_CRYPT},
                                               {"Sanctuary", ROOM_SANCTUARY},
                                               {"House", ROOM_HOUSE},
                                               {"LargeRoom", ROOM_LARGEROOM},
                                               {"Ruin", ROOM_RUIN},
                                               {"Witch Hovel", ROOM_WITCH_HOVEL},
                                               {"Gallows", ROOM_GALLOWS},
                                               {"Blood Garden", ROOM_BLOOD_GARDEN},
                                               {"Flesh Pit", ROOM_FLESH_PIT},
                                               {"Void Obelisk", ROOM_VOID_OBELISK},
                                               {"Plague Nursery", ROOM_PLAGUE_NURSERY},
                                               {NULL, ROOM_NONE}}; // fin de table

// Lookup de l’ID à partir du nom
/**
 * @brief Translates a room name in the STV file to the corresponding enum value.
 */
static RoomTypeID get_room_id_by_name(const char* name)
{
    for (int i = 0; ROOM_TYPE_NAMES[i].name; ++i)
    {
        if (strcmp(ROOM_TYPE_NAMES[i].name, name) == 0)
            return ROOM_TYPE_NAMES[i].id;
    }
    return ROOM_NONE;
}

int load_rooms_from_stv(const char* path, RoomTypeRule* outArray, int maxRooms, const ObjectType* objects, int objectCount)
{
    FILE* f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "❌ Cannot open %s\n", path);
        return 0;
    }

    char         line[512];
    RoomTypeRule current   = {0};
    int          count     = 0;
    bool         inSection = false;
    char         reqBuffer[512];

    while (fgets(line, sizeof(line), f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        // Nouvelle section [RoomName]
        if (line[0] == '[')
        {
            // On stocke la précédente
            if (inSection && count < maxRooms)
                outArray[count++] = current;

            memset(&current, 0, sizeof(RoomTypeRule));
            inSection = true;

            // Extraire le nom de la section
            char sectionName[64];
            sscanf(line, "[%63[^]]", sectionName);
            trim(sectionName);

            // Définir le nom et l’ID
            current.name = str_dup(sectionName);
            current.id   = get_room_id_by_name(sectionName);
            continue;
        }

        char key[64], value[256];
        if (sscanf(line, "%63[^=]=%255[^\n]", key, value) == 2)
        {
            trim(key);
            trim(value);

            if (strcmp(key, "min_area") == 0)
                current.minArea = atoi(value);
            else if (strcmp(key, "max_area") == 0)
                current.maxArea = atoi(value);
            else if (strcmp(key, "requirement") == 0)
            {
                strncpy(reqBuffer, value, sizeof(reqBuffer) - 1);
                reqBuffer[sizeof(reqBuffer) - 1] = '\0';

                // Compter les requirements
                int reqCount = 1;
                for (char* p = reqBuffer; *p; ++p)
                    if (*p == ',')
                        reqCount++;

                ObjectRequirement* reqs = malloc(sizeof(ObjectRequirement) * reqCount);
                int                idx  = 0;

                char* token = strtok(reqBuffer, ",");
                while (token && idx < reqCount)
                {
                    trim(token);
                    char objName[64];
                    int  minCount = 0;
                    if (sscanf(token, "%63[^:]:%d", objName, &minCount) == 2)
                    {
                        const ObjectType* obj = find_object_by_name(objects, objectCount, objName);
                        if (obj)
                        {
                            reqs[idx].objectId = obj->id;
                            reqs[idx].minCount = minCount;
                            idx++;
                        }
                        else
                        {
                            fprintf(stderr, "⚠️  Unknown object in room '%s': '%s'\n", current.name ? current.name : "?", objName);
                        }
                    }
                    token = strtok(NULL, ",");
                }

                current.requirements     = reqs;
                current.requirementCount = idx;
            }
            else if (strcmp(key, "id") == 0)
            {
                // Optionnel si tu veux forcer un ID spécifique
                current.id = (RoomTypeID)atoi(value);
            }
        }
    }

    if (inSection && count < maxRooms)
        outArray[count++] = current;

    fclose(f);
    return count;
}
