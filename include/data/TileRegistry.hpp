/**
 * @file TileRegistry.hpp
 * @brief Parses and stores world map tile definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>

/**
 * @struct TileDef
 * @brief Blueprint of a map tile loaded from tiles.stv.
 */
struct TileDef {
    int id = -1;
    std::string name;
    bool walkable = true;
    Color color = MAGENTA;

    // Optional texture / spritesheet rendering.
    std::string texturePath = "";

    bool useSpriteSheet = false;
    SpriteSheetMode spriteSheetMode = SpriteSheetMode::None;

    int spriteSheetColumns = 1;
    int spriteSheetRows = 1;

    float spriteFrameWidth = 0.0f;
    float spriteFrameHeight = 0.0f;

    float spriteColumnGap = 0.0f;
    float spriteRowGap = 0.0f;
};

/**
 * @class TileRegistry
 * @brief Parses and stores all tile definitions.
 */
class TileRegistry {
public:
    TileRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    const TileDef* GetTileDef(int id) const;
    int GetTileIdByString(const std::string& stringId) const;

    const std::unordered_map<int, TileDef>& GetAllTiles() const {
        return m_tiles;
    }

private:
    std::unordered_map<int, TileDef> m_tiles;
    std::unordered_map<std::string, int> m_stringToIdMap;
};
