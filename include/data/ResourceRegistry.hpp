/**
 * @file ResourceRegistry.hpp
 * @brief Parses and stores resource and item definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include <string>
#include <unordered_map>

struct ResourceDef {
    std::string id;
    std::string name;

    int maxStack = 1;
    bool isConsumable = false;

    float nutrition = 0.0f;
    float hydration = 0.0f;
    float healing = 0.0f;

    int spoilageDays = 0;

    std::unordered_map<std::string, int> requirements;
};

class ResourceRegistry {
public:
    ResourceRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    const ResourceDef* GetResourceDef(const std::string& id) const;

    const std::unordered_map<std::string, ResourceDef>& GetAllResources() const {
        return m_resources;
    }

private:
    std::unordered_map<std::string, ResourceDef> m_resources;
};
