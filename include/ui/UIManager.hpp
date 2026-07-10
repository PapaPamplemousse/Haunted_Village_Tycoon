#pragma once
#include <raylib.h>
#include <string>
#include <vector>

// Forward declarations pour éviter d'inclure les gros headers ici
class EntityRegistry;
class FurnitureRegistry;
class ConstructionRegistry;

enum class BuildCategory { Entities, Furniture, Constructions };

class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;

    void Initialize(const EntityRegistry& entReg, const FurnitureRegistry& furReg, const ConstructionRegistry& conReg);

    void Update();
    void Render() const;

    bool IsMenuOpen() const {
        return m_isOpen;
    }
    std::string GetSelectedPrefab() const {
        return m_selectedPrefab;
    }
    BuildCategory GetSelectedCategory() const {
        return m_currentCategory;
    }

private:
    bool m_isOpen = false;
    BuildCategory m_currentCategory = BuildCategory::Entities;
    int m_selectedIndex = 0;

    std::string m_selectedPrefab = "";

    std::vector<std::string> m_entities;
    std::vector<std::string> m_furniture;
    std::vector<std::string> m_constructions;

    const std::vector<std::string>& GetCurrentList() const;
};
