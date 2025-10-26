#ifndef BIOME_DEFINITIONS_H
#define BIOME_DEFINITIONS_H

#include "world.h"

typedef struct
{
    BiomeKind  kind;
    TileTypeID primary, secondary;
    float      tempMin, tempMax;
    float      humidMin, humidMax;
    float      heightMin, heightMax;
    float      treeMul, bushMul, rockMul, structMul;
    int        maxInstances;
    int        minInstances;
} BiomeDef;

extern BiomeDef gBiomeDefs[];
extern int      gBiomeCount;

void            load_biome_definitions(const char* path);
const char*     get_biome_name(BiomeKind k);
const BiomeDef* get_biome_def(BiomeKind kind);
BiomeKind       biome_kind_from_string(const char* s);
const char*     biome_kind_to_string(BiomeKind k);

#endif
