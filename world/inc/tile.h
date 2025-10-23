#ifndef TILE_H
#define TILE_H

#include <stdbool.h>
#include <raylib.h>

// Liste des types de tuiles disponibles
typedef enum
{
    TILE_GRASS = 0,
    TILE_WATER,
    TILE_LAVA,
    TILE_COUNT
} TileTypeID;

typedef enum
{
    TILE_CATEGORY_GROUND,
    TILE_CATEGORY_WALL,
    TILE_CATEGORY_DOOR,
    TILE_CATEGORY_WATER,
    TILE_CATEGORY_TREE,
    TILE_CATEGORY_ROAD
} TileCategory;

typedef struct
{
    const char*  name;
    TileTypeID   id;
    TileCategory category;
    bool         walkable;
    Color        color;
    Texture2D    texture; // optional (not used initially)
    bool         isBreakable;
    int          durability;
} TileType;

// Initialise le tableau des TileType (charge les textures si nécessaire)
void init_tile_types(void);
// Décharge les textures à la fermeture
void unload_tile_types(void);
// Renvoie un pointeur sur le type de tuile demandé
TileType* get_tile_type(TileTypeID id);

#endif // TILE_H
