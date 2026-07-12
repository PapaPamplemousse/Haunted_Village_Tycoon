/**
 * @file StructureRegistry.hpp
 * @brief Parses and stores definitions for enclosed rooms and structures.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct StructureDef {
    std::string id;
    std::string name;

    int minArea = 0;
    int maxArea = 999999;

    std::unordered_map<std::string, int> requirements;
    std::vector<std::string> grantedBuffs;
    std::unordered_map<std::string, int> jobSlots;

    // Semantic tags used by systems.
    // isHousing: structure can contribute to population / private ownership logic.
    // isBedroom: structure can be assigned to a family as a private bedroom.
    bool isHousing = false;
    bool isBedroom = false;
};

class StructureRegistry {
public:
    StructureRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);
    const std::unordered_map<std::string, StructureDef>& GetAllStructures() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, StructureDef> m_templates;
};
