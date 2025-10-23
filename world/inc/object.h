#ifndef OBJECT_H
#define OBJECT_H

#include "raylib.h"
#include <stdbool.h>

// Forward declarations pour éviter les inclusions circulaires
typedef struct Building Building;

// Identifiants uniques des types d'objets
typedef enum
{
    OBJ_NONE = 0,
    OBJ_BED_SMALL,
    OBJ_BED_LARGE,
    OBJ_TABLE_WOOD,
    OBJ_CHAIR_WOOD,
    OBJ_TORCH_WALL,
    OBJ_FIRE_PIT,
    OBJ_CHEST_WOOD,
    OBJ_WORKBENCH,
    OBJ_DOOR_WOOD,
    OBJ_WINDOW_WOOD,
    OBJ_WALL_STONE,
    OBJ_DECOR_PLANT,
    OBJ_MAX
} ObjectTypeID;

// Définition des types d’objets
typedef struct
{
    ObjectTypeID id;
    const char*  name;        // id interne
    const char*  displayName; // nom lisible
    const char*  category;    // "structure", "furniture", etc.

    int       maxHP;
    int       comfort;
    int       warmth;
    int       lightLevel;
    int       width;
    int       height;
    bool      walkable;
    bool      flammable;
    Color     color;
    Texture2D texture;
} ObjectType;

// Instance d’objet sur la carte
typedef struct Object
{
    const ObjectType* type;
    Vector2           position;
    int               hp;
    bool              isActive;
} Object;

// Condition d’objet pour définir une pièce
typedef struct
{
    ObjectTypeID objectId;
    int          minCount;
} ObjectRequirement;

// Règle pour définir un type de pièce
typedef struct
{
    const char*              name;
    int                      minArea;
    int                      maxArea;
    const ObjectRequirement* requirements;
    int                      requirementCount;
} RoomTypeRule;

// Fonctions publiques
const ObjectType*   get_object_type(ObjectTypeID id);
const RoomTypeRule* analyze_building_type(const Building* b);
Object*             create_object(ObjectTypeID id, int x, int y);
void                draw_objects(Camera2D* camera);
bool                is_wall_object(const Object* o);
bool                is_door_object(const Object* o);
bool                is_blocking_object(const Object* o);

#endif
