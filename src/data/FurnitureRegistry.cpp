#include "data/FurnitureRegistry.hpp"

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

bool FurnitureRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        FurnitureDef def;
        def.id = block.id;

        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("interaction_type"))
            def.interactionType = block.properties.at("interaction_type");
        if (block.properties.count("storage_capacity"))
            def.storageCapacity = std::stoi(block.properties.at("storage_capacity"));

        // Parse grid_size (e.g., "1, 2")
        if (block.properties.count("grid_size")) {
            std::stringstream ss(block.properties.at("grid_size"));
            std::string token;
            if (std::getline(ss, token, ','))
                def.gridWidth = std::stoi(token);
            if (std::getline(ss, token, ','))
                def.gridHeight = std::stoi(token);
        }

        // Parse blueprint_cost (e.g., "wood:10, stone:2")
        if (block.properties.count("blueprint_cost")) {
            std::stringstream ss(block.properties.at("blueprint_cost"));
            std::string costToken;
            while (std::getline(ss, costToken, ',')) {
                std::stringstream pairSS(costToken);
                std::string resId, resCount;
                if (std::getline(pairSS, resId, ':') && std::getline(pairSS, resCount)) {
                    // Trim spaces
                    resId.erase(0, resId.find_first_not_of(" \t"));
                    resId.erase(resId.find_last_not_of(" \t") + 1);
                    def.blueprintCost[resId] = std::stoi(resCount);
                }
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

    std::cout << "[INFO] Loaded " << m_templates.size() << " furniture definitions." << std::endl;
    return true;
}

EntityID FurnitureRegistry::SpawnFurniture(EntityManager& em, const std::string& prefabId, Vector2 position, bool asBlueprint) {
    auto it = m_templates.find(prefabId);
    if (it == m_templates.end()) {
        std::cerr << "[WARNING] Cannot spawn unknown furniture prefab: " << prefabId << std::endl;
        return 0;
    }

    const FurnitureDef& def = it->second;
    EntityID id = em.CreateEntity();

    // 1. Global Identity & Placement
    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    // 2. Logic Dispatcher based on blueprint status
    if (asBlueprint) {
        // Spawns as an un-interactable project frame
        em.hasBlueprint[id] = true;
        em.blueprints[id] = {def.blueprintCost, false};
    } else {
        // Spawns immediately complete and fully functional
        if (def.storageCapacity > 0) {
            em.hasInventory[id] = true;
            em.inventories[id] = {}; // Allocated empty container chest
        }
    }

    // 5. Visual Representation
    em.hasSprite[id] = true;
    em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, false};

    std::string typeStr = asBlueprint ? "Blueprint Plan" : "Completed Facility";
    std::cout << "[ECS] Spawned Furniture [" << typeStr << "]: " << def.name << " at Position (" << position.x << ", " << position.y << ")"
              << std::endl;
    return id;
}

const FurnitureDef* FurnitureRegistry::GetFurnitureDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
