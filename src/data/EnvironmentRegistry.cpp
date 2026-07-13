/**
 * @file EnvironmentRegistry.cpp
 * @brief Implementation of the EnvironmentRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/EnvironmentRegistry.hpp"

#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

std::vector<DropEntry> ParseDrops(const std::string& value) {
    std::vector<DropEntry> drops;

    std::stringstream ss(value);
    std::string dropToken;

    while (std::getline(ss, dropToken, ',')) {
        std::stringstream dropSS(dropToken);

        std::string itemId;
        std::string amount;
        std::string chance;

        if (std::getline(dropSS, itemId, ':') && std::getline(dropSS, amount, ':') && std::getline(dropSS, chance, ':')) {
            DropEntry drop;
            drop.itemId = Trim(itemId);
            drop.amount = std::stoi(Trim(amount));
            drop.chance = std::stof(Trim(chance));

            if (!drop.itemId.empty() && drop.amount > 0) {
                drops.push_back(drop);
            }
        }
    }

    return drops;
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

void ParseSpriteSize(const std::string& value, float& width, float& height) {
    std::stringstream ss(value);
    std::string token;

    if (std::getline(ss, token, ',')) {
        width = std::stof(Trim(token));
    }

    if (std::getline(ss, token, ',')) {
        height = std::stof(Trim(token));
    }
}

void ParseSpriteFrameSize(const std::string& value, float& width, float& height) {
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

bool EnvironmentRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        EnvironmentDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        }

        if (block.properties.count("max_hp")) {
            def.maxHp = std::stof(Trim(block.properties.at("max_hp")));
        }

        if (block.properties.count("is_obstacle")) {
            def.isObstacle = ParseBool(block.properties.at("is_obstacle"));
        }

        if (block.properties.count("harvest_tool")) {
            def.harvestTool = Trim(block.properties.at("harvest_tool"));
        }

        if (block.properties.count("drops")) {
            def.drops = ParseDrops(block.properties.at("drops"));
        }

        // ---------------------------------------------------------
        // Graphics
        // ---------------------------------------------------------
        if (block.properties.count("texture")) {
            def.texturePath = Trim(block.properties.at("texture"));
        }

        if (block.properties.count("color")) {
            def.color = ParseColor(block.properties.at("color"));
        }

        if (block.properties.count("sprite_size")) {
            ParseSpriteSize(block.properties.at("sprite_size"), def.spriteWidth, def.spriteHeight);
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
            ParseSpriteFrameSize(block.properties.at("sprite_frame_size"), def.spriteFrameWidth, def.spriteFrameHeight);
        }

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " environment definitions." << std::endl;

    return true;
}

EntityID EnvironmentRegistry::SpawnEnvironment(EntityManager& em, const std::string& prefabId, Vector2 position) const {
    auto it = m_templates.find(prefabId);

    if (it == m_templates.end()) {
        std::cerr << "[WARNING] Cannot spawn unknown environment prefab: " << prefabId << std::endl;
        return 0;
    }

    const EnvironmentDef& def = it->second;
    EntityID id = em.CreateEntity();

    // 1. Identity
    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id};

    // 2. Health
    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    // 3. Placement
    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    // 4. Harvestable resource
    em.hasHarvestable[id] = true;
    em.harvestables[id] = {def.harvestTool, def.drops};

    // 5. Obstacle / construction collision
    if (def.isObstacle) {
        em.hasConstruction[id] = true;
        em.constructions[id] = {true, false};
    }

    // 6. Visual representation
    em.hasSprite[id] = true;
    em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, false};

    em.sprites[id].useSpriteSheet = def.useSpriteSheet;
    em.sprites[id].sheetMode = def.spriteSheetMode;
    em.sprites[id].sheetColumns = std::max(1, def.spriteSheetColumns);
    em.sprites[id].sheetRows = std::max(1, def.spriteSheetRows);
    em.sprites[id].frameWidth = def.spriteFrameWidth;
    em.sprites[id].frameHeight = def.spriteFrameHeight;
    em.sprites[id].columnGap = def.spriteColumnGap;
    em.sprites[id].rowGap = def.spriteRowGap;
    em.sprites[id].isInUse = false;

    std::cout << "[ECS] Spawned Environment: " << def.name << " at Position (" << position.x << ", " << position.y << ")" << std::endl;

    return id;
}
