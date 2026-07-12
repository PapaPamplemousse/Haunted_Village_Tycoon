/**
 * @file BehaviorRegistry.hpp
 * @brief Parses and stores AI behavior mappings and rules.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include "ecs/Components.hpp"

#include <string>
#include <vector>

struct BehaviorMapping {
    std::string id;
    BehaviorRule rule;
    std::vector<std::string> categories;
    std::vector<std::string> species;
    std::vector<std::string> professions;
};

class BehaviorRegistry {
public:
    BehaviorRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    /**
     * @brief Retourne les règles applicables pour un profil donné.
     */
    std::vector<BehaviorRule> GetBehaviorsFor(const std::string& category, const std::string& species, const std::string& profession) const;

private:
    std::vector<BehaviorMapping> m_mappings;
};
