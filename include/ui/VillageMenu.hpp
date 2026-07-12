/**
 * @file VillageMenu.hpp
 * @brief Village management menu for settlement overview, villagers, professions and storage inspection.
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

enum class VillageMenuTab { Overview = 0, Villagers, Professions, Storage };

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
 * Manual profession assignment will be added later once the profession system
 * supports explicit player overrides without fighting auto-assignment.
 */
class VillageMenu {
public:
    VillageMenu() = default;

    void Update(EntityManager& em, GameCamera& camera, const ProfessionRegistry& professionReg, const BehaviorRegistry& behaviorReg);
    void Render(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem) const;

    bool IsOpen() const {
        return m_isOpen;
    }

private:
    bool m_isOpen = false;
    VillageMenuTab m_currentTab = VillageMenuTab::Overview;

    int m_selectedVillagerIndex = 0;
    int m_selectedProfessionSlotIndex = 0;

    EntityID FindPrimaryVillage(const EntityManager& em) const;
    int GetTabIndex() const;
    void SetTabIndex(int index);

    void RenderTabs(float x, float y) const;
    void RenderOverview(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem, EntityID villageId,
                        float x, float y) const;
    void RenderVillagers(const EntityManager& em, EntityID villageId, float x, float y) const;
    void RenderProfessions(const EntityManager& em, float x, float y) const;
    void RenderStorage(const EntityManager& em, const ResourceRegistry& resourceReg, EntityID villageId, float x, float y) const;

    EntityID GetSelectedVillager(const EntityManager& em, EntityID villageId) const;
};
