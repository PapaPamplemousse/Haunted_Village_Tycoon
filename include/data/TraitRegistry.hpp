/**
 * @file TraitRegistry.hpp
 * @brief Parses and stores personality trait definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct TraitDef {
    std::string id;
    std::string name;

    float sociability = 0.0f;
    float bravery = 0.0f;
    float kindness = 0.0f;
    float patience = 0.0f;
    float aggression = 0.0f;
    float loyalty = 0.0f;

    float resentmentGainMultiplier = 1.0f;
    float resentmentDecayMultiplier = 1.0f;
    float workScoreModifier = 0.0f;

    int weight = 10;
};

class TraitRegistry {
public:
    TraitRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    const TraitDef* GetTraitDef(const std::string& id) const;

    void AssignRandomPersonality(EntityManager& em, EntityID entity) const;
    void AssignMissingPersonalities(EntityManager& em) const;

private:
    std::unordered_map<std::string, TraitDef> m_traits;
    std::vector<std::string> m_weightedTraitIds;
};
