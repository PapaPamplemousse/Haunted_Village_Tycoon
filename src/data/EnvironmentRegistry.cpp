#include "data/EnvironmentRegistry.hpp"

#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <iostream>
#include <sstream>

bool EnvironmentRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        EnvironmentDef def;
        def.id = block.id;

        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("max_hp"))
            def.maxHp = std::stof(block.properties.at("max_hp"));
        if (block.properties.count("is_obstacle"))
            def.isObstacle = (block.properties.at("is_obstacle") == "true");
        if (block.properties.count("harvest_tool"))
            def.harvestTool = block.properties.at("harvest_tool");

        // Parse drops = wood:3:1.0
        if (block.properties.count("drops")) {
            std::stringstream ss(block.properties.at("drops"));
            std::string token;
            if (std::getline(ss, token, ':'))
                def.dropItem = token;
            if (std::getline(ss, token, ':'))
                def.dropAmount = std::stoi(token);
            if (std::getline(ss, token, ':'))
                def.dropChance = std::stof(token);
        }

        m_templates[block.id] = def;
    }
    std::cout << "[INFO] Loaded " << m_templates.size() << " environment definitions." << std::endl;
    return true;
}

EntityID EnvironmentRegistry::SpawnEnvironment(EntityManager& em, const std::string& prefabId, Vector2 position) const {
    auto it = m_templates.find(prefabId);
    if (it == m_templates.end())
        return 0;

    const EnvironmentDef& def = it->second;
    EntityID id = em.CreateEntity();

    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id, "", "", 0};

    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    em.hasHarvestable[id] = true;
    em.harvestables[id] = {def.harvestTool, def.dropItem, def.dropAmount, def.dropChance};

    // Si c'est un obstacle (comme l'arbre), on lui met un ConstructionComponent (isWall = true)
    // pour que le Pathfinder A* le contourne naturellement !
    if (def.isObstacle) {
        em.hasConstruction[id] = true;
        em.constructions[id] = {true, false}; // isWall = true, isDoor = false
    }

    // Rendu basique pour le moment
    em.hasSprite[id] = true;
    Color color = (prefabId == "TREE_OAK") ? DARKGREEN : LIME;
    std::string shape = (prefabId == "TREE_OAK") ? "triangle" : "circle";
    em.sprites[id] = {shape, color, 32.0f, 32.0f, false};

    return id;
}
