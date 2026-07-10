#include "data/NameRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>
#include <raylib.h>
#include <sstream>

bool NameRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        if (block.properties.count("names")) {
            std::stringstream ss(block.properties.at("names"));
            std::string name;
            while (std::getline(ss, name, ',')) {
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                m_namesBySpecies[block.id].push_back(name);
            }
        }
    }
    std::cout << "[INFO] Loaded names for " << m_namesBySpecies.size() << " species." << std::endl;
    return true;
}

std::string NameRegistry::GetRandomName(const std::string& species) const {
    auto it = m_namesBySpecies.find(species);
    if (it != m_namesBySpecies.end() && !it->second.empty()) {
        int randomIndex = GetRandomValue(0, it->second.size() - 1);
        return it->second[randomIndex];
    }
    return "Unknown";
}
