#pragma once
#include "data/NameRegistry.hpp"
#include "ecs/Components.hpp"

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration
class EntityManager;
class BehaviorRegistry;

/**
 * @struct EntityDef
 * @brief Static configuration blueprint for a game entity loaded from entities.stv
 */
struct EntityDef {
    std::string id;
    std::string name;
    float maxHp = 100.0f;
    float baseAtk = 1.0f;
    float maxSpeed = 30.0f;
    std::string category;
    std::string species;
    std::string genderModel = "undefined";
    std::string defaultProfession = "none";
    std::vector<std::string> innateBehaviors;
    std::vector<BehaviorRule> innateBehaviorRules;
    /* Graphics */
    std::string texturePath;
    Color color;
    float spriteWidth = 32.0f;
    float spriteHeight = 32.0f;
};

/**
 * @class EntityRegistry
 * @brief Parses and stores static entity templates, and handles their injection into the ECS memory.
 */
class EntityRegistry {
public:
    EntityRegistry() = default;
    ~EntityRegistry() = default;

    /**
     * @brief Loads entity definitions from an .stv file.
     * @param filepath Relative path to entities.stv
     * @return true on success, false on failure.
     */
    bool LoadFromSTV(const std::string& filepath);

    /**
     * @brief Instantiates a static entity template into the live ECS world.
     * @param em Reference to the active EntityManager.
     * @param prefabId The unique ID string of the entity (e.g., "VILLAGER").
     * @param position Where to spawn the entity in world space.
     * @return EntityID The allocated ECS unique ID. Returns 0 on failure.
     */
    EntityID SpawnEntity(EntityManager& em, const std::string& prefabId, Vector2 position, const NameRegistry& nameReg,
                         const BehaviorRegistry& behaviorReg);

    /**
     * @brief Fetches a template definition by its ID.
     */
    const EntityDef* GetEntityDef(const std::string& prefabId) const;

    const std::unordered_map<std::string, EntityDef>& GetAllEntities() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, EntityDef> m_templates;
};
