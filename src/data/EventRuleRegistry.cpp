/**
 * @file Implementation of data-driven event rule loading.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/EventRuleRegistry.hpp"

#include "data/STVParser.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

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

int GetInt(const std::unordered_map<std::string, std::string>& props, const std::string& key, int defaultValue) {
    auto it = props.find(key);

    if (it == props.end()) {
        return defaultValue;
    }

    return std::stoi(Trim(it->second));
}

std::string GetString(const std::unordered_map<std::string, std::string>& props, const std::string& key, const std::string& defaultValue) {
    auto it = props.find(key);
    if (it == props.end()) {
        return defaultValue;
    }

    return Trim(it->second);
}

IntRange GetIntRange(const std::unordered_map<std::string, std::string>& props, const std::string& key, IntRange defaultValue) {
    auto it = props.find(key);

    if (it == props.end()) {
        return defaultValue;
    }

    std::stringstream ss(it->second);
    std::string token;
    IntRange range = defaultValue;

    if (std::getline(ss, token, ',')) {
        range.min = std::stoi(Trim(token));
    }

    if (std::getline(ss, token, ',')) {
        range.max = std::stoi(Trim(token));
    } else {
        range.max = range.min;
    }

    if (range.max < range.min) {
        std::swap(range.min, range.max);
    }

    return range;
}

std::vector<int> GetIntList(const std::unordered_map<std::string, std::string>& props, const std::string& key) {
    std::vector<int> result;

    auto it = props.find(key);

    if (it == props.end()) {
        return result;
    }

    std::stringstream ss(it->second);
    std::string token;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (!token.empty()) {
            result.push_back(std::stoi(token));
        }
    }

    return result;
}

} // namespace

bool EventRuleRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    m_rules.clear();

    for (const auto& block : blocks) {
        EventRuleDef rule;
        rule.id = block.id;

        rule.name = GetString(block.properties, "name", block.id);
        rule.type = GetString(block.properties, "type", "chronicle");
        rule.phase = GetString(block.properties, "phase", "night");

        rule.priority = GetInt(block.properties, "priority", 0);

        rule.minPopulation = GetInt(block.properties, "min_population", 0);
        rule.minSeasonNumber = GetInt(block.properties, "min_season_number", 0);
        rule.allowedSeasonIndexes = GetIntList(block.properties, "allowed_seasons");

        rule.baseChance = GetFloat(block.properties, "base_chance", 0.0f);
        rule.populationChanceFactor = GetFloat(block.properties, "population_chance_factor", 0.0f);
        rule.seasonChanceFactor = GetFloat(block.properties, "season_chance_factor", 0.0f);

        rule.cooldownDays = GetInt(block.properties, "cooldown_days", 0);

        rule.fear = GetFloat(block.properties, "fear", 0.0f);
        rule.corruption = GetFloat(block.properties, "corruption", 0.0f);

        rule.message = GetString(block.properties, "message", "");
        rule.emptyMessage = GetString(block.properties, "empty_message", "");

        rule.foodAmount = GetIntRange(block.properties, "food_amount", {1, 4});

        rule.spawnPrefabId = GetString(block.properties, "spawn_prefab", "");
        rule.spawnCount = GetIntRange(block.properties, "spawn_count", {1, 1});
        rule.spawnPopulationDivisor = GetInt(block.properties, "spawn_population_divisor", 0);
        rule.spawnSeasonDivisor = GetInt(block.properties, "spawn_season_divisor", 0);
        rule.spawnRandomBonus = GetInt(block.properties, "spawn_random_bonus", 0);
        rule.spawnRadiusTiles = GetIntRange(block.properties, "spawn_radius_tiles", {12, 24});

        m_rules.push_back(rule);
    }

    // J'ai corrigé la syntaxe de la lambda pour std::sort ici !
    std::sort(m_rules.begin(), m_rules.end(), [](const EventRuleDef& lhs, const EventRuleDef& rhs) { return lhs.priority > rhs.priority; });

    std::cout << "[INFO] Loaded " << m_rules.size() << " event rules." << std::endl;

    return true;
}
