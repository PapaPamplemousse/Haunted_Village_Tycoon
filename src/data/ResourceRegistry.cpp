#include "data/ResourceRegistry.hpp"

#include "data/STVParser.hpp"

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

std::unordered_map<std::string, int> ParseRequirements(const std::string& value) {
    std::unordered_map<std::string, int> result;

    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        std::stringstream pairSS(token);
        std::string id;
        std::string count;

        if (std::getline(pairSS, id, ':') && std::getline(pairSS, count)) {
            id = Trim(id);
            count = Trim(count);

            if (!id.empty() && !count.empty()) {
                result[id] = std::stoi(count);
            }
        }
    }

    return result;
}

} // namespace

bool ResourceRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        ResourceDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        }

        if (block.properties.count("max_stack")) {
            def.maxStack = std::stoi(block.properties.at("max_stack"));
        }

        if (block.properties.count("is_consumable")) {
            def.isConsumable = block.properties.at("is_consumable") == "true";
        }

        if (block.properties.count("nutrition")) {
            def.nutrition = std::stof(block.properties.at("nutrition"));
        }

        if (block.properties.count("hydration")) {
            def.hydration = std::stof(block.properties.at("hydration"));
        }

        if (block.properties.count("healing")) {
            def.healing = std::stof(block.properties.at("healing"));
        }

        if (block.properties.count("spoilage_days")) {
            def.spoilageDays = std::stoi(block.properties.at("spoilage_days"));
        }

        if (block.properties.count("requirements")) {
            def.requirements = ParseRequirements(block.properties.at("requirements"));
        }

        m_resources[def.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_resources.size() << " resource definitions." << std::endl;

    return true;
}

const ResourceDef* ResourceRegistry::GetResourceDef(const std::string& id) const {
    auto it = m_resources.find(id);
    return it != m_resources.end() ? &it->second : nullptr;
}
