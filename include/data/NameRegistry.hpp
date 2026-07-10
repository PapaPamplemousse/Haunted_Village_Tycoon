#pragma once
#include <string>
#include <unordered_map>
#include <vector>

class NameRegistry {
public:
    NameRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);

    /**
     * @brief Pioche un nom au hasard selon l'espèce (ex: "cannibal").
     */
    std::string GetRandomName(const std::string& species) const;

private:
    std::unordered_map<std::string, std::vector<std::string>> m_namesBySpecies;
};
