#include "debug/InspectionSystem.hpp"

#include "core/Config.hpp"

#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace {

int WorldToTile(float value) {
    return static_cast<int>(std::floor(value / Config::TILE_SIZE));
}

const char* DoorStateToString(DoorState state) {
    switch (state) {
        case DoorState::OPEN:
            return "OPEN";
        case DoorState::CLOSED:
            return "CLOSED";
        case DoorState::LOCKED:
            return "LOCKED";
        default:
            return "UNKNOWN";
    }
}

float ComputeFoodNutrition(const InventoryComponent& inventory, const ResourceRegistry& resourceReg) {
    float totalNutrition = 0.0f;

    for (const auto& item : inventory.items) {
        const std::string& itemId = item.first;
        const int count = item.second;

        if (count <= 0)
            continue;

        const ResourceDef* resource = resourceReg.GetResourceDef(itemId);
        if (resource == nullptr || !resource->isConsumable || resource->nutrition <= 0.0f) {
            continue;
        }

        totalNutrition += resource->nutrition * static_cast<float>(count);
    }
    return totalNutrition;
}

// --- NOUVEAUX OUTILS POUR UN CODE CLEAN ---

struct TooltipLine {
    std::string text;
    Color color;
};

void AddHeader(std::vector<TooltipLine>& lines, const std::string& text) {
    lines.push_back({text, SKYBLUE});
}

void AddNormal(std::vector<TooltipLine>& lines, const std::string& text) {
    lines.push_back({text, LIGHTGRAY});
}

EntityID GetHoveredEntity(int hoverX, int hoverY, const EntitySpatialGrid& spatialGrid, const EntityManager& entityManager) {
    const std::vector<EntityID> hoveredEntities = spatialGrid.GetEntitiesAtTile(hoverX, hoverY, entityManager);

    EntityID firstEntity = static_cast<EntityID>(-1);
    EntityID doorEntity = static_cast<EntityID>(-1);
    EntityID behaviorEntity = static_cast<EntityID>(-1);

    for (EntityID i : hoveredEntities) {
        if (i >= entityManager.active.size() || !entityManager.active[i] || !entityManager.hasTransform[i]) {
            continue;
        }

        if (firstEntity == static_cast<EntityID>(-1))
            firstEntity = i;
        if (entityManager.hasDoor[i])
            doorEntity = i;
        if (entityManager.hasBehavior[i])
            behaviorEntity = i;
    }

    if (behaviorEntity != static_cast<EntityID>(-1))
        return behaviorEntity;
    if (doorEntity != static_cast<EntityID>(-1))
        return doorEntity;
    return firstEntity;
}

std::vector<TooltipLine> BuildInspectionLines(EntityID i, const EntityManager& entityManager, const ResourceRegistry& resourceReg) {
    std::vector<TooltipLine> lines;

    lines.push_back({"Entity ID: " + std::to_string(i), GOLD});

    // 1. Identité & Tags
    if (entityManager.hasTag[i]) {
        const auto& tag = entityManager.tags[i];
        if (!tag.firstName.empty())
            AddNormal(lines, tag.firstName + " the " + tag.name);
        else
            AddNormal(lines, tag.name);

        if (!tag.species.empty())
            AddNormal(lines, "Species: " + tag.species);
        if (!tag.gender.empty() && tag.gender != "undefined")
            AddNormal(lines, "Gender: " + tag.gender);
        AddNormal(lines, TextFormat("Age: %d", tag.age));
    }

    // 2. Position
    if (entityManager.hasTransform[i]) {
        const int tileX = WorldToTile(entityManager.transforms[i].position.x);
        const int tileY = WorldToTile(entityManager.transforms[i].position.y);
        AddNormal(lines, "Tile: " + std::to_string(tileX) + ", " + std::to_string(tileY));
    }

    // 3. Stats & Vitals
    if (entityManager.hasHealth[i]) {
        AddNormal(lines, TextFormat("HP: %.0f / %.0f", entityManager.healths[i].current, entityManager.healths[i].max));
    }
    if (entityManager.hasNeeds[i]) {
        AddNormal(lines, TextFormat("Hunger: %.0f / %.0f", entityManager.needs[i].hunger, entityManager.needs[i].maxHunger));
    }
    if (entityManager.hasStats[i]) {
        AddNormal(lines, TextFormat("Speed: %.0f", entityManager.stats[i].maxSpeed));
        AddNormal(lines, TextFormat("Base ATK: %.0f", entityManager.stats[i].baseAttack));
        AddNormal(lines, TextFormat("Action radius: %.0f", entityManager.stats[i].actionRadiusTiles));
    }

    // 4. Intelligence & Rôles
    if (entityManager.hasProfession[i]) {
        std::string profName = entityManager.professions[i].currentProfession;
        if (!profName.empty() && profName != "none") {
            profName[0] = static_cast<char>(std::toupper(profName[0]));
        }
        AddNormal(lines, "Profession: " + profName);
    }
    if (entityManager.hasBehavior[i]) {
        AddHeader(lines, "--- AI ---");
        AddNormal(lines, "Task: " + entityManager.behaviors[i].currentTask);
    }

    // 5. Famille & Société
    if (entityManager.hasFamily[i]) {
        const auto& family = entityManager.families[i];
        AddHeader(lines, "--- Family ---");
        if (family.partnerId != static_cast<EntityID>(-1))
            AddNormal(lines, "Partner ID: " + std::to_string(family.partnerId));
        if (family.parentA != static_cast<EntityID>(-1))
            AddNormal(lines, "Parent A: " + std::to_string(family.parentA));
        if (family.parentB != static_cast<EntityID>(-1))
            AddNormal(lines, "Parent B: " + std::to_string(family.parentB));
        AddNormal(lines, "Children: " + std::to_string(family.children.size()));
    }
    if (entityManager.hasVillageMember[i]) {
        AddHeader(lines, "--- Village Member ---");
        AddNormal(lines, "Village ID: " + std::to_string(entityManager.villageMembers[i].villageId));
    }
    if (entityManager.hasVillage[i]) {
        const auto& village = entityManager.villages[i];
        AddHeader(lines, "--- Village ---");
        AddNormal(lines, "Name: " + village.name);
        AddNormal(lines, "Population: " + std::to_string(village.currentPopulation) + " / " + std::to_string(village.populationLimit));
        AddNormal(lines, "Adults: " + std::to_string(village.adultPopulation));
        AddNormal(lines, "Children: " + std::to_string(village.childPopulation));

        if (entityManager.hasInventory[i]) {
            int foodNutrition = static_cast<int>(ComputeFoodNutrition(entityManager.inventories[i], resourceReg));
            AddNormal(lines, "Food nutrition: " + std::to_string(foodNutrition));
        }
    }

    // 6. Équipement & Inventaire & Objets interactifs
    if (entityManager.hasDoor[i]) {
        const auto& door = entityManager.doors[i];
        AddHeader(lines, "--- Door ---");
        AddNormal(lines, "State: " + std::string(DoorStateToString(door.state)));
        AddNormal(lines, "Owner ID: " + std::to_string(door.ownerId));
    }

    if (entityManager.hasEquipment[i]) {
        const auto& equip = entityManager.equipments[i];
        AddHeader(lines, "--- Equipment ---");
        AddNormal(lines, "Tool: " + equip.rightHandToolType);
        AddNormal(lines, TextFormat("Weapon damage: %.0f", equip.rightHandDamage));
    }

    const bool hasInv = entityManager.hasInventory[i];
    const bool hasHarv = entityManager.hasHarvestable[i];
    if (hasInv || hasHarv) {
        AddHeader(lines, "--- Inventory ---");
        bool isEmpty = true;

        if (hasInv && !entityManager.inventories[i].items.empty()) {
            for (const auto& item : entityManager.inventories[i].items) {
                AddNormal(lines, item.first + ": " + std::to_string(item.second));
            }
            isEmpty = false;
        }

        if (entityManager.hasStorage[i]) {
            AddNormal(lines, TextFormat("Storage capacity: %d", entityManager.storages[i].capacity));
        }

        if (hasHarv) {
            for (const DropEntry& drop : entityManager.harvestables[i].drops) {
                if (drop.amount > 0 && !drop.itemId.empty()) {
                    AddNormal(lines, drop.itemId + ": " + std::to_string(drop.amount) + " (Yield)");
                    isEmpty = false;
                }
            }
        }

        if (isEmpty)
            AddNormal(lines, "Empty");
    }

    return lines;
}

void DrawTooltipBox(const std::vector<TooltipLine>& lines) {
    if (lines.empty())
        return;

    const Vector2 mousePos = GetMousePosition();
    const float boxWidth = 330.0f;
    const float lineHeight = 22.0f;
    const float boxHeight = lines.size() * lineHeight + 12.0f;

    float boxX = mousePos.x + 15.0f;
    float boxY = mousePos.y + 15.0f;

    // Reste dans l'écran
    if (boxX + boxWidth > GetScreenWidth())
        boxX = mousePos.x - boxWidth - 15.0f;
    if (boxY + boxHeight > GetScreenHeight())
        boxY = mousePos.y - boxHeight - 15.0f;

    DrawRectangle(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight),
                  ColorAlpha(BLACK, 0.9f));
    DrawRectangleLines(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight), DARKGRAY);

    for (size_t l = 0; l < lines.size(); ++l) {
        DrawText(lines[l].text.c_str(), static_cast<int>(boxX + 10.0f), static_cast<int>(boxY + 8.0f + l * lineHeight), 18, lines[l].color);
    }
}

} // namespace

// --- LA FONCTION PRINCIPALE ---

void InspectionSystem::Render(const InputManager& inputManager, const EntityManager& entityManager, const EntitySpatialGrid& spatialGrid,
                              const ResourceRegistry& resourceReg) {
    if (!inputManager.IsInspectPressed()) {
        return;
    }

    const EntityID hoveredEntity = GetHoveredEntity(inputManager.GetMouseGridX(), inputManager.GetMouseGridY(), spatialGrid, entityManager);

    if (hoveredEntity == static_cast<EntityID>(-1)) {
        return;
    }

    std::vector<TooltipLine> lines = BuildInspectionLines(hoveredEntity, entityManager, resourceReg);
    DrawTooltipBox(lines);
}
