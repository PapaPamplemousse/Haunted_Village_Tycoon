#pragma once

#include <stdbool.h>

typedef struct LocalizationLanguage
{
    const char* code;
    const char* label_key;
} LocalizationLanguage;

bool localization_init(const char* language);
void localization_shutdown(void);

const char* localization_default_language(void);
const char* localization_current_language(void);

bool localization_set_language(const char* language);

const char* localization_get(const char* key);
const char* localization_try(const char* key);

const LocalizationLanguage* localization_languages(int* count);
