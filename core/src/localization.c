#include "localization.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCALIZATION_DIR      "data/lang"
#define LOCALIZATION_FALLBACK "en"
#define LOCALIZATION_DEFAULT  "fr"

typedef struct LocalizationEntry
{
    char* key;
    char* value;
} LocalizationEntry;

typedef struct LocalizationTable
{
    LocalizationEntry* entries;
    int                count;
    int                capacity;
} LocalizationTable;

static LocalizationTable g_primary  = {0};
static LocalizationTable g_fallback = {0};
static char              g_currentLanguage[16] = {0};

static const LocalizationLanguage g_availableLanguages[] = {
    {"fr", "language.fr"},
    {"en", "language.en"},
};
static const int g_availableLanguageCount = (int)(sizeof(g_availableLanguages) / sizeof(g_availableLanguages[0]));

static void strip_utf8_bom(char* text)
{
    if (!text)
        return;
    unsigned char* u = (unsigned char*)text;
    if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF)
        memmove(text, text + 3, strlen(text + 3) + 1);
}

static void trim_whitespace(char* s)
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

static char* string_duplicate(const char* src)
{
    if (!src)
        return NULL;
    size_t len  = strlen(src);
    char*  copy = (char*)malloc(len + 1);
    if (!copy)
        return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

static void table_clear(LocalizationTable* table)
{
    if (!table)
        return;

    for (int i = 0; i < table->count; ++i)
    {
        free(table->entries[i].key);
        free(table->entries[i].value);
    }
    free(table->entries);
    table->entries  = NULL;
    table->count    = 0;
    table->capacity = 0;
}

static bool table_append(LocalizationTable* table, const char* key, const char* value)
{
    if (!table || !key || !value)
        return false;

    for (int i = 0; i < table->count; ++i)
    {
        if (strcmp(table->entries[i].key, key) == 0)
        {
            char* newValue = string_duplicate(value);
            if (!newValue)
                return false;
            free(table->entries[i].value);
            table->entries[i].value = newValue;
            return true;
        }
    }

    if (table->count >= table->capacity)
    {
        int newCapacity = (table->capacity == 0) ? 32 : table->capacity * 2;
        LocalizationEntry* resized = (LocalizationEntry*)realloc(table->entries, (size_t)newCapacity * sizeof(LocalizationEntry));
        if (!resized)
            return false;
        table->entries  = resized;
        table->capacity = newCapacity;
    }

    char* keyCopy   = string_duplicate(key);
    char* valueCopy = string_duplicate(value);
    if (!keyCopy || !valueCopy)
    {
        free(keyCopy);
        free(valueCopy);
        return false;
    }

    table->entries[table->count].key   = keyCopy;
    table->entries[table->count].value = valueCopy;
    table->count++;
    return true;
}

static const char* table_lookup(const LocalizationTable* table, const char* key)
{
    if (!table || !key)
        return NULL;

    for (int i = 0; i < table->count; ++i)
    {
        if (strcmp(table->entries[i].key, key) == 0)
            return table->entries[i].value;
    }
    return NULL;
}

static bool parse_lang_file(const char* code, LocalizationTable* out)
{
    if (!code || !*code || !out)
        return false;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s.lang", LOCALIZATION_DIR, code);

    FILE* f = fopen(path, "r");
    if (!f)
        return false;

    LocalizationTable table = {0};
    char              line[512];
    bool              firstLine = true;

    while (fgets(line, sizeof(line), f))
    {
        if (firstLine)
        {
            strip_utf8_bom(line);
            firstLine = false;
        }

        char* comment = strpbrk(line, "#;");
        if (comment)
            *comment = '\0';

        trim_whitespace(line);
        if (line[0] == '\0')
            continue;

        char* equals = strchr(line, '=');
        if (!equals)
            continue;

        *equals = '\0';
        char* key   = line;
        char* value = equals + 1;
        trim_whitespace(key);
        trim_whitespace(value);

        if (*key == '\0')
            continue;

        const char* finalValue = value;
        size_t      valueLen   = strlen(value);
        if (valueLen >= 2 && value[0] == '"' && value[valueLen - 1] == '"')
        {
            value[valueLen - 1] = '\0';
            finalValue           = value + 1;
        }

        if (!table_append(&table, key, finalValue))
        {
            table_clear(&table);
            fclose(f);
            return false;
        }
    }

    fclose(f);
    *out = table;
    return true;
}

const LocalizationLanguage* localization_languages(int* count)
{
    if (count)
        *count = g_availableLanguageCount;
    return g_availableLanguages;
}

const char* localization_default_language(void)
{
    return LOCALIZATION_DEFAULT;
}

const char* localization_current_language(void)
{
    if (g_currentLanguage[0] == '\0')
        return LOCALIZATION_FALLBACK;
    return g_currentLanguage;
}

void localization_shutdown(void)
{
    table_clear(&g_primary);
    table_clear(&g_fallback);
    g_currentLanguage[0] = '\0';
}

bool localization_init(const char* language)
{
    localization_shutdown();

    parse_lang_file(LOCALIZATION_FALLBACK, &g_fallback);

    const char* desired = (language && *language) ? language : LOCALIZATION_DEFAULT;
    if (!localization_set_language(desired))
    {
        if (strcmp(desired, LOCALIZATION_FALLBACK) != 0)
            localization_set_language(LOCALIZATION_FALLBACK);
    }

    if (g_currentLanguage[0] == '\0')
        snprintf(g_currentLanguage, sizeof(g_currentLanguage), "%s", LOCALIZATION_FALLBACK);

    return g_currentLanguage[0] != '\0';
}

bool localization_set_language(const char* language)
{
    if (!language || !*language)
        language = LOCALIZATION_DEFAULT;

    if (strcmp(language, g_currentLanguage) == 0)
        return true;

    if (strcmp(language, LOCALIZATION_FALLBACK) == 0)
    {
        table_clear(&g_primary);
        snprintf(g_currentLanguage, sizeof(g_currentLanguage), "%s", language);
        return true;
    }

    LocalizationTable table = {0};
    if (!parse_lang_file(language, &table))
        return false;

    table_clear(&g_primary);
    g_primary = table;
    snprintf(g_currentLanguage, sizeof(g_currentLanguage), "%s", language);
    return true;
}

const char* localization_try(const char* key)
{
    const char* text = table_lookup(&g_primary, key);
    if (text)
        return text;
    return table_lookup(&g_fallback, key);
}

const char* localization_get(const char* key)
{
    if (!key || *key == '\0')
        return "";

    const char* text = localization_try(key);
    if (text)
        return text;
    return key;
}
