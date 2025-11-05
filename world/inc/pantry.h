#ifndef PANTRY_H
#define PANTRY_H

#include <stdbool.h>
#include <stdint.h>
#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PantryItemType
{
    PANTRY_ITEM_NONE = 0,
    PANTRY_ITEM_MEAT,
    PANTRY_ITEM_PLANT,
    PANTRY_ITEM_MAX
} PantryItemType;

typedef struct Pantry
{
    int id;
    int buildingId;
    int capacity;
    int counts[PANTRY_ITEM_MAX];
} Pantry;

void      pantry_system_reset(void);
Pantry*   pantry_create_or_get(int buildingId, int capacity);
Pantry*   pantry_get_for_building(int buildingId);
bool      pantry_deposit(Pantry* pantry, PantryItemType type, int quantity);
int       pantry_withdraw(Pantry* pantry, PantryItemType type, int quantity);
void      pantry_remove(int buildingId);
void      pantry_debug_draw(const Pantry* pantry, Vector2 screenPosition);

#ifdef __cplusplus
}
#endif

#endif /* PANTRY_H */
