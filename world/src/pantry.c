#include "pantry.h"

#include <stdio.h>
#include <string.h>

#include "world.h"

static Pantry gPantries[MAX_BUILDINGS];
static int    gPantryCount = 0;

static Pantry* pantry_find(int buildingId)
{
    if (buildingId < 0)
        return NULL;
    for (int i = 0; i < gPantryCount; ++i)
    {
        if (gPantries[i].buildingId == buildingId)
            return &gPantries[i];
    }
    return NULL;
}

void pantry_system_reset(void)
{
    memset(gPantries, 0, sizeof(gPantries));
    gPantryCount = 0;
}

Pantry* pantry_get_for_building(int buildingId)
{
    return pantry_find(buildingId);
}

Pantry* pantry_create_or_get(int buildingId, int capacity)
{
    Pantry* existing = pantry_find(buildingId);
    if (existing)
    {
        if (capacity > 0)
            existing->capacity = capacity;
        return existing;
    }

    if (gPantryCount >= MAX_BUILDINGS)
        return NULL;

    Pantry* pantry = &gPantries[gPantryCount++];
    memset(pantry, 0, sizeof(*pantry));
    pantry->id         = gPantryCount;
    pantry->buildingId = buildingId;
    pantry->capacity   = (capacity > 0) ? capacity : 0;
    return pantry;
}

bool pantry_deposit(Pantry* pantry, PantryItemType type, int quantity)
{
    if (!pantry || type <= PANTRY_ITEM_NONE || type >= PANTRY_ITEM_MAX || quantity <= 0)
        return false;

    int currentTotal = 0;
    for (int i = 0; i < PANTRY_ITEM_MAX; ++i)
        currentTotal += pantry->counts[i];

    if (pantry->capacity > 0 && currentTotal >= pantry->capacity)
        return false;

    int space = pantry->capacity > 0 ? pantry->capacity - currentTotal : quantity;
    if (space <= 0)
        return false;

    int toStore = (quantity <= space) ? quantity : space;
    pantry->counts[type] += toStore;
    return toStore == quantity;
}

int pantry_withdraw(Pantry* pantry, PantryItemType type, int quantity)
{
    if (!pantry || type <= PANTRY_ITEM_NONE || type >= PANTRY_ITEM_MAX || quantity <= 0)
        return 0;

    int available = pantry->counts[type];
    if (available <= 0)
        return 0;

    int taken = (quantity <= available) ? quantity : available;
    pantry->counts[type] -= taken;
    return taken;
}

void pantry_remove(int buildingId)
{
    for (int i = 0; i < gPantryCount; ++i)
    {
        if (gPantries[i].buildingId == buildingId)
        {
            if (i < gPantryCount - 1)
                memmove(&gPantries[i], &gPantries[i + 1], (size_t)(gPantryCount - i - 1) * sizeof(Pantry));
            --gPantryCount;
            return;
        }
    }
}

void pantry_debug_draw(const Pantry* pantry, Vector2 screenPosition)
{
    if (!pantry)
        return;

    char buffer[128];
    snprintf(buffer,
             sizeof(buffer),
             "Pantry #%d M:%d P:%d",
             pantry->buildingId,
             pantry->counts[PANTRY_ITEM_MEAT],
             pantry->counts[PANTRY_ITEM_PLANT]);
    DrawText(buffer, (int)screenPosition.x, (int)screenPosition.y, 10, WHITE);
}
