/**
 * @file FactionRegistry.cpp
 * @brief Implementation of the faction registry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/FactionRegistry.hpp"

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

float GetFloat(const std::unordered_map<std::string, std::string>& props, const std::string& key, float defaultValue) {
    auto it = props.find(key);

    if (it == props.end()) {
        return defaultValue;
    }

    return std::stof(Trim(it->second));
}

std::string GetString(const std::unordered_map<std::string, std::string>& props, const std::string& key, const std::string& defaultValue) {
    auto it = props.find(key);

    if (it == props.end()) {
        return defaultValue;
    }

    return Trim(it->second);
}

std::vector<std::string> ParseStringList(const std::string& value) {
    std::vector<std::string> result;

    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (!token.empty()) {
            result.push_back(token);
        }
    }

    return result;
}

} // namespace

bool FactionRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    m_factions.clear();

    for (const auto& block : blocks) {
        FactionDef def;
        def.id = block.id;

        def.name = GetString(block.properties, "name", block.id);
        def.description = GetString(block.properties, "description", "");

        def.baseConviction = GetFloat(block.properties, "base_conviction", 10.0f);
        def.fearAffinity = GetFloat(block.properties, "fear_affinity", 0.0f);
        def.corruptionAffinity = GetFloat(block.properties, "corruption_affinity", 0.0f);

        def.sameFactionTrustGain = GetFloat(block.properties, "same_faction_trust_gain", 0.15f);
        def.opposedFactionResentmentGain = GetFloat(block.properties, "opposed_faction_resentment_gain", 0.10f);

        auto opposedIt = block.properties.find("opposed_factions");
        if (opposedIt != block.properties.end()) {
            def.opposedFactions = ParseStringList(opposedIt->second);
        }

        m_factions[def.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_factions.size() << " faction definitions." << std::endl;
    return true;
}

const FactionDef* FactionRegistry::GetFaction(const std::string& id) const {
    auto it = m_factions.find(id);
    return it != m_factions.end() ? &it->second : nullptr;
}
