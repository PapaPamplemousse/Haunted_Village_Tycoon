#include "ui/UIManager.hpp"

#include "core/Config.hpp"

UIManager::UIManager() {
    // Initialisation temporaire des listes (Data-Driven plus tard)
    m_entities = {"VILLAGER", "CANNIBAL"};
    m_furniture = {"CAMPFIRE", "WOOD_CHEST"};
    m_constructions = {"WOOD_WALL", "WOOD_DOOR"};
}

const std::vector<std::string>& UIManager::GetCurrentList() const {
    if (m_currentCategory == BuildCategory::Entities)
        return m_entities;
    if (m_currentCategory == BuildCategory::Furniture)
        return m_furniture;
    return m_constructions;
}

void UIManager::Update() {
    // 1. Toggle Menu avec la touche 'E'
    if (IsKeyPressed(KEY_E)) {
        m_isOpen = !m_isOpen;
        // Reset de la sélection quand on ouvre/ferme
        if (m_isOpen)
            m_selectedPrefab = "";
    }

    // 2. Navigation dans le menu
    if (m_isOpen) {
        const auto& currentList = GetCurrentList();

        // Changer de catégorie (Gauche / Droite)
        if (IsKeyPressed(KEY_RIGHT)) {
            if (m_currentCategory == BuildCategory::Entities)
                m_currentCategory = BuildCategory::Furniture;
            else if (m_currentCategory == BuildCategory::Furniture)
                m_currentCategory = BuildCategory::Constructions;
            m_selectedIndex = 0;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            if (m_currentCategory == BuildCategory::Constructions)
                m_currentCategory = BuildCategory::Furniture;
            else if (m_currentCategory == BuildCategory::Furniture)
                m_currentCategory = BuildCategory::Entities;
            m_selectedIndex = 0;
        }

        // Naviguer dans la liste (Haut / Bas)
        if (IsKeyPressed(KEY_DOWN) && !currentList.empty()) {
            m_selectedIndex = (m_selectedIndex + 1) % currentList.size();
        }
        if (IsKeyPressed(KEY_UP) && !currentList.empty()) {
            m_selectedIndex = (m_selectedIndex - 1 + currentList.size()) % currentList.size();
        }

        // Valider la sélection avec ENTER
        if (IsKeyPressed(KEY_ENTER) && !currentList.empty()) {
            m_selectedPrefab = currentList[m_selectedIndex];
            m_isOpen = false; // Ferme le menu et passe en mode "Placement"
        }
    }
}

void UIManager::Render() const {
    if (!m_isOpen)
        return;

    // Fond sombre semi-transparent
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.7f));

    int centerX = GetScreenWidth() / 2;
    int centerY = GetScreenHeight() / 2;

    // Titre
    DrawText("BUILD MENU", centerX - MeasureText("BUILD MENU", 40) / 2, 50, 40, RAYWHITE);
    DrawText("Use ARROWS to navigate. Press ENTER to select. E to close.", centerX - 250, 100, 20, LIGHTGRAY);

    // Dessin des Catégories (Onglets)
    Color entCol = (m_currentCategory == BuildCategory::Entities) ? YELLOW : GRAY;
    Color furCol = (m_currentCategory == BuildCategory::Furniture) ? YELLOW : GRAY;
    Color conCol = (m_currentCategory == BuildCategory::Constructions) ? YELLOW : GRAY;

    DrawText("ENTITIES", centerX - 250, 150, 30, entCol);
    DrawText("FURNITURE", centerX - 50, 150, 30, furCol);
    DrawText("CONSTRUCTIONS", centerX + 150, 150, 30, conCol);

    // Dessin de la liste d'objets
    const auto& currentList = GetCurrentList();
    int startY = 250;

    for (size_t i = 0; i < currentList.size(); ++i) {
        Color itemCol = (i == m_selectedIndex) ? GREEN : RAYWHITE;
        std::string prefix = (i == m_selectedIndex) ? "> " : "  ";
        DrawText((prefix + currentList[i]).c_str(), centerX - 100, startY + (i * 40), 30, itemCol);
    }
}
