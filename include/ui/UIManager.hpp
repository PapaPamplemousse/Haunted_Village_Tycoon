#pragma once
#include <raylib.h>
#include <string>
#include <vector>

/**
 * @enum BuildCategory
 * @brief Represents the tabs in the build menu.
 */
enum class BuildCategory { Entities, Furniture, Constructions };

/**
 * @class UIManager
 * @brief Handles the in-game UI, specifically the Build Menu overlay.
 */
class UIManager {
public:
    UIManager();
    ~UIManager() = default;

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

    // L'objet actuellement "en main" prêt à être placé
    std::string m_selectedPrefab = "";

    // Listes des objets disponibles (Hardcodées pour l'instant, on les liera aux Registres plus tard)
    std::vector<std::string> m_entities;
    std::vector<std::string> m_furniture;
    std::vector<std::string> m_constructions;

    const std::vector<std::string>& GetCurrentList() const;
};
