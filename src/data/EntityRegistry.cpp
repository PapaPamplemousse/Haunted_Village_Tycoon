#include "data/EntityRegistry.hpp"

#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <iostream>
#include <sstream>

static Color ParseColor(const std::string& value) {
    Color c = {255, 255, 255, 255};
    std::stringstream ss(value);
    std::string token;
    int i = 0;
    while (std::getline(ss, token, ',')) {
        if (i == 0)
            c.r = (unsigned char)std::stoi(token);
        else if (i == 1)
            c.g = (unsigned char)std::stoi(token);
        else if (i == 2)
            c.b = (unsigned char)std::stoi(token);
        else if (i == 3)
            c.a = (unsigned char)std::stoi(token);
        i++;
    }
    return c;
}

bool EntityRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        EntityDef def;
        def.id = block.id;

        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("max_hp"))
            def.maxHp = std::stof(block.properties.at("max_hp"));
        if (block.properties.count("max_speed"))
            def.maxSpeed = std::stof(block.properties.at("max_speed"));
        if (block.properties.count("category"))
            def.category = block.properties.at("category");
        if (block.properties.count("species"))
            def.species = block.properties.at("species");

        // Parse innate behaviors (comma-separated list)
        if (block.properties.count("innate_behaviors")) {
            std::stringstream ss(block.properties.at("innate_behaviors"));
            std::string behavior;
            while (std::getline(ss, behavior, ',')) {
                // Trim trailing/leading spaces
                behavior.erase(0, behavior.find_first_not_of(" \t"));
                behavior.erase(behavior.find_last_not_of(" \t") + 1);
                def.innateBehaviors.push_back(behavior);
            }
        }

        if (block.properties.count("texture"))
            def.texturePath = block.properties.at("texture");
        if (block.properties.count("color"))
            def.color = ParseColor(block.properties.at("color"));
        if (block.properties.count("sprite_size")) {
            std::stringstream ss(block.properties.at("sprite_size"));
            std::string token;
            if (std::getline(ss, token, ','))
                def.spriteWidth = std::stof(token);
            if (std::getline(ss, token, ','))
                def.spriteHeight = std::stof(token);
        }

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " entity definitions." << std::endl;
    return true;
}

EntityID EntityRegistry::SpawnEntity(EntityManager& em, const std::string& prefabId, Vector2 position, const NameRegistry& nameReg) {
    auto it = m_templates.find(prefabId);
    if (it == m_templates.end()) {
        std::cerr << "[WARNING] Cannot spawn unknown entity prefab: " << prefabId << std::endl;
        return 0;
    }

    const EntityDef& def = it->second;
    EntityID id = em.CreateEntity();

    // 1. Core Identity & Location
    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    // 2. Health & Survival Stats
    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasNeeds[id] = true;
    em.needs[id] = {100.0f, 100.0f}; // Start fully fed

    // 3. AI & Career Logic
    em.hasProfession[id] = true;
    em.professions[id] = {"none"}; // Default unassigned job class

    em.hasBehavior[id] = true;
    em.behaviors[id] = {def.innateBehaviors};

    // 4. Default Empty Inventory for carrying resources
    em.hasInventory[id] = true;
    em.inventories[id] = {};

    // 5. Visual Representation
    em.hasSprite[id] = true;
    em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, true};

    // 6. RPG Statistics
    em.hasStats[id] = true;
    em.stats[id] = {def.maxSpeed};

    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id, nameReg.GetRandomName(def.species), def.species, 0};

    std::cout << "[ECS] Spawned Entity: " << def.name << " at Position (" << position.x << ", " << position.y << ")" << std::endl;
    return id;
}

const EntityDef* EntityRegistry::GetEntityDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
