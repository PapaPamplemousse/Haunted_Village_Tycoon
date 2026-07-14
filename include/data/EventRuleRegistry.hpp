/**
 * @file EventRuleRegistry.hpp
 * @brief Parses and stores data-driven simulation event rules.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct IntRange {
    int min = 0;
    int max = 0;
};

struct EventRuleDef {
    std::string id;
    std::string name;

    // Supported V2.1 types:
    // - chronicle
    // - settlement_effect
    // - food_theft
    // - spawn_wave
    std::string type = "chronicle";

    // Supported phases:
    // - night
    // - dawn
    // - season
    std::string phase = "night";

    int priority = 0;

    int minPopulation = 0;
    int minSeasonNumber = 0;

    std::vector<int> allowedSeasonIndexes;

    float baseChance = 0.0f;
    float populationChanceFactor = 0.0f;
    float seasonChanceFactor = 0.0f;

    int cooldownDays = 0;

    float fear = 0.0f;
    float corruption = 0.0f;

    std::string message;
    std::string emptyMessage;

    // food_theft
    IntRange foodAmount = {1, 4};

    // spawn_wave
    std::string spawnPrefabId;
    IntRange spawnCount = {1, 1};
    int spawnPopulationDivisor = 0;
    int spawnSeasonDivisor = 0;
    int spawnRandomBonus = 0;
    IntRange spawnRadiusTiles = {12, 24};
};

class EventRuleRegistry {
public:
    EventRuleRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    const std::vector<EventRuleDef>& GetRules() const {
        return m_rules;
    }

private:
    std::vector<EventRuleDef> m_rules;
};
