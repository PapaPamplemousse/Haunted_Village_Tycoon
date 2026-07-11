#include "systems/WorldRenderSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

void WorldRenderSystem::Render(const EntityManager& entityManager, const WorldMap& worldMap, const TileRegistry& tileRegistry,
                               const Camera2D& camera, int hoverX, int hoverY, bool showNames) const {
    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    const int startX = std::max(0, static_cast<int>(topLeft.x / Config::TILE_SIZE) - 1);
    const int startY = std::max(0, static_cast<int>(topLeft.y / Config::TILE_SIZE) - 1);
    const int endX = std::min(worldMap.GetWidth(), static_cast<int>(bottomRight.x / Config::TILE_SIZE) + 1);
    const int endY = std::min(worldMap.GetHeight(), static_cast<int>(bottomRight.y / Config::TILE_SIZE) + 1);

    // ==========================================================
    // TERRAIN
    // ==========================================================
    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const int tileId = worldMap.GetTile(x, y);
            const TileDef* def = tileRegistry.GetTileDef(tileId);
            const Color color = def ? def->color : MAGENTA;

            DrawRectangle(x * Config::TILE_SIZE, y * Config::TILE_SIZE, Config::TILE_SIZE - 1, Config::TILE_SIZE - 1, color);
        }
    }

    // ==========================================================
    // HOVER TILE
    // ==========================================================
    if (hoverX >= 0 && hoverX < worldMap.GetWidth() && hoverY >= 0 && hoverY < worldMap.GetHeight()) {
        DrawRectangle(hoverX * Config::TILE_SIZE, hoverY * Config::TILE_SIZE, Config::TILE_SIZE, Config::TILE_SIZE,
                      ColorAlpha(WHITE, 0.3f));
    }

    // ==========================================================
    // ROOM FLOORS
    // ==========================================================
    for (size_t i = 0; i < entityManager.active.size(); ++i) {
        if (!entityManager.active[i] || !entityManager.hasRoom[i]) {
            continue;
        }

        const auto& room = entityManager.rooms[i];
        const Color roomTint = (room.structureId == "EMPTY_ROOM") ? ColorAlpha(GRAY, 0.3f) : ColorAlpha(BLUE, 0.3f);

        for (const Vector2& tile : room.floorTiles) {
            if (tile.x < startX || tile.x > endX || tile.y < startY || tile.y > endY) {
                continue;
            }

            DrawRectangle(static_cast<int>(tile.x * Config::TILE_SIZE), static_cast<int>(tile.y * Config::TILE_SIZE), Config::TILE_SIZE,
                          Config::TILE_SIZE, roomTint);
        }
    }
}

void WorldRenderSystem::ShowName(const EntityManager& entityManager, const Camera2D& camera, bool showNames) const {
    // ==========================================================
    // ROOM LABELS, TAB
    // ==========================================================
    if (!showNames) {
        return;
    }

    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    for (size_t i = 0; i < entityManager.active.size(); ++i) {
        if (!entityManager.active[i] || !entityManager.hasRoom[i]) {
            continue;
        }

        const auto& room = entityManager.rooms[i];

        if (room.floorTiles.empty()) {
            continue;
        }

        float sumX = 0.0f;
        float sumY = 0.0f;

        for (const Vector2& tile : room.floorTiles) {
            sumX += tile.x;
            sumY += tile.y;
        }

        const float centerX = (sumX / static_cast<float>(room.floorTiles.size())) * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f);
        const float centerY = (sumY / static_cast<float>(room.floorTiles.size())) * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f);

        if (centerX < topLeft.x || centerX > bottomRight.x || centerY < topLeft.y || centerY > bottomRight.y) {
            continue;
        }

        const std::string roomName = room.name;
        const int nameFontSize = 30;
        const int jobFontSize = 20;
        const int padding = 6;
        const int lineSpacing = 4;

        int maxWidth = MeasureText(roomName.c_str(), nameFontSize);
        int totalHeight = nameFontSize;

        std::vector<std::string> jobLines;

        if (entityManager.hasWorkplace[i]) {
            const auto& workplace = entityManager.workplaces[i];
            std::unordered_map<std::string, std::pair<int, int>> slots;

            for (const auto& slot : workplace.slots) {
                slots[slot.profession].second++;

                if (slot.workerId != static_cast<EntityID>(-1)) {
                    slots[slot.profession].first++;
                }
            }

            for (const auto& pair : slots) {
                const std::string jobStr =
                    pair.first + " : " + std::to_string(pair.second.first) + " / " + std::to_string(pair.second.second);
                jobLines.push_back(jobStr);

                const int jobWidth = MeasureText(jobStr.c_str(), jobFontSize);
                if (jobWidth > maxWidth) {
                    maxWidth = jobWidth;
                }

                totalHeight += jobFontSize + lineSpacing;
            }
        }

        const float boxX = centerX - maxWidth / 2.0f;
        const float boxY = centerY - totalHeight / 2.0f;

        const Color textColor = (room.structureId == "EMPTY_ROOM") ? LIGHTGRAY : GOLD;

        DrawRectangle(boxX - padding, boxY - padding, maxWidth + padding * 2, totalHeight + padding * 2, ColorAlpha(BLACK, 0.8f));

        const int roomNameWidth = MeasureText(roomName.c_str(), nameFontSize);

        DrawText(roomName.c_str(), static_cast<int>(boxX + (maxWidth - roomNameWidth) / 2.0f), static_cast<int>(boxY), nameFontSize,
                 textColor);

        float currentY = boxY + nameFontSize + lineSpacing;

        for (const std::string& jobStr : jobLines) {
            const int jobWidth = MeasureText(jobStr.c_str(), jobFontSize);
            DrawText(jobStr.c_str(), static_cast<int>(boxX + (maxWidth - jobWidth) / 2.0f), static_cast<int>(currentY), jobFontSize,
                     LIGHTGRAY);
            currentY += jobFontSize + lineSpacing;
        }
    }
}
