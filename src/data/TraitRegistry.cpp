/**
 * @file TraitRegistry.cpp
 * @brief Implementation of the TraitRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/TraitRegistry.hpp"

#include "data/STVParser.hpp"

#include <algorithm>
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

float ParseFloat(const std::unordered_map<std::string, std::string>& properties, const std::string& key, float defaultValue) {
    auto it = properties.find(key);

    if (it == properties.end()) {
        return defaultValue;
    }

    return std::stof(Trim(it->second));
}

int ParseInt(const std::unordered_map<std::string, std::string>& properties, const std::string& key, int defaultValue) {
    auto it = properties.find(key);

    if (it == properties.end()) {
        return defaultValue;
    }

    return std::stoi(Trim(it->second));
}

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

bool IsHumanEntity(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].species == "human";
}

} // namespace

bool TraitRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    m_traits.clear();
    m_weightedTraitIds.clear();

    for (const auto& block : blocks) {
        TraitDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = Trim(block.properties.at("name"));
        } else {
            def.name = def.id;
        }

        def.sociability = ParseFloat(block.properties, "sociability", 0.0f);
        def.bravery = ParseFloat(block.properties, "bravery", 0.0f);
        def.kindness = ParseFloat(block.properties, "kindness", 0.0f);
        def.patience = ParseFloat(block.properties, "patience", 0.0f);
        def.aggression = ParseFloat(block.properties, "aggression", 0.0f);
        def.loyalty = ParseFloat(block.properties, "loyalty", 0.0f);

        def.resentmentGainMultiplier = ParseFloat(block.properties, "resentment_gain_multiplier", 1.0f);
        def.resentmentDecayMultiplier = ParseFloat(block.properties, "resentment_decay_multiplier", 1.0f);
        def.workScoreModifier = ParseFloat(block.properties, "work_score_modifier", 0.0f);

        def.weight = std::max(1, ParseInt(block.properties, "weight", 10));

        m_traits[def.id] = def;

        for (int i = 0; i < def.weight; ++i) {
            m_weightedTraitIds.push_back(def.id);
        }
    }

    std::cout << "[INFO] Loaded " << m_traits.size() << " personality traits." << std::endl;

    return true;
}

const TraitDef* TraitRegistry::GetTraitDef(const std::string& id) const {
    auto it = m_traits.find(id);
    return it != m_traits.end() ? &it->second : nullptr;
}

void TraitRegistry::AssignRandomPersonality(EntityManager& em, EntityID entity) const {
    if (!IsHumanEntity(em, entity) || m_weightedTraitIds.empty()) {
        return;
    }

    if (em.hasPersonality[entity] && !em.personalities[entity].traits.empty()) {
        return;
    }

    em.hasPersonality[entity] = true;
    em.personalities[entity] = {};

    PersonalityComponent& personality = em.personalities[entity];

    const int traitCount = GetRandomValue(2, 3);

    for (int i = 0; i < traitCount; ++i) {
        const int index = GetRandomValue(0, static_cast<int>(m_weightedTraitIds.size()) - 1);
        const std::string& traitId = m_weightedTraitIds[static_cast<size_t>(index)];

        if (std::find(personality.traits.begin(), personality.traits.end(), traitId) != personality.traits.end()) {
            continue;
        }

        const TraitDef* trait = GetTraitDef(traitId);

        if (trait == nullptr) {
            continue;
        }

        personality.traits.push_back(traitId);

        personality.sociability = Clamp01(personality.sociability + trait->sociability);
        personality.bravery = Clamp01(personality.bravery + trait->bravery);
        personality.kindness = Clamp01(personality.kindness + trait->kindness);
        personality.patience = Clamp01(personality.patience + trait->patience);
        personality.aggression = Clamp01(personality.aggression + trait->aggression);
        personality.loyalty = Clamp01(personality.loyalty + trait->loyalty);

        personality.resentmentGainMultiplier *= trait->resentmentGainMultiplier;
        personality.resentmentDecayMultiplier *= trait->resentmentDecayMultiplier;
        personality.workScoreModifier += trait->workScoreModifier;
    }
}

void TraitRegistry::AssignMissingPersonalities(EntityManager& em) const {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsHumanEntity(em, entity)) {
            continue;
        }

        if (em.hasPersonality[entity] && !em.personalities[entity].traits.empty()) {
            continue;
        }

        AssignRandomPersonality(em, entity);
    }
}
