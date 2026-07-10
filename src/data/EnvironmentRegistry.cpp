#include "data/EnvironmentRegistry.hpp"

#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
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
            def.maxHp = std::stof(block.properties.at("max_hp"));
        }

        if (block.properties.count("is_obstacle")) {
            def.isObstacle = block.properties.at("is_obstacle") == "true";
        }

        if (block.properties.count("harvest_tool")) {
            def.harvestTool = block.properties.at("harvest_tool");
        }

        if (block.properties.count("drops")) {
            def.drops = ParseDrops(block.properties.at("drops"));
        }

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " environment definitions." << std::endl;

    return true;
}

EntityID EnvironmentRegistry::SpawnEnvironment(EntityManager& em, const std::string& prefabId, Vector2 position) const {
    auto it = m_templates.find(prefabId);

    if (it == m_templates.end()) {
        return 0;
    }

    const EnvironmentDef& def = it->second;
    EntityID id = em.CreateEntity();

    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id};

    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    em.hasHarvestable[id] = true;
    em.harvestables[id] = {def.harvestTool, def.drops};

    if (def.isObstacle) {
        em.hasConstruction[id] = true;
        em.constructions[id] = {true, false};
    }

    em.hasSprite[id] = true;

    Color color = prefabId == "TREE_OAK" ? DARKGREEN : LIME;
    std::string shape = prefabId == "TREE_OAK" ? "triangle" : "circle";

    em.sprites[id] = {shape, color, 32.0f, 32.0f, false};

    return id;
}
