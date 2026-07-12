/**
 * @file VillageMenu.hpp
 * @brief Village management menu for settlement overview, villagers, professions, storage and social inspection.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/GameCamera.hpp"
#include "data/BehaviorRegistry.hpp"
#include "data/ProfessionRegistry.hpp"
#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/TimeSystem.hpp"

#include <cstddef>

// Added the Social tab to the enum
enum class VillageMenuTab { Overview = 0, Villagers, Professions, Storage, Social };

/**
 * @class VillageMenu
 * @brief UI panel used to inspect village-level information.
 *
 * V1 provides read-only village management:
 * - overview;
 * - villagers list;
 * - profession slots;
 * - storage content.
 *
 * V2 adds:
 * - social tab for families and relationships.
 */
class VillageMenu {
public:
    VillageMenu() = default;

    void Update(EntityManager& em, GameCamera& camera, const ProfessionRegistry& professionReg, const BehaviorRegistry& behaviorReg);
    void Render(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem,
                const ProfessionRegistry& professionReg) const;

    bool IsOpen() const {
        return m_isOpen;
    }

private:
    bool m_isOpen = false;
    VillageMenuTab m_currentTab = VillageMenuTab::Overview;

    int m_selectedVillagerIndex = 0;
    int m_selectedProfessionSlotIndex = 0;
    int m_selectedProfessionCandidateIndex = 0;

    // Social indices
    int m_selectedSocialPrimaryIndex = 0;
    int m_selectedSocialTargetIndex = 0;

    EntityID FindPrimaryVillage(const EntityManager& em) const;
    int GetTabIndex() const;
    void SetTabIndex(int index);

    void RenderTabs(float x, float y) const;
    void RenderOverview(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem, EntityID villageId,
                        float x, float y) const;
    void RenderVillagers(const EntityManager& em, EntityID villageId, float x, float y) const;
    void RenderProfessions(const EntityManager& em, const ProfessionRegistry& professionReg, float x, float y) const;
    void RenderStorage(const EntityManager& em, const ResourceRegistry& resourceReg, EntityID villageId, float x, float y) const;

    // Added Social render method
    void RenderSocial(const EntityManager& em, EntityID villageId, float x, float y) const;

    EntityID GetSelectedVillager(const EntityManager& em, EntityID villageId) const;
};
