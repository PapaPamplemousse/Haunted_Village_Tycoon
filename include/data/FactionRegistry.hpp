/**
 * @file FactionRegistry.hpp
 * @brief Parses and stores faction / religion definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct FactionDef {
    std::string id;
    std::string name;
    std::string description;

    float baseConviction = 10.0f;

    // How strongly fear/corruption push villagers toward this faction.
    float fearAffinity = 0.0f;
    float corruptionAffinity = 0.0f;

    // Social modifiers.
    float sameFactionTrustGain = 0.15f;
    float opposedFactionResentmentGain = 0.10f;

    std::vector<std::string> opposedFactions;
};

class FactionRegistry {
public:
    bool LoadFromSTV(const std::string& filepath);

    const FactionDef* GetFaction(const std::string& id) const;

    const std::unordered_map<std::string, FactionDef>& GetAllFactions() const {
        return m_factions;
    }

private:
    std::unordered_map<std::string, FactionDef> m_factions;
};
