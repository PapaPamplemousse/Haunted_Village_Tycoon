/**
 * @file ProfessionRegistry.hpp
 * @brief Parses and stores profession requirements and definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <string>
#include <unordered_map>

struct ProfessionDef {
    std::string id;
    std::string name;
    int minAge = 0;
    std::string reqSpecies;
};

class ProfessionRegistry {
public:
    bool LoadFromSTV(const std::string& filepath);
    const ProfessionDef* GetProfession(const std::string& id) const;

private:
    std::unordered_map<std::string, ProfessionDef> m_professions;
};
