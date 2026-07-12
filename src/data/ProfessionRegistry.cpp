/**
 * @file ProfessionRegistry.cpp
 * @brief Implementation of the ProfessionRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/ProfessionRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>

bool ProfessionRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        ProfessionDef def;
        def.id = block.id;
        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("min_age"))
            def.minAge = std::stoi(block.properties.at("min_age"));
        if (block.properties.count("req_species"))
            def.reqSpecies = block.properties.at("req_species");
        m_professions[def.id] = def;
    }
    std::cout << "[INFO] Loaded " << m_professions.size() << " professions." << std::endl;
    return true;
}

const ProfessionDef* ProfessionRegistry::GetProfession(const std::string& id) const {
    auto it = m_professions.find(id);
    return it != m_professions.end() ? &(it->second) : nullptr;
}
