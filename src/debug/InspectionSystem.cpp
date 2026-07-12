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
        if (item.second <= 0)
            continue;
        const ResourceDef* resource = resourceReg.GetResourceDef(item.first);
        if (resource && resource->isConsumable && resource->nutrition > 0.0f) {
            totalNutrition += resource->nutrition * static_cast<float>(item.second);
        }
    }
    return totalNutrition;
}

// --- SYSTEME D'UI AVANCE ---

enum class InfoType { Title, Header, Text, KeyValue, ProgressBar };

struct TooltipLine {
    InfoType type;
    std::string label;
    std::string value;
    float currentVal;
    float maxVal;
    Color color;
};

void AddTitle(std::vector<TooltipLine>& lines, const std::string& text) {
    lines.push_back({InfoType::Title, text, "", 0, 0, GOLD});
}

void AddHeader(std::vector<TooltipLine>& lines, const std::string& text) {
    lines.push_back({InfoType::Header, text, "", 0, 0, SKYBLUE});
}

void AddText(std::vector<TooltipLine>& lines, const std::string& text, Color color = LIGHTGRAY) {
    lines.push_back({InfoType::Text, text, "", 0, 0, color});
}

void AddKV(std::vector<TooltipLine>& lines, const std::string& key, const std::string& value) {
    lines.push_back({InfoType::KeyValue, key, value, 0, 0, WHITE});
}

void AddBar(std::vector<TooltipLine>& lines, const std::string& label, float current, float max, Color color) {
    lines.push_back({InfoType::ProgressBar, label, "", current, max, color});
}

EntityID GetHoveredEntity(int hoverX, int hoverY, const EntitySpatialGrid& spatialGrid, const EntityManager& entityManager) {
    const std::vector<EntityID> hoveredEntities = spatialGrid.GetEntitiesAtTile(hoverX, hoverY, entityManager);
    EntityID firstEntity = static_cast<EntityID>(-1);
    EntityID doorEntity = static_cast<EntityID>(-1);
    EntityID behaviorEntity = static_cast<EntityID>(-1);

    for (EntityID i : hoveredEntities) {
        if (i >= entityManager.active.size() || !entityManager.active[i] || !entityManager.hasTransform[i])
            continue;
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

    // 1. Titre & Identité
    std::string title = "Entity #" + std::to_string(i);
    if (entityManager.hasTag[i]) {
        const auto& tag = entityManager.tags[i];
        title = tag.firstName.empty() ? tag.name : (tag.firstName + " the " + tag.name);
        AddTitle(lines, title);

        if (!tag.species.empty())
            AddKV(lines, "Species", tag.species);
        if (!tag.gender.empty() && tag.gender != "undefined")
            AddKV(lines, "Gender", tag.gender);
        AddKV(lines, "Age", std::to_string(tag.age));
    } else {
        AddTitle(lines, title);
    }

    if (entityManager.hasTransform[i]) {
        const int tileX = WorldToTile(entityManager.transforms[i].position.x);
        const int tileY = WorldToTile(entityManager.transforms[i].position.y);
        AddKV(lines, "Position", std::to_string(tileX) + ", " + std::to_string(tileY));
    }

    // 2. Barres de Vies et Stats
    if (entityManager.hasHealth[i] || entityManager.hasNeeds[i] || entityManager.hasStats[i]) {
        AddHeader(lines, "Vitals & Stats");

        if (entityManager.hasHealth[i]) {
            AddBar(lines, "HP", entityManager.healths[i].current, entityManager.healths[i].max, RED);
        }
        if (entityManager.hasNeeds[i]) {
            AddBar(lines, "Hunger", entityManager.needs[i].hunger, entityManager.needs[i].maxHunger, ORANGE);
            AddBar(lines, "Fatigue", entityManager.needs[i].fatigue, entityManager.needs[i].maxFatigue, SKYBLUE);
        }
        if (entityManager.hasStats[i]) {
            AddKV(lines, "Speed", TextFormat("%.0f", entityManager.stats[i].maxSpeed));
            AddKV(lines, "Base ATK", TextFormat("%.0f", entityManager.stats[i].baseAttack));
            AddKV(lines, "Action Radius", TextFormat("%.0f", entityManager.stats[i].actionRadiusTiles));
        }
    }

    // 3. IA & Profession
    if (entityManager.hasBehavior[i] || entityManager.hasProfession[i]) {
        AddHeader(lines, "Behavior");
        if (entityManager.hasProfession[i]) {
            std::string prof = entityManager.professions[i].currentProfession;
            if (!prof.empty() && prof != "none")
                prof[0] = static_cast<char>(std::toupper(prof[0]));
            AddKV(lines, "Profession", prof);
        }
        if (entityManager.hasBehavior[i]) {
            AddKV(lines, "Current Task", entityManager.behaviors[i].currentTask);
        }
    }

    // 4. Famille & Société
    if (entityManager.hasFamily[i] || entityManager.hasVillage[i] || entityManager.hasVillageMember[i]) {
        AddHeader(lines, "Social & Family");

        if (entityManager.hasVillageMember[i]) {
            AddKV(lines, "Village ID", std::to_string(entityManager.villageMembers[i].villageId));
        }

        if (entityManager.hasVillage[i]) {
            const auto& village = entityManager.villages[i];
            AddKV(lines, "Village Name", village.name);
            AddKV(lines, "Population", std::to_string(village.currentPopulation) + " / " + std::to_string(village.populationLimit));
            AddKV(lines, "Adults / Children", std::to_string(village.adultPopulation) + " / " + std::to_string(village.childPopulation));

            if (entityManager.hasInventory[i]) {
                int foodNutrition = static_cast<int>(ComputeFoodNutrition(entityManager.inventories[i], resourceReg));
                AddKV(lines, "Food Nutrition", std::to_string(foodNutrition));
            }
        }

        if (entityManager.hasFamily[i]) {
            const auto& family = entityManager.families[i];
            if (family.partnerId != static_cast<EntityID>(-1))
                AddKV(lines, "Partner ID", std::to_string(family.partnerId));
            if (family.parentA != static_cast<EntityID>(-1))
                AddKV(lines, "Parent A", std::to_string(family.parentA));
            if (family.parentB != static_cast<EntityID>(-1))
                AddKV(lines, "Parent B", std::to_string(family.parentB));
            AddKV(lines, "Children Count", std::to_string(family.children.size()));
        }
    }

    // 5. Inventaire & Equipement & Portes
    bool hasDoor = entityManager.hasDoor[i];
    bool hasEquip = entityManager.hasEquipment[i];
    bool hasInv = entityManager.hasInventory[i];
    bool hasHarv = entityManager.hasHarvestable[i];
    bool hasRes = entityManager.hasRestSpot[i];

    if (hasDoor || hasEquip || hasInv || hasHarv) {
        AddHeader(lines, "Equipment & Cargo");

        if (hasDoor) {
            const auto& door = entityManager.doors[i];
            AddKV(lines, "Door State", DoorStateToString(door.state));
            AddKV(lines, "Owner ID", std::to_string(door.ownerId));
        }

        if (hasEquip) {
            const auto& equip = entityManager.equipments[i];
            AddKV(lines, "Tool", equip.rightHandToolType);
            AddKV(lines, "Weapon DMG", TextFormat("%.0f", equip.rightHandDamage));
        }

        if (hasInv) {
            if (entityManager.hasStorage[i]) {
                AddKV(lines, "Capacity", std::to_string(entityManager.storages[i].capacity));
            }
            for (const auto& item : entityManager.inventories[i].items) {
                AddKV(lines, item.first, std::to_string(item.second));
            }
        }
        if (hasRes) {
            const auto& rest = entityManager.restSpots[i];

            AddHeader(lines, "Rest Spot");
            AddKV(lines, "Capacity", std::to_string(rest.occupants.size()) + " / " + std::to_string(rest.capacity));

            if (rest.isPrivate) {
                AddKV(lines, "Private", "true");
            }

            if (rest.ownerFamilyId != static_cast<EntityID>(-1)) {
                AddKV(lines, "Owner Family", std::to_string(rest.ownerFamilyId));
            }

            if (rest.ownerVillageId != static_cast<EntityID>(-1)) {
                AddKV(lines, "Owner Village", std::to_string(rest.ownerVillageId));
            }
        }

        if (hasHarv) {
            for (const DropEntry& drop : entityManager.harvestables[i].drops) {
                if (drop.amount > 0 && !drop.itemId.empty()) {
                    AddKV(lines, drop.itemId, std::to_string(drop.amount) + " (Yield)");
                }
            }
        }
    }

    return lines;
}

void DrawTooltipBox(const std::vector<TooltipLine>& lines) {
    if (lines.empty())
        return;

    const float boxWidth = 350.0f;
    const float padding = 15.0f;
    const float baseLineHeight = 22.0f;

    // Calcul dynamique de la hauteur
    float totalHeight = padding * 2.0f;
    for (const auto& line : lines) {
        if (line.type == InfoType::Header)
            totalHeight += baseLineHeight + 10.0f;
        else
            totalHeight += baseLineHeight;
    }

    const Vector2 mousePos = GetMousePosition();
    float boxX = mousePos.x + 20.0f;
    float boxY = mousePos.y + 20.0f;

    // Rester dans l'écran
    if (boxX + boxWidth > GetScreenWidth())
        boxX = mousePos.x - boxWidth - 10.0f;
    if (boxY + totalHeight > GetScreenHeight())
        boxY = mousePos.y - totalHeight - 10.0f;

    // Dessin du fond
    DrawRectangle(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(totalHeight),
                  ColorAlpha(BLACK, 0.95f));
    DrawRectangleLines(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(totalHeight), DARKGRAY);

    // Dessin des éléments
    float currentY = boxY + padding;
    int rightEdge = static_cast<int>(boxX + boxWidth - padding);

    for (const auto& line : lines) {
        if (line.type == InfoType::Header) {
            currentY += 10.0f; // Espacement avant le header
            DrawText(line.label.c_str(), static_cast<int>(boxX + padding), static_cast<int>(currentY), 18, line.color);
            // Ligne de soulignement subtile
            DrawLine(static_cast<int>(boxX + padding), static_cast<int>(currentY + 20), rightEdge, static_cast<int>(currentY + 20),
                     Fade(line.color, 0.3f));
            currentY += baseLineHeight;
        } else if (line.type == InfoType::Title) {
            DrawText(line.label.c_str(), static_cast<int>(boxX + padding), static_cast<int>(currentY), 20, line.color);
            currentY += baseLineHeight;
        } else if (line.type == InfoType::KeyValue) {
            DrawText(line.label.c_str(), static_cast<int>(boxX + padding), static_cast<int>(currentY), 18, GRAY);
            int valWidth = MeasureText(line.value.c_str(), 18);
            DrawText(line.value.c_str(), rightEdge - valWidth, static_cast<int>(currentY), 18, line.color);
            currentY += baseLineHeight;
        } else if (line.type == InfoType::ProgressBar) {
            DrawText(line.label.c_str(), static_cast<int>(boxX + padding), static_cast<int>(currentY), 18, GRAY);

            float barX = boxX + padding + 100.0f; // Alignement des jauges
            float barWidth = rightEdge - barX;
            float barHeight = 16.0f;
            float ratio = line.maxVal > 0 ? (line.currentVal / line.maxVal) : 0;
            if (ratio > 1.0f)
                ratio = 1.0f;
            if (ratio < 0.0f)
                ratio = 0.0f;

            // Fond de la jauge
            DrawRectangle(static_cast<int>(barX), static_cast<int>(currentY + 3), static_cast<int>(barWidth), static_cast<int>(barHeight),
                          Fade(DARKGRAY, 0.5f));
            // Remplissage de la jauge
            DrawRectangle(static_cast<int>(barX), static_cast<int>(currentY + 3), static_cast<int>(barWidth * ratio),
                          static_cast<int>(barHeight), line.color);

            // Texte par dessus la jauge
            std::string valText = TextFormat("%.0f/%.0f", line.currentVal, line.maxVal);
            int valWidth = MeasureText(valText.c_str(), 16);
            DrawText(valText.c_str(), static_cast<int>(barX + (barWidth / 2) - (valWidth / 2)), static_cast<int>(currentY + 3), 16, WHITE);

            currentY += baseLineHeight;
        } else { // Texte Normal
            DrawText(line.label.c_str(), static_cast<int>(boxX + padding + 15.0f), static_cast<int>(currentY), 18, line.color);
            currentY += baseLineHeight;
        }
    }
}

} // namespace

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
