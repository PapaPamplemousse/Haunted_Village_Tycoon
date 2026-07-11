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

} // namespace

void InspectionSystem::Render(const InputManager& inputManager, const EntityManager& entityManager,
                              const EntitySpatialGrid& spatialGrid) const {
    if (!inputManager.IsInspectPressed()) {
        return;
    }

    const int hoverX = inputManager.GetMouseGridX();
    const int hoverY = inputManager.GetMouseGridY();

    EntityID hoveredEntity = static_cast<EntityID>(-1);
    EntityID firstEntityOnTile = static_cast<EntityID>(-1);
    EntityID doorEntityOnTile = static_cast<EntityID>(-1);
    EntityID behaviorEntityOnTile = static_cast<EntityID>(-1);

    const std::vector<EntityID> hoveredEntities = spatialGrid.GetEntitiesAtTile(hoverX, hoverY, entityManager);

    for (EntityID i : hoveredEntities) {
        if (i >= entityManager.active.size() || !entityManager.active[i] || !entityManager.hasTransform[i]) {
            continue;
        }

        if (firstEntityOnTile == static_cast<EntityID>(-1)) {
            firstEntityOnTile = i;
        }

        if (entityManager.hasDoor[i]) {
            doorEntityOnTile = i;
        }

        if (entityManager.hasBehavior[i]) {
            behaviorEntityOnTile = i;
        }
    }

    if (behaviorEntityOnTile != static_cast<EntityID>(-1)) {
        hoveredEntity = behaviorEntityOnTile;
    } else if (doorEntityOnTile != static_cast<EntityID>(-1)) {
        hoveredEntity = doorEntityOnTile;
    } else {
        hoveredEntity = firstEntityOnTile;
    }

    if (hoveredEntity == static_cast<EntityID>(-1)) {
        return;
    }

    std::vector<std::string> lines;
    const EntityID i = hoveredEntity;

    lines.push_back("Entity ID: " + std::to_string(i));

    if (entityManager.hasTag[i]) {
        const auto& tag = entityManager.tags[i];

        if (!tag.firstName.empty()) {
            lines.push_back(tag.firstName + " the " + tag.name);
        } else {
            lines.push_back(tag.name);
        }

        if (!tag.species.empty()) {
            lines.push_back("Species: " + tag.species);
        }

        if (!tag.gender.empty() && tag.gender != "undefined") {
            lines.push_back("Gender: " + tag.gender);
        }

        lines.push_back(TextFormat("Age: %d", tag.age));
    }

    if (entityManager.hasProfession[i]) {
        const auto& prof = entityManager.professions[i];
        std::string profName = prof.currentProfession;

        if (!profName.empty() && profName != "none") {
            profName[0] = static_cast<char>(std::toupper(profName[0]));
        }

        lines.push_back("Profession: " + profName);
    }

    if (entityManager.hasTransform[i]) {
        const int tileX = WorldToTile(entityManager.transforms[i].position.x);
        const int tileY = WorldToTile(entityManager.transforms[i].position.y);

        lines.push_back("Tile: " + std::to_string(tileX) + ", " + std::to_string(tileY));
    }

    if (entityManager.hasDoor[i]) {
        const auto& door = entityManager.doors[i];

        lines.push_back("Door state: " + std::string(DoorStateToString(door.state)));
        lines.push_back("Owner ID: " + std::to_string(door.ownerId));
    }

    if (entityManager.hasHealth[i]) {
        lines.push_back(TextFormat("HP: %.0f / %.0f", entityManager.healths[i].current, entityManager.healths[i].max));
    }

    if (entityManager.hasStats[i]) {
        lines.push_back(TextFormat("Speed: %.0f", entityManager.stats[i].maxSpeed));
        lines.push_back(TextFormat("Base ATK: %.0f", entityManager.stats[i].baseAttack));
        lines.push_back(TextFormat("Action radius: %.0f", entityManager.stats[i].actionRadiusTiles));
    }

    if (entityManager.hasNeeds[i]) {
        lines.push_back(TextFormat("Hunger: %.0f / %.0f", entityManager.needs[i].hunger, entityManager.needs[i].maxHunger));
    }

    if (entityManager.hasEquipment[i]) {
        const auto& equip = entityManager.equipments[i];

        lines.push_back("--- Equipment ---");
        lines.push_back("Tool: " + equip.rightHandToolType);
        lines.push_back(TextFormat("Weapon damage: %.0f", equip.rightHandDamage));
    }

    const bool hasInv = entityManager.hasInventory[i];
    const bool hasHarv = entityManager.hasHarvestable[i];

    if (hasInv || hasHarv) {
        lines.push_back("--- Inventory ---");

        bool isEmpty = true;

        if (hasInv && !entityManager.inventories[i].items.empty()) {
            for (const auto& item : entityManager.inventories[i].items) {
                lines.push_back(item.first + ": " + std::to_string(item.second));
            }

            isEmpty = false;
        }

        if (entityManager.hasStorage[i]) {
            lines.push_back(TextFormat("Storage capacity: %d", entityManager.storages[i].capacity));
        }

        if (hasHarv) {
            const auto& harvestable = entityManager.harvestables[i];

            for (const DropEntry& drop : harvestable.drops) {
                if (drop.amount > 0 && !drop.itemId.empty()) {
                    lines.push_back(drop.itemId + ": " + std::to_string(drop.amount) + " (Yield)");

                    isEmpty = false;
                }
            }
        }

        if (isEmpty) {
            lines.push_back("Empty");
        }
    }

    if (entityManager.hasBehavior[i]) {
        const auto& behavior = entityManager.behaviors[i];

        lines.push_back("--- AI ---");
        lines.push_back("Task: " + behavior.currentTask);
        lines.push_back("Activity: " + behavior.activityPeriod);
        lines.push_back(TextFormat("Work hours: %.0f - %.0f", behavior.workStartHour, behavior.workEndHour));
        lines.push_back(TextFormat("Store threshold: %d", behavior.storeThreshold));
    }

    if (lines.empty()) {
        return;
    }

    const Vector2 mousePos = GetMousePosition();

    const float boxWidth = 330.0f;
    const float lineHeight = 22.0f;
    const float boxHeight = lines.size() * lineHeight + 12.0f;

    float boxX = mousePos.x + 15.0f;
    float boxY = mousePos.y + 15.0f;

    if (boxX + boxWidth > GetScreenWidth()) {
        boxX = mousePos.x - boxWidth - 15.0f;
    }

    if (boxY + boxHeight > GetScreenHeight()) {
        boxY = mousePos.y - boxHeight - 15.0f;
    }

    DrawRectangle(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight),
                  ColorAlpha(BLACK, 0.9f));

    DrawRectangleLines(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight), DARKGRAY);

    for (size_t l = 0; l < lines.size(); ++l) {
        Color color = LIGHTGRAY;

        if (l == 0) {
            color = GOLD;
        } else if (lines[l] == "--- AI ---" || lines[l] == "--- Inventory ---" || lines[l] == "--- Equipment ---") {
            color = SKYBLUE;
        }

        DrawText(lines[l].c_str(), static_cast<int>(boxX + 10.0f), static_cast<int>(boxY + 8.0f + l * lineHeight), 18, color);
    }
}
