/**
 * @file TileRegistry.cpp
 * @brief Implementation of the TileRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/TileRegistry.hpp"

#include "data/STVParser.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool ParseBool(const std::string& value) {
    const std::string trimmed = Trim(value);

    return trimmed == "true" || trimmed == "1" || trimmed == "yes" || trimmed == "on";
}

Color ParseColor(const std::string& value) {
    Color c = {255, 255, 255, 255};

    std::stringstream ss(value);
    std::string token;
    int i = 0;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (i == 0) {
            c.r = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 1) {
            c.g = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 2) {
            c.b = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 3) {
            c.a = static_cast<unsigned char>(std::stoi(token));
        }

        i++;
    }

    return c;
}

SpriteSheetMode ParseSpriteSheetMode(const std::string& value) {
    const std::string mode = Trim(value);

    if (mode == "directional_action") {
        return SpriteSheetMode::DirectionalAction;
    }

    if (mode == "furniture_state") {
        return SpriteSheetMode::FurnitureState;
    }

    if (mode == "seasonal") {
        return SpriteSheetMode::Seasonal;
    }

    return SpriteSheetMode::None;
}

void ParseFrameSize(const std::string& value, float& width, float& height) {
    std::stringstream ss(value);
    std::string token;

    if (std::getline(ss, token, ',')) {
        width = std::stof(Trim(token));
    }

    if (std::getline(ss, token, ',')) {
        height = std::stof(Trim(token));
    }
}

} // namespace

bool TileRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        TileDef def;

        if (block.properties.count("id")) {
            def.id = std::stoi(Trim(block.properties.at("id")));
        }

        if (block.properties.count("name")) {
            def.name = Trim(block.properties.at("name"));
        }

        if (block.properties.count("walkable")) {
            def.walkable = ParseBool(block.properties.at("walkable"));
        }

        if (block.properties.count("color")) {
            def.color = ParseColor(block.properties.at("color"));
        } else {
            def.color = MAGENTA;
        }

        // ---------------------------------------------------------
        // Optional texture / spritesheet rendering
        // ---------------------------------------------------------
        if (block.properties.count("texture")) {
            def.texturePath = Trim(block.properties.at("texture"));
        }

        if (block.properties.count("sprite_mode")) {
            def.spriteSheetMode = ParseSpriteSheetMode(block.properties.at("sprite_mode"));
            def.useSpriteSheet = def.spriteSheetMode != SpriteSheetMode::None;
        }

        if (block.properties.count("sprite_sheet_columns")) {
            def.spriteSheetColumns = std::max(1, std::stoi(Trim(block.properties.at("sprite_sheet_columns"))));
        }

        if (block.properties.count("sprite_sheet_rows")) {
            def.spriteSheetRows = std::max(1, std::stoi(Trim(block.properties.at("sprite_sheet_rows"))));
        }

        if (block.properties.count("sprite_column_gap")) {
            def.spriteColumnGap = std::stof(Trim(block.properties.at("sprite_column_gap")));
        }

        if (block.properties.count("sprite_row_gap")) {
            def.spriteRowGap = std::stof(Trim(block.properties.at("sprite_row_gap")));
        }

        if (block.properties.count("sprite_frame_size")) {
            ParseFrameSize(block.properties.at("sprite_frame_size"), def.spriteFrameWidth, def.spriteFrameHeight);
        }

        m_tiles[def.id] = def;
        m_stringToIdMap[block.id] = def.id;
    }

    std::cout << "[INFO] Loaded " << m_tiles.size() << " tiles." << std::endl;
    return true;
}

const TileDef* TileRegistry::GetTileDef(int id) const {
    auto it = m_tiles.find(id);
    return it != m_tiles.end() ? &(it->second) : nullptr;
}

int TileRegistry::GetTileIdByString(const std::string& stringId) const {
    auto it = m_stringToIdMap.find(stringId);
    return it != m_stringToIdMap.end() ? it->second : -1;
}
