/**
 * @file WorldRenderSystem.cpp
 * @brief Implementation of world and terrain rendering.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/WorldRenderSystem.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

int GetTileSpriteColumn(const TileDef& tile, int seasonIndex) {
    if (!tile.useSpriteSheet) {
        return 0;
    }

    if (tile.spriteSheetMode == SpriteSheetMode::Seasonal) {
        return std::max(0, std::min(tile.spriteSheetColumns - 1, seasonIndex));
    }

    return 0;
}

int GetTileSpriteRow(const TileDef&) {
    return 0;
}

void DrawTileFallback(int x, int y, Color color) {
    DrawRectangle(x * Config::TILE_SIZE, y * Config::TILE_SIZE, Config::TILE_SIZE - 1, Config::TILE_SIZE - 1, color);
}

} // namespace

void WorldRenderSystem::Render(const EntityManager& entityManager, const WorldMap& worldMap, const TileRegistry& tileRegistry,
                               const Camera2D& camera, int hoverX, int hoverY, bool, int seasonIndex, TextureCache& textureCache) const {
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

            if (def == nullptr) {
                DrawTileFallback(x, y, MAGENTA);
                continue;
            }

            const Texture2D* texture = textureCache.GetTexture(def->texturePath);

            if (texture == nullptr || !def->useSpriteSheet) {
                DrawTileFallback(x, y, def->color);
                continue;
            }

            const int columns = std::max(1, def->spriteSheetColumns);
            const int rows = std::max(1, def->spriteSheetRows);

            const float frameWidth = def->spriteFrameWidth > 0.0f
                                         ? def->spriteFrameWidth
                                         : (static_cast<float>(texture->width) - def->spriteColumnGap * static_cast<float>(columns - 1)) /
                                               static_cast<float>(columns);

            const float frameHeight =
                def->spriteFrameHeight > 0.0f
                    ? def->spriteFrameHeight
                    : (static_cast<float>(texture->height) - def->spriteRowGap * static_cast<float>(rows - 1)) / static_cast<float>(rows);

            const int col = std::max(0, std::min(columns - 1, GetTileSpriteColumn(*def, seasonIndex)));
            const int row = std::max(0, std::min(rows - 1, GetTileSpriteRow(*def)));

            const Rectangle source = {static_cast<float>(col) * (frameWidth + def->spriteColumnGap),
                                      static_cast<float>(row) * (frameHeight + def->spriteRowGap), frameWidth, frameHeight};

            const Rectangle destination = {static_cast<float>(x * Config::TILE_SIZE), static_cast<float>(y * Config::TILE_SIZE),
                                           static_cast<float>(Config::TILE_SIZE), static_cast<float>(Config::TILE_SIZE)};

            DrawTexturePro(*texture, source, destination, {0.0f, 0.0f}, 0.0f, WHITE);
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
