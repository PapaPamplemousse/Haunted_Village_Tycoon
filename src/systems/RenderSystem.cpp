/**
 * @file RenderSystem.cpp
 * @brief Implementation of entity rendering.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/RenderSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int WorldToTile(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

std::string GetDisplayName(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity]) {
        return "Unknown";
    }

    const TagComponent& tag = em.tags[entity];

    if (!tag.firstName.empty()) {
        return tag.firstName;
    }

    if (!tag.name.empty()) {
        return tag.name;
    }

    return "Entity #" + std::to_string(entity);
}

bool IsRoomTile(const RoomComponent& room, int tileX, int tileY) {
    for (const Vector2& tile : room.floorTiles) {
        if (static_cast<int>(tile.x) == tileX && static_cast<int>(tile.y) == tileY) {
            return true;
        }
    }

    return false;
}

bool IsEntityInsideRoom(const EntityManager& em, EntityID entity, const RoomComponent& room) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity]) {
        return false;
    }

    const int tileX = WorldToTile(em.transforms[entity].position.x);
    const int tileY = WorldToTile(em.transforms[entity].position.y);

    return IsRoomTile(room, tileX, tileY);
}

Vector2 GetRoomCenterWorld(const RoomComponent& room) {
    if (room.floorTiles.empty()) {
        return {0.0f, 0.0f};
    }

    float sumX = 0.0f;
    float sumY = 0.0f;

    for (const Vector2& tile : room.floorTiles) {
        sumX += tile.x;
        sumY += tile.y;
    }

    const float invCount = 1.0f / static_cast<float>(room.floorTiles.size());

    const float centerTileX = sumX * invCount;
    const float centerTileY = sumY * invCount;

    return {centerTileX * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f, centerTileY * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

int CountBedsInsideRoom(const EntityManager& em, const RoomComponent& room) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (entity >= em.active.size() || !em.active[entity] || !em.hasRestSpot[entity] || !em.hasTransform[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        if (!IsEntityInsideRoom(em, entity, room)) {
            continue;
        }

        capacity += std::max(0, em.restSpots[entity].capacity);
    }

    return capacity;
}

int CountActiveChildrenOfCouple(const EntityManager& em, EntityID parentA, EntityID parentB) {
    if (parentA >= em.active.size() || !em.active[parentA] || !em.hasFamily[parentA]) {
        return 0;
    }

    int count = 0;

    for (EntityID child : em.families[parentA].children) {
        if (child >= em.active.size() || !em.active[child] || !em.hasFamily[child]) {
            continue;
        }

        const FamilyComponent& childFamily = em.families[child];

        const bool sameParents = (childFamily.parentA == parentA && childFamily.parentB == parentB) ||
                                 (childFamily.parentA == parentB && childFamily.parentB == parentA);

        if (sameParents) {
            count++;
        }
    }

    return count;
}

std::string GetFamilyOwnerText(const EntityManager& em, EntityID familyKey) {
    if (familyKey == static_cast<EntityID>(-1)) {
        return "Owner: none";
    }

    if (familyKey >= em.active.size() || !em.active[familyKey] || !em.hasFamily[familyKey]) {
        return "Owner: family #" + std::to_string(familyKey);
    }

    const EntityID partner = em.families[familyKey].partnerId;

    if (partner < em.active.size() && em.active[partner]) {
        const int childCount = CountActiveChildrenOfCouple(em, familyKey, partner);

        return "Owner: " + GetDisplayName(em, familyKey) + " + " + GetDisplayName(em, partner) +
               " | children: " + std::to_string(childCount);
    }

    return "Owner: family #" + std::to_string(familyKey);
}

void DrawWorldLabel(const std::vector<std::string>& lines, Vector2 position, int fontSize, Color mainColor) {
    if (lines.empty()) {
        return;
    }

    int maxWidth = 0;

    for (const std::string& line : lines) {
        maxWidth = std::max(maxWidth, MeasureText(line.c_str(), fontSize));
    }

    const int padding = 4;
    const int lineHeight = fontSize + 2;
    const int boxHeight = static_cast<int>(lines.size()) * lineHeight + padding * 2;

    const float x = position.x - static_cast<float>(maxWidth) * 0.5f;
    const float y = position.y;

    DrawRectangle(static_cast<int>(x - padding), static_cast<int>(y - padding), maxWidth + padding * 2, boxHeight,
                  ColorAlpha(BLACK, 0.72f));

    for (size_t i = 0; i < lines.size(); ++i) {
        const Color color = (i == 0) ? mainColor : LIGHTGRAY;

        DrawText(lines[i].c_str(), static_cast<int>(x), static_cast<int>(y + static_cast<float>(i * lineHeight)), fontSize, color);
    }
}

struct WorldLabel {
    std::vector<std::string> lines;
    Vector2 position = {0.0f, 0.0f};
    int fontSize = 16;
    Color mainColor = RAYWHITE;
};

Rectangle MeasureWorldLabelRect(const WorldLabel& label) {
    int maxWidth = 0;

    for (const std::string& line : label.lines) {
        maxWidth = std::max(maxWidth, MeasureText(line.c_str(), label.fontSize));
    }

    const int padding = 4;
    const int lineHeight = label.fontSize + 2;
    const int boxHeight = static_cast<int>(label.lines.size()) * lineHeight + padding * 2;

    return {label.position.x - static_cast<float>(maxWidth) * 0.5f - static_cast<float>(padding),
            label.position.y - static_cast<float>(padding), static_cast<float>(maxWidth + padding * 2), static_cast<float>(boxHeight)};
}

bool RectanglesOverlap(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}

void ResolveLabelOverlaps(std::vector<WorldLabel>& labels) {
    std::vector<Rectangle> placedRects;

    constexpr int MAX_ATTEMPTS = 12;
    constexpr float VERTICAL_STEP = 22.0f;

    for (WorldLabel& label : labels) {
        Vector2 basePosition = label.position;

        for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
            label.position = {basePosition.x, basePosition.y + static_cast<float>(attempt) * VERTICAL_STEP};

            Rectangle currentRect = MeasureWorldLabelRect(label);

            bool overlaps = false;

            for (const Rectangle& placed : placedRects) {
                if (RectanglesOverlap(currentRect, placed)) {
                    overlaps = true;
                    break;
                }
            }

            if (!overlaps) {
                placedRects.push_back(currentRect);
                break;
            }

            if (attempt == MAX_ATTEMPTS - 1) {
                placedRects.push_back(currentRect);
            }
        }
    }
}

void DrawResolvedWorldLabels(const std::vector<WorldLabel>& labels) {
    for (const WorldLabel& label : labels) {
        DrawWorldLabel(label.lines, label.position, label.fontSize, label.mainColor);
    }
}

} // namespace

void RenderSystem::Render(const EntityManager& em, const EntitySpatialGrid& spatialGrid, const Camera2D& camera, bool showNames) const {
    double time = GetTime();

    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    const float margin = static_cast<float>(Config::RENDER_ENTITY_MARGIN_TILES) * Config::TILE_SIZE;

    topLeft.x -= margin;
    topLeft.y -= margin;
    bottomRight.x += margin;
    bottomRight.y += margin;

    Rectangle visibleRect = {topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};

    const std::vector<EntityID> visibleEntities = spatialGrid.GetEntitiesInRect(visibleRect, em);

    // ==========================================
    // PASS 1 : DESSINER LA GEOMETRIE
    // ==========================================
    for (EntityID i : visibleEntities) {
        if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i] || !em.hasSprite[i]) {
            continue;
        }

        Vector2 drawPos = em.transforms[i].position;
        const auto& sprite = em.sprites[i];

        if (sprite.isAnimated) {
            float bounceOffset = std::sin(time * 5.0f + static_cast<float>(i)) * 3.0f;
            drawPos.y += bounceOffset;
        }

        if (sprite.texturePath == "square") {
            Rectangle rec = {drawPos.x - sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f, sprite.width, sprite.height};

            DrawRectangleRec(rec, sprite.tint);
            DrawRectangleLinesEx(rec, 1.0f, BLACK);
        } else if (sprite.texturePath == "circle") {
            DrawCircleV(drawPos, sprite.width / 2.0f, sprite.tint);
            DrawCircleLines(drawPos.x, drawPos.y, sprite.width / 2.0f, BLACK);
        } else if (sprite.texturePath == "triangle") {
            Vector2 v1 = {drawPos.x, drawPos.y - sprite.height / 2.0f};
            Vector2 v2 = {drawPos.x - sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            Vector2 v3 = {drawPos.x + sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};

            DrawTriangle(v1, v2, v3, sprite.tint);
            DrawTriangleLines(v1, v2, v3, BLACK);
        } else {
            DrawRectangle(drawPos.x, drawPos.y, sprite.width, sprite.height, MAGENTA);
        }

        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            DrawPoly(drawPos, 6, sprite.width / 1.5f, time * 50.0f, ColorAlpha(YELLOW, 0.6f));
        }

        if (em.hasDeconstruct[i]) {
            Vector2 p1 = {drawPos.x - sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f};
            Vector2 p2 = {drawPos.x + sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            Vector2 p3 = {drawPos.x + sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f};
            Vector2 p4 = {drawPos.x - sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};

            DrawLineEx(p1, p2, 4.0f, RED);
            DrawLineEx(p3, p4, 4.0f, RED);
        }
    }

    // ==========================================
    // PASS 2 : DESSINER LES NOMS ET INFOS (TAB)
    // ==========================================
    if (showNames) {
        // 2.1 Entity labels: name + species/category.
        for (EntityID i : visibleEntities) {
            if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i] || !em.hasTag[i] || !em.hasStats[i]) {
                continue;
            }

            const Vector2 drawPos = em.transforms[i].position;
            const TagComponent& tag = em.tags[i];

            const std::string displayName = !tag.firstName.empty() ? tag.firstName : tag.name;

            if (displayName.empty()) {
                continue;
            }

            std::string subLine;

            if (!tag.species.empty()) {
                subLine = tag.species;
            }

            if (!tag.category.empty()) {
                if (!subLine.empty()) {
                    subLine += " | ";
                }

                subLine += tag.category;
            }

            std::vector<std::string> lines;
            lines.push_back(displayName);

            if (!subLine.empty()) {
                lines.push_back(subLine);
            }

            DrawWorldLabel(lines, {drawPos.x, drawPos.y - 34.0f}, 18, RAYWHITE);
        }

        // 2.2 Room labels: housing / bedroom / workplace ownership.
        // Labels are collected first, then shifted to avoid overlap.
        std::vector<WorldLabel> roomLabels;

        for (EntityID roomId = 0; roomId < em.active.size(); ++roomId) {
            if (roomId >= em.active.size() || !em.active[roomId] || !em.hasRoom[roomId]) {
                continue;
            }

            const RoomComponent& room = em.rooms[roomId];
            const bool hasWorkplace = em.hasWorkplace[roomId];

            if (!room.isHousing && !room.isBedroom && !room.isPrivate && !hasWorkplace) {
                continue;
            }

            const Vector2 roomCenter = GetRoomCenterWorld(room);

            std::vector<std::string> lines;
            lines.push_back(room.name);

            // ---------------------------------------------------------
            // Workplace rooms: show job slot occupancy.
            // ---------------------------------------------------------
            if (hasWorkplace) {
                const WorkplaceComponent& workplace = em.workplaces[roomId];

                std::unordered_map<std::string, std::pair<int, int>> slots;

                for (const JobSlot& slot : workplace.slots) {
                    slots[slot.profession].second++;

                    if (slot.workerId != static_cast<EntityID>(-1)) {
                        slots[slot.profession].first++;
                    }
                }

                for (const auto& pair : slots) {
                    lines.push_back(pair.first + " : " + std::to_string(pair.second.first) + " / " + std::to_string(pair.second.second));
                }

                roomLabels.push_back({lines, {roomCenter.x, roomCenter.y + 18.0f}, 16, GOLD});

                continue;
            }

            // ---------------------------------------------------------
            // Housing / bedroom rooms: show family ownership and beds.
            // ---------------------------------------------------------
            const int bedCapacity = CountBedsInsideRoom(em, room);

            if (room.ownerFamilyId != static_cast<EntityID>(-1)) {
                lines.push_back(GetFamilyOwnerText(em, room.ownerFamilyId));
            } else if (room.ownerVillageId != static_cast<EntityID>(-1)) {
                lines.push_back("Owner: village #" + std::to_string(room.ownerVillageId));
            } else if (room.isBedroom) {
                lines.push_back("Owner: unassigned bedroom");
            } else {
                lines.push_back("Owner: public");
            }

            lines.push_back("Beds: " + std::to_string(bedCapacity));

            roomLabels.push_back({lines, {roomCenter.x, roomCenter.y + 18.0f}, 16, room.structureId == "EMPTY_ROOM" ? LIGHTGRAY : GOLD});
        }

        ResolveLabelOverlaps(roomLabels);
        DrawResolvedWorldLabels(roomLabels);
    }
}
