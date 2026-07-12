/**
 * @file WeaponRegistry.hpp
 * @brief Parses and stores weapon and tool definitions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <string>
#include <unordered_map>

/**
 * @struct WeaponDef
 * @brief Blueprint of a weapon or tool loaded from weapons.stv
 */
struct WeaponDef {
    std::string id;
    std::string name;
    std::string equipmentSlot;
    float damage = 0.0f;
    std::string toolType = "none";
};

/**
 * @class WeaponRegistry
 * @brief Parses and stores all weapon and tool definitions.
 */
class WeaponRegistry {
public:
    WeaponRegistry() = default;

    bool LoadFromSTV(const std::string& filepath);
    const WeaponDef* GetWeaponDef(const std::string& prefabId) const;

    const std::unordered_map<std::string, WeaponDef>& GetAllWeapons() const {
        return m_templates;
    }

private:
    std::unordered_map<std::string, WeaponDef> m_templates;
};
