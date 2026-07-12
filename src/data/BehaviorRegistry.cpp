/**
 * @file BehaviorRegistry.cpp
 * @brief Implementation of the BehaviorRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/BehaviorRegistry.hpp"

#include "data/STVParser.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> SplitCommaList(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string token;
    while (std::getline(ss, token, ',')) {
        token = Trim(token);
        if (!token.empty())
            result.push_back(token);
    }
    return result;
}

BehaviorRule ParseRule(const std::string& rawBehavior) {
    BehaviorRule rule;
    const std::string behavior = Trim(rawBehavior);
    const size_t openParen = behavior.find('(');
    const size_t closeParen = behavior.find_last_of(')');

    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen) {
        rule.name = behavior;
        return rule;
    }

    rule.name = Trim(behavior.substr(0, openParen));
    const std::string argumentList = behavior.substr(openParen + 1, closeParen - openParen - 1);
    rule.arguments = SplitCommaList(argumentList);
    return rule;
}

bool Contains(const std::vector<std::string>& list, const std::string& item) {
    return std::find(list.begin(), list.end(), item) != list.end();
}
} // namespace

bool BehaviorRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        if (!block.properties.count("rule"))
            continue;

        BehaviorMapping mapping;
        mapping.id = block.id;
        mapping.rule = ParseRule(block.properties.at("rule"));

        if (block.properties.count("categories")) {
            mapping.categories = SplitCommaList(block.properties.at("categories"));
        }
        if (block.properties.count("species")) {
            mapping.species = SplitCommaList(block.properties.at("species"));
        }
        if (block.properties.count("professions")) {
            mapping.professions = SplitCommaList(block.properties.at("professions"));
        }

        m_mappings.push_back(mapping);
    }
    std::cout << "[INFO] Loaded " << m_mappings.size() << " behavior mappings." << std::endl;
    return true;
}

std::vector<BehaviorRule> BehaviorRegistry::GetBehaviorsFor(const std::string& category, const std::string& species,
                                                            const std::string& profession) const {
    std::vector<BehaviorRule> rules;
    for (const auto& mapping : m_mappings) {
        bool applies = false;
        if (!mapping.categories.empty() && Contains(mapping.categories, category))
            applies = true;
        if (!mapping.species.empty() && Contains(mapping.species, species))
            applies = true;
        if (!mapping.professions.empty() && Contains(mapping.professions, profession))
            applies = true;

        if (applies) {
            rules.push_back(mapping.rule);
        }
    }
    return rules;
}
