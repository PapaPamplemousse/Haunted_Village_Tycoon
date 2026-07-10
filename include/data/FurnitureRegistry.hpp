#pragma once
#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>

class EntityManager;

/**
 * @struct FurnitureDef
 * @brief Static configuration blueprint for a buildable object loaded from furniture.stv
 */
struct FurnitureDef {
    std::string id;
    std::string name;

    // Size in grid cells (e.g., 1x1, 2x3)
    int gridWidth = 1;
    int gridHeight = 1;

    std::string interactionType;
    int storageCapacity = 0;
    std::unordered_map<std::string, int> blueprintCost;

    /* Graphics */
    std::string texturePath;
    Color color;

    // size of the sprite in pixels (for rendering)
    float spriteWidth = 32.0f;
    float spriteHeight = 32.0f;
};

/**
 * @class FurnitureRegistry
 * @brief Parses and stores furniture templates, handling their creation as blueprints or complete structures.
 */
class FurnitureRegistry {
public:
    FurnitureRegistry() = default;
    ~FurnitureRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    /**
     * @brief Spawns a furniture structure into the world.
     * @param em Reference to the active EntityManager.
     * @param prefabId The unique ID of the object (e.g., "WOOD_CHEST").
     * @param position Grid or world coordinates where it should rest.
     * @param asBlueprint If true, spawns a ghost waiting for logistics/construction instead of an active object.
     * @return EntityID The allocated ECS unique ID.
     */
    EntityID SpawnFurniture(EntityManager& em, const std::string& prefabId, Vector2 position, bool asBlueprint);

    const FurnitureDef* GetFurnitureDef(const std::string& prefabId) const;

    const std::unordered_map<std::string, FurnitureDef>& GetAllFurniture() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, FurnitureDef> m_templates;
};
