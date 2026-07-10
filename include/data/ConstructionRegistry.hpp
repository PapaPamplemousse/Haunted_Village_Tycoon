#pragma once
#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>

class EntityManager;

struct ConstructionDef {
    std::string id;
    std::string name;
    float maxHp = 100.0f;

    bool isWall = false;
    bool isDoor = false;
    std::unordered_map<std::string, int> blueprintCost;

    // Graphics
    std::string texturePath;
    Color color;
    float spriteWidth = 32.0f;
    float spriteHeight = 32.0f;
};

class ConstructionRegistry {
public:
    ConstructionRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);
    EntityID SpawnConstruction(EntityManager& em, const std::string& prefabId, Vector2 position, bool asBlueprint);
    const std::unordered_map<std::string, ConstructionDef>& GetAllConstructions() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, ConstructionDef> m_templates;
};
