#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct StructureDef {
    std::string id;
    std::string name;
    int minArea = 0;
    int maxArea = 999999;
    std::unordered_map<std::string, int> requirements; // Ex: {"CAMPFIRE": 1}
    std::vector<std::string> grantedBuffs;
    std::unordered_map<std::string, int> jobSlots;
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
