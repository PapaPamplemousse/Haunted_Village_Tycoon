// #include "data/EntityRegistry.hpp"

// #include "data/STVParser.hpp"
// #include "ecs/EntityManager.hpp"

// #include <iostream>
// #include <sstream>

// static Color ParseColor(const std::string& value) {
//     Color c = {255, 255, 255, 255};
//     std::stringstream ss(value);
//     std::string token;
//     int i = 0;
//     while (std::getline(ss, token, ',')) {
//         if (i == 0)
//             c.r = (unsigned char)std::stoi(token);
//         else if (i == 1)
//             c.g = (unsigned char)std::stoi(token);
//         else if (i == 2)
//             c.b = (unsigned char)std::stoi(token);
//         else if (i == 3)
//             c.a = (unsigned char)std::stoi(token);
//         i++;
//     }
//     return c;
// }

// bool EntityRegistry::LoadFromSTV(const std::string& filepath) {
//     auto blocks = STVParser::Parse(filepath);
//     if (blocks.empty())
//         return false;

//     for (const auto& block : blocks) {
//         EntityDef def;
//         def.id = block.id;

//         if (block.properties.count("name"))
//             def.name = block.properties.at("name");
//         if (block.properties.count("max_hp"))
//             def.maxHp = std::stof(block.properties.at("max_hp"));
//         if (block.properties.count("max_speed"))
//             def.maxSpeed = std::stof(block.properties.at("max_speed"));
//         if (block.properties.count("category"))
//             def.category = block.properties.at("category");
//         if (block.properties.count("species"))
//             def.species = block.properties.at("species");

//         // Parse innate behaviors (comma-separated list)
//         if (block.properties.count("innate_behaviors")) {
//             std::stringstream ss(block.properties.at("innate_behaviors"));
//             std::string behavior;
//             while (std::getline(ss, behavior, ',')) {
//                 // Trim trailing/leading spaces
//                 behavior.erase(0, behavior.find_first_not_of(" \t"));
//                 behavior.erase(behavior.find_last_not_of(" \t") + 1);
//                 def.innateBehaviors.push_back(behavior);
//             }
//         }

//         if (block.properties.count("texture"))
//             def.texturePath = block.properties.at("texture");
//         if (block.properties.count("color"))
//             def.color = ParseColor(block.properties.at("color"));
//         if (block.properties.count("sprite_size")) {
//             std::stringstream ss(block.properties.at("sprite_size"));
//             std::string token;
//             if (std::getline(ss, token, ','))
//                 def.spriteWidth = std::stof(token);
//             if (std::getline(ss, token, ','))
//                 def.spriteHeight = std::stof(token);
//         }

//         m_templates[block.id] = def;
//     }

//     std::cout << "[INFO] Loaded " << m_templates.size() << " entity definitions." << std::endl;
//     return true;
// }

// EntityID EntityRegistry::SpawnEntity(EntityManager& em, const std::string& prefabId, Vector2 position, const NameRegistry& nameReg) {
//     auto it = m_templates.find(prefabId);
//     if (it == m_templates.end()) {
//         std::cerr << "[WARNING] Cannot spawn unknown entity prefab: " << prefabId << std::endl;
//         return 0;
//     }

//     const EntityDef& def = it->second;
//     EntityID id = em.CreateEntity();

//     // 1. Core Identity & Location
//     em.hasTag[id] = true;
//     em.tags[id] = {def.name, def.id};

//     em.hasTransform[id] = true;
//     em.transforms[id] = {position};

//     // 2. Health & Survival Stats
//     em.hasHealth[id] = true;
//     em.healths[id] = {def.maxHp, def.maxHp};

//     em.hasNeeds[id] = true;
//     em.needs[id] = {100.0f, 100.0f}; // Start fully fed

//     // 3. AI & Career Logic
//     em.hasProfession[id] = true;
//     em.professions[id] = {"none"}; // Default unassigned job class

//     em.hasBehavior[id] = true;
//     em.behaviors[id] = {def.innateBehaviors};

//     // 4. Default Empty Inventory for carrying resources
//     em.hasInventory[id] = true;
//     em.inventories[id] = {};

//     // 5. Visual Representation
//     em.hasSprite[id] = true;
//     em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, true};

//     // 6. RPG Statistics
//     em.hasStats[id] = true;
//     em.stats[id] = {def.maxSpeed};

//     em.hasTag[id] = true;
//     em.tags[id] = {def.name, def.id, nameReg.GetRandomName(def.species), def.species, 0};

//     std::cout << "[ECS] Spawned Entity: " << def.name << " at Position (" << position.x << ", " << position.y << ")" << std::endl;
//     return id;
// }

// const EntityDef* EntityRegistry::GetEntityDef(const std::string& prefabId) const {
//     auto it = m_templates.find(prefabId);
//     return it != m_templates.end() ? &(it->second) : nullptr;
// }

#include "data/EntityRegistry.hpp"

#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

Color ParseColor(const std::string& value) {
    Color c = {255, 255, 255, 255};

    std::stringstream ss(value);
    std::string token;
    int i = 0;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (i == 0) {
            c.r = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 1) {
            c.g = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 2) {
            c.b = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 3) {
            c.a = static_cast<unsigned char>(std::stoi(token));
        }

        i++;
    }

    return c;
}

/**
 * @brief Splits a behavior list while ignoring commas inside parentheses.
 *
 * Example:
 *   "wander, hunt(human,rabbit), gather"
 * becomes:
 *   ["wander", "hunt(human,rabbit)", "gather"]
 */
std::vector<std::string> SplitBehaviorList(const std::string& value) {
    std::vector<std::string> result;

    std::string current;
    int parenthesisDepth = 0;

    for (char c : value) {
        if (c == '(') {
            parenthesisDepth++;
            current.push_back(c);
            continue;
        }

        if (c == ')') {
            parenthesisDepth = std::max(0, parenthesisDepth - 1);
            current.push_back(c);
            continue;
        }

        if (c == ',' && parenthesisDepth == 0) {
            const std::string token = Trim(current);

            if (!token.empty()) {
                result.push_back(token);
            }

            current.clear();
            continue;
        }

        current.push_back(c);
    }

    const std::string token = Trim(current);

    if (!token.empty()) {
        result.push_back(token);
    }

    return result;
}

BehaviorRule ParseBehaviorRule(const std::string& rawBehavior) {
    BehaviorRule rule;

    const std::string behavior = Trim(rawBehavior);

    const size_t openParen = behavior.find('(');
    const size_t closeParen = behavior.find_last_of(')');

    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen) {
        rule.name = behavior;
        return rule;
    }

    rule.name = Trim(behavior.substr(0, openParen));

    const std::string argumentList = behavior.substr(openParen + 1, closeParen - openParen - 1);

    std::stringstream ss(argumentList);
    std::string argument;

    while (std::getline(ss, argument, ',')) {
        argument = Trim(argument);

        if (!argument.empty()) {
            rule.arguments.push_back(argument);
        }
    }

    return rule;
}

} // namespace

bool EntityRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        EntityDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        }

        if (block.properties.count("max_hp")) {
            def.maxHp = std::stof(block.properties.at("max_hp"));
        }

        if (block.properties.count("max_speed")) {
            def.maxSpeed = std::stof(block.properties.at("max_speed"));
        }

        if (block.properties.count("category")) {
            def.category = block.properties.at("category");
        }

        if (block.properties.count("species")) {
            def.species = block.properties.at("species");
        }

        if (block.properties.count("innate_behaviors")) {
            const std::vector<std::string> rawBehaviors = SplitBehaviorList(block.properties.at("innate_behaviors"));

            for (const std::string& rawBehavior : rawBehaviors) {
                BehaviorRule rule = ParseBehaviorRule(rawBehavior);

                if (rule.name.empty()) {
                    continue;
                }

                def.innateBehaviorRules.push_back(rule);

                // Keep backward compatibility with the previous AI code.
                def.innateBehaviors.push_back(rule.name);
            }
        }

        if (block.properties.count("texture")) {
            def.texturePath = block.properties.at("texture");
        }

        if (block.properties.count("color")) {
            def.color = ParseColor(block.properties.at("color"));
        }

        if (block.properties.count("sprite_size")) {
            std::stringstream ss(block.properties.at("sprite_size"));
            std::string token;

            if (std::getline(ss, token, ',')) {
                def.spriteWidth = std::stof(Trim(token));
            }

            if (std::getline(ss, token, ',')) {
                def.spriteHeight = std::stof(Trim(token));
            }
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

    // 1. Identity and location
    em.hasTag[id] = true;
    em.tags[id] = {def.name, def.id, nameReg.GetRandomName(def.species), def.species, 0};

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    // 2. Health and needs
    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasNeeds[id] = true;
    em.needs[id] = {100.0f, 100.0f};

    // 3. AI
    em.hasProfession[id] = true;
    em.professions[id] = {"none"};

    em.hasBehavior[id] = true;
    em.behaviors[id] = {};
    em.behaviors[id].innateCapabilities = def.innateBehaviors;
    em.behaviors[id].innateBehaviorRules = def.innateBehaviorRules;

    // 4. Inventory
    em.hasInventory[id] = true;
    em.inventories[id] = {};

    // 5. Visual representation
    em.hasSprite[id] = true;
    em.sprites[id] = {def.texturePath, def.color, def.spriteWidth, def.spriteHeight, true};

    // 6. Stats
    em.hasStats[id] = true;
    em.stats[id] = {def.maxSpeed};

    std::cout << "[ECS] Spawned Entity: " << def.name << " at Position (" << position.x << ", " << position.y << ")" << std::endl;

    return id;
}

const EntityDef* EntityRegistry::GetEntityDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
