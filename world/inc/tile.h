#ifndef TILE_H
#define TILE_H

#include <stdbool.h>
#include <raylib.h>

// Liste des types de tuiles disponibles
typedef enum
{
    TILE_GRASS = 0,
    TILE_WALL,
    TILE_WATER,
    TILE_COUNT
} TileTypeID;

// Structure décrivant les propriétés d’un type de tuile
typedef struct
{
    const char* name;     // Nom lisible
    Color       color;    // Couleur par défaut
    bool        passable; // Vrai si un PNJ peut marcher dessus
    Texture2D   texture;  // Texture (peut être un texture NULL si non utilisée)
} TileType;

// Initialise le tableau des TileType (charge les textures si nécessaire)
void init_tile_types(void);
// Décharge les textures à la fermeture
void unload_tile_types(void);
// Renvoie un pointeur sur le type de tuile demandé
TileType* get_tile_type(TileTypeID id);

#endif // TILE_H
