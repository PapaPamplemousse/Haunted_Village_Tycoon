#include "data/WeaponRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>

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

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " weapon definitions." << std::endl;
    return true;
}

const WeaponDef* WeaponRegistry::GetWeaponDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
