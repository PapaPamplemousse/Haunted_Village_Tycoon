/**
 * @file EnvironmentRegistry.hpp
 * @brief Parses and stores natural environment definitions such as trees, bushes and seasonal resources.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

class EntityManager;

/**
 * @struct EnvironmentDef
 * @brief Static configuration for a natural environment object loaded from environment.stv.
 */
struct EnvironmentDef {
    std::string id;
    std::string name;

    float maxHp = 80.0f;
    bool isObstacle = false;
    std::string harvestTool = "none";

    std::vector<DropEntry> drops;

    // Graphics fallback / sprite rendering.
    std::string texturePath = "square";
    Color color = WHITE;
    float spriteWidth = 32.0f;
    float spriteHeight = 32.0f;

    // Optional spritesheet support.
    bool useSpriteSheet = false;
    SpriteSheetMode spriteSheetMode = SpriteSheetMode::None;

    int spriteSheetColumns = 1;
    int spriteSheetRows = 1;

    float spriteFrameWidth = 0.0f;
    float spriteFrameHeight = 0.0f;

    float spriteColumnGap = 0.0f;
    float spriteRowGap = 0.0f;
};

class EnvironmentRegistry {
public:
    EnvironmentRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);
    EntityID SpawnEnvironment(EntityManager& em, const std::string& prefabId, Vector2 position) const;

    const std::unordered_map<std::string, EnvironmentDef>& GetAllEnvironments() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, EnvironmentDef> m_templates;
};
