/**
 * @file music_loader.c
 * @brief Parses music metadata from .stv configuration files.
 */

#include "music_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/**
 * @brief Local strdup helper to remain portable across C runtimes.
 */
static char* str_dup(const char* s)
{
    if (!s)
        return NULL;
    size_t len  = strlen(s) + 1;
    char*  copy = (char*)malloc(len);
    if (copy)
        memcpy(copy, s, len);
    return copy;
}

/**
 * @brief Removes leading and trailing whitespace from a mutable string.
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
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

/**
 * @brief Parses a textual boolean representation.
 */
static bool parse_bool(const char* value, bool defaultValue)
{
    if (!value)
        return defaultValue;

    if (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0)
        return true;
    if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0)
        return false;

    char* endPtr = NULL;
    long  v      = strtol(value, &endPtr, 10);
    if (endPtr && *endPtr == '\0')
        return (v != 0);
    return defaultValue;
}

/**
 * @brief Assigns a freshly duplicated string to a target pointer.
 */
static void assign_string(char** target, const char* value)
{
    if (!target || !value)
        return;
    char* copy = str_dup(value);
    if (!copy)
        return;
    free(*target);
    *target = copy;
}

/**
 * @brief Converts a textual usage category to the corresponding enum.
 */
static MusicUsage parse_usage(const char* value)
{
    if (!value)
        return MUSIC_USAGE_UNKNOWN;

    if (strcasecmp(value, "gameplay") == 0)
        return MUSIC_USAGE_GAMEPLAY;
    if (strcasecmp(value, "event") == 0)
        return MUSIC_USAGE_EVENT;
    if (strcasecmp(value, "ambient") == 0)
        return MUSIC_USAGE_AMBIENT;
    if (strcasecmp(value, "menu") == 0)
        return MUSIC_USAGE_MENU;
    return MUSIC_USAGE_UNKNOWN;
}

/**
 * @brief Applies default values to a definition structure.
 */
static void set_defaults(MusicDefinition* def)
{
    if (!def)
        return;
    memset(def, 0, sizeof(*def));
    def->id             = -1;
    def->usage          = MUSIC_USAGE_UNKNOWN;
    def->loop           = true;
    def->loopCount      = -1;
    def->defaultVolume  = 1.0f;
    def->defaultFadeIn  = 1.0f;
    def->defaultFadeOut = 1.0f;
    def->crossfadeLead  = 4.0f;
    def->cueOffset      = 0.0f;
}

/**
 * @brief Releases dynamic members inside a definition.
 */
static void free_definition(MusicDefinition* def)
{
    if (!def)
        return;
    free(def->name);
    free(def->filePath);
    free(def->group);
    free(def->eventName);
    def->name      = NULL;
    def->filePath  = NULL;
    def->group     = NULL;
    def->eventName = NULL;
}

/**
 * @brief Adds the current definition to the dynamic array if valid.
 */
static void finalize_definition(MusicDefinition* current,
                                MusicDefinition** array,
                                int*              count,
                                int*              capacity)
{
    if (!current || !array || !count || !capacity)
        return;

    if (!current->filePath)
    {
        fprintf(stderr, "Warning: Skipping a music entry without file path.\n");
        free_definition(current);
        return;
    }

    if (*count >= *capacity)
    {
        int newCapacity = (*capacity == 0) ? 8 : (*capacity * 2);
        MusicDefinition* resized = (MusicDefinition*)realloc(*array, sizeof(MusicDefinition) * (size_t)newCapacity);
        if (!resized)
        {
            fprintf(stderr, "❌ Failed to expand music definition array.\n");
            free_definition(current);
            return;
        }
        *array    = resized;
        *capacity = newCapacity;
    }

    (*array)[*count] = *current;
    (*count)++;

    MusicDefinition blank;
    set_defaults(&blank);
    *current = blank;
}

MusicDefinition* music_loader_load(const char* path, int* outCount)
{
    if (outCount)
        *outCount = 0;

    if (!path)
        return NULL;

    FILE* f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "❌ Unable to open music definition file: %s\n", path);
        return NULL;
    }

    MusicDefinition* defs     = NULL;
    int              count    = 0;
    int              capacity = 0;
    MusicDefinition  current;
    set_defaults(&current);
    bool inSection = false;
    char line[512];
    int  lineNumber = 0;

    while (fgets(line, sizeof(line), f))
    {
        lineNumber++;
        trim(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        if (line[0] == '[')
        {
            if (inSection)
                finalize_definition(&current, &defs, &count, &capacity);
            set_defaults(&current);
            inSection = true;
            continue;
        }

        if (!inSection)
            continue;

        char key[128];
        char value[384];
        if (sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2)
        {
            fprintf(stderr, "Warning: Invalid line in music file (%s:%d): %s\n", path, lineNumber, line);
            continue;
        }

        trim(key);
        trim(value);

        if (strcasecmp(key, "id") == 0)
            current.id = atoi(value);
        else if (strcasecmp(key, "name") == 0)
            assign_string(&current.name, value);
        else if (strcasecmp(key, "file") == 0 || strcasecmp(key, "path") == 0)
            assign_string(&current.filePath, value);
        else if (strcasecmp(key, "group") == 0)
            assign_string(&current.group, value);
        else if (strcasecmp(key, "event") == 0 || strcasecmp(key, "trigger") == 0)
            assign_string(&current.eventName, value);
        else if (strcasecmp(key, "usage") == 0 || strcasecmp(key, "category") == 0)
            current.usage = parse_usage(value);
        else if (strcasecmp(key, "loop") == 0)
            current.loop = parse_bool(value, true);
        else if (strcasecmp(key, "loop_count") == 0)
            current.loopCount = atoi(value);
        else if (strcasecmp(key, "volume") == 0)
            current.defaultVolume = (float)atof(value);
        else if (strcasecmp(key, "fade_in") == 0)
            current.defaultFadeIn = (float)atof(value);
        else if (strcasecmp(key, "fade_out") == 0)
            current.defaultFadeOut = (float)atof(value);
        else if (strcasecmp(key, "crossfade_lead") == 0)
            current.crossfadeLead = (float)atof(value);
        else if (strcasecmp(key, "cue_offset") == 0 || strcasecmp(key, "start_offset") == 0)
            current.cueOffset = (float)atof(value);
        else
            fprintf(stderr, "Warning: Unknown key in music file (%s:%d): %s\n", path, lineNumber, key);
    }

    if (inSection)
        finalize_definition(&current, &defs, &count, &capacity);
    else
        free_definition(&current);

    fclose(f);

    if (outCount)
        *outCount = count;

    if (count == 0)
    {
        free(defs);
        return NULL;
    }

    return defs;
}

void music_loader_free(MusicDefinition* defs, int count)
{
    if (!defs)
        return;
    for (int i = 0; i < count; ++i)
        free_definition(&defs[i]);
    free(defs);
}
