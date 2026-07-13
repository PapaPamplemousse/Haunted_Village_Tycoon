/**
 * @file WeaponRegistry.cpp
 * @brief Implementation of the WeaponRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/WeaponRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>
#include <sstream>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::unordered_map<std::string, int> ParseRequirements(const std::string& value) {
    std::unordered_map<std::string, int> result;

    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        std::stringstream pairSS(token);
        std::string itemId;
        std::string amount;

        if (std::getline(pairSS, itemId, ':') && std::getline(pairSS, amount)) {
            itemId = Trim(itemId);
            amount = Trim(amount);

            if (!itemId.empty()) {
                result[itemId] = std::stoi(amount);
            }
        }
    }

    return result;
}

} // namespace

bool WeaponRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        WeaponDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        }
        if (block.properties.count("equipment_slot")) {
            def.equipmentSlot = block.properties.at("equipment_slot");
        }
        if (block.properties.count("damage")) {
            def.damage = std::stof(block.properties.at("damage"));
        }
        if (block.properties.count("tool_type")) {
            def.toolType = block.properties.at("tool_type");
        }
        if (block.properties.count("requirements")) {
            def.requirements = ParseRequirements(block.properties.at("requirements"));
        }

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " weapon definitions." << std::endl;
    return true;
}

const WeaponDef* WeaponRegistry::GetWeaponDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
