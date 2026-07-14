/**
 * @file InspectionSystem.cpp
 * @brief Implementation of the debug entity inspector rendering.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "debug/InspectionSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
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

int CountInventoryItems(const InventoryComponent& inventory) {
    int total = 0;

    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            total += item.second;
        }
    }

    return total;
}

int CountDistinctInventoryItems(const InventoryComponent& inventory) {
    int total = 0;

    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            total++;
        }
    }

    return total;
}

std::string FormatInventoryItem(const std::string& itemId, int count, const ResourceRegistry& resourceReg) {
    std::string text = itemId + " x" + std::to_string(count);

    const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

    if (resource != nullptr && resource->isConsumable && resource->nutrition > 0.0f) {
        const int totalNutrition = static_cast<int>(resource->nutrition * static_cast<float>(count));
        text += " | food " + std::to_string(totalNutrition);
    }

    return text;
}

std::vector<TooltipLine> BuildInspectionLines(EntityID i, const EntityManager& entityManager, const ResourceRegistry& resourceReg) {
    std::vector<TooltipLine> lines;

    // =========================================================
    // Identity
    // =========================================================
    std::string title = "Entity #" + std::to_string(i);

    if (entityManager.hasTag[i]) {
        const TagComponent& tag = entityManager.tags[i];

        title = tag.firstName.empty() ? tag.name : tag.firstName;

        AddTitle(lines, title);

        if (!tag.species.empty()) {
            AddKV(lines, "Species", tag.species);
        }

        if (tag.age > 0) {
            AddKV(lines, "Age", std::to_string(tag.age));
        }
    } else {
        AddTitle(lines, title);
    }

    if (entityManager.hasTransform[i]) {
        const int tileX = WorldToTile(entityManager.transforms[i].position.x);
        const int tileY = WorldToTile(entityManager.transforms[i].position.y);

        AddKV(lines, "Tile", std::to_string(tileX) + ", " + std::to_string(tileY));
    }

    // =========================================================
    // Faction
    // =========================================================
    if (entityManager.hasFaction[i]) {
        const FactionComponent& faction = entityManager.factions[i];

        AddHeader(lines, "Faction");
        AddKV(lines, "Faction", faction.factionId);
        AddKV(lines, "Conviction", TextFormat("%.0f", faction.conviction));
        AddKV(lines, "Openness", TextFormat("%.2f", faction.openness));
    }

    // =========================================================
    // Vitals
    // =========================================================
    if (entityManager.hasHealth[i] || entityManager.hasNeeds[i]) {
        AddHeader(lines, "Status");

        if (entityManager.hasHealth[i]) {
            AddBar(lines, "HP", entityManager.healths[i].current, entityManager.healths[i].max, RED);
        }

        if (entityManager.hasNeeds[i]) {
            AddBar(lines, "Hunger", entityManager.needs[i].hunger, entityManager.needs[i].maxHunger, ORANGE);
            AddBar(lines, "Fatigue", entityManager.needs[i].fatigue, entityManager.needs[i].maxFatigue, SKYBLUE);

            if (entityManager.needs[i].collapsedFromFatigue) {
                AddText(lines, "Collapsed from fatigue", RED);
            }
        }
    }

    // =========================================================
    // AI / job
    // =========================================================
    if (entityManager.hasProfession[i] || entityManager.hasBehavior[i]) {
        AddHeader(lines, "AI");

        if (entityManager.hasProfession[i]) {
            const ProfessionComponent& profession = entityManager.professions[i];

            AddKV(lines, "Job", profession.currentProfession);

            const std::string mode = profession.assignmentMode == ProfessionAssignmentMode::Manual ? "manual" : "auto";

            AddKV(lines, "Mode", mode);
        }

        if (entityManager.hasBehavior[i]) {
            const BehaviorComponent& behavior = entityManager.behaviors[i];

            AddKV(lines, "Task", behavior.currentTask);

            if (behavior.currentJobTarget != 0 && behavior.currentJobTarget < entityManager.active.size() &&
                entityManager.active[behavior.currentJobTarget]) {
                AddKV(lines, "Target", "#" + std::to_string(behavior.currentJobTarget));
            }
        }
    }

    // =========================================================
    // Personality
    // =========================================================
    if (entityManager.hasPersonality[i]) {
        const PersonalityComponent& personality = entityManager.personalities[i];

        if (!personality.traits.empty()) {
            AddHeader(lines, "Personality");

            std::string traitsText;

            for (size_t t = 0; t < personality.traits.size(); ++t) {
                if (t > 0) {
                    traitsText += ", ";
                }

                traitsText += personality.traits[t];
            }

            AddText(lines, traitsText, LIGHTGRAY);

            AddKV(lines, "Kindness", TextFormat("%.2f", personality.kindness));
            AddKV(lines, "Aggression", TextFormat("%.2f", personality.aggression));
            AddKV(lines, "Bravery", TextFormat("%.2f", personality.bravery));
        }
    }

    // =========================================================
    // Inventory / storage summary
    // =========================================================
    if (entityManager.hasInventory[i]) {
        const InventoryComponent& inventory = entityManager.inventories[i];

        const int itemCount = CountInventoryItems(inventory);
        const int distinctItems = CountDistinctInventoryItems(inventory);

        if (itemCount > 0 || entityManager.hasStorage[i]) {
            AddHeader(lines, "Inventory");

            if (entityManager.hasStorage[i]) {
                const int capacity = entityManager.storages[i].capacity;
                AddKV(lines, "Capacity", std::to_string(itemCount) + " / " + std::to_string(capacity));
            } else {
                AddKV(lines, "Items", std::to_string(itemCount));
            }

            AddKV(lines, "Types", std::to_string(distinctItems));

            constexpr int MAX_DISPLAYED_ITEMS = 6;

            int displayedItems = 0;
            int hiddenItems = 0;

            for (const auto& item : inventory.items) {
                const std::string& itemId = item.first;
                const int count = item.second;

                if (count <= 0) {
                    continue;
                }

                if (displayedItems < MAX_DISPLAYED_ITEMS) {
                    AddKV(lines, itemId, "x" + std::to_string(count));

                    const ResourceDef* resource = resourceReg.GetResourceDef(itemId);

                    if (resource != nullptr && resource->isConsumable && resource->nutrition > 0.0f) {
                        const int totalNutrition = static_cast<int>(resource->nutrition * static_cast<float>(count));
                        AddText(lines, "  food +" + std::to_string(totalNutrition), DARKGREEN);
                    }

                    displayedItems++;
                } else {
                    hiddenItems++;
                }
            }

            if (hiddenItems > 0) {
                AddText(lines, "... +" + std::to_string(hiddenItems) + " more item types", GRAY);
            }
        }
    }

    // =========================================================
    // Equipment
    // =========================================================
    if (entityManager.hasEquipment[i]) {
        const EquipmentComponent& equipment = entityManager.equipments[i];

        if (!equipment.rightHandItemId.empty() || equipment.rightHandDamage > 0.0f) {
            AddHeader(lines, "Equipment");

            if (!equipment.rightHandItemId.empty()) {
                AddKV(lines, "Right Hand", equipment.rightHandItemId);
            }

            if (!equipment.equipmentSlot.empty()) {
                AddKV(lines, "Slot", equipment.equipmentSlot);
            }

            AddKV(lines, "Tool Type", equipment.rightHandToolType);
            AddKV(lines, "Damage", TextFormat("%.0f", equipment.rightHandDamage));
        }
    }

    // =========================================================
    // Furniture / special object summary
    // =========================================================
    if (entityManager.hasRestSpot[i] || entityManager.hasDoor[i] || entityManager.hasConstruction[i]) {
        AddHeader(lines, "Object");

        if (entityManager.hasRestSpot[i]) {
            const RestSpotComponent& rest = entityManager.restSpots[i];

            AddKV(lines, "Bed", std::to_string(rest.occupants.size()) + " / " + std::to_string(rest.capacity));

            if (rest.ownerFamilyId != static_cast<EntityID>(-1)) {
                AddKV(lines, "Owner", "family #" + std::to_string(rest.ownerFamilyId));
            } else if (rest.ownerVillageId != static_cast<EntityID>(-1)) {
                AddKV(lines, "Owner", "village #" + std::to_string(rest.ownerVillageId));
            } else {
                AddKV(lines, "Owner", rest.isPrivate ? "private" : "public");
            }
        }

        if (entityManager.hasDoor[i]) {
            AddKV(lines, "Door", DoorStateToString(entityManager.doors[i].state));
        }

        if (entityManager.hasConstruction[i]) {
            const ConstructionComponent& construction = entityManager.constructions[i];

            if (construction.isDoor) {
                AddKV(lines, "Type", "door");
            } else if (construction.isWall) {
                AddKV(lines, "Type", "wall");
            }
        }
    }

    return lines;
}

void DrawTooltipBox(const std::vector<TooltipLine>& lines) {
    if (lines.empty())
        return;

    const float boxWidth = 300.0f;
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
                              const ResourceRegistry& resourceReg) const {
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
