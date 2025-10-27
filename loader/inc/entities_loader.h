#ifndef ENTITIES_LOADER_H
#define ENTITIES_LOADER_H

#include <stdbool.h>

#include "entity.h"

bool entities_loader_load(EntitySystem* sys, const char* path);

#endif
