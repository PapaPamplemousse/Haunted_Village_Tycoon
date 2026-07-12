/**
 * @file ConstructionRegistry.cpp
 * @brief Implementation of the ConstructionRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/ConstructionRegistry.hpp"

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

bool ConstructionRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        ConstructionDef def;
        def.id = block.id;

        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("max_hp"))
            def.maxHp = std::stof(block.properties.at("max_hp"));
        if (block.properties.count("is_wall"))
            def.isWall = (block.properties.at("is_wall") == "true");
        if (block.properties.count("is_door"))
            def.isDoor = (block.properties.at("is_door") == "true");

        if (block.properties.count("blueprint_cost")) {
            std::stringstream ss(block.properties.at("blueprint_cost"));
            std::string costToken;
            while (std::getline(ss, costToken, ',')) {
                std::stringstream pairSS(costToken);
                std::string resId, resCount;
                if (std::getline(pairSS, resId, ':') && std::getline(pairSS, resCount)) {
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
    std::cout << "[INFO] Loaded " << m_templates.size() << " construction definitions." << std::endl;
    return true;
}

EntityID ConstructionRegistry::SpawnConstruction(EntityManager& em, const std::string& prefabId, Vector2 position, bool asBlueprint) {
    auto it = m_templates.find(prefabId);
    if (it == m_templates.end()) {
        return 0;
    }

    const ConstructionDef& def = it->second;
    EntityID id = em.CreateEntity();

    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id};

    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    em.hasConstruction[id] = true;
    em.constructions[id] = {def.isWall, def.isDoor};

    if (asBlueprint) {
        em.hasBlueprint[id] = true;
        em.blueprints[id] = {def.blueprintCost, false};
    }

    // IMPORTANT:
    // A construction marked as a door must also receive a DoorComponent.
    // Otherwise the pathfinder and AISystem cannot identify/open it.
    if (def.isDoor) {
        em.hasDoor[id] = true;
        em.doors[id] = {};
        em.doors[id].state = DoorState::CLOSED;
        em.doors[id].ownerId = 0;
    }

    em.hasSprite[id] = true;
    em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, false};

    return id;
}
