#pragma once

#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

class EntityManager;

struct EnvironmentDef {
    std::string id;
    std::string name;
    float maxHp = 80.0f;
    bool isObstacle = false;
    std::string harvestTool = "none";

    std::vector<DropEntry> drops;
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
