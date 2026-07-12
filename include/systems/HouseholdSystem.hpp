/**
 * @file HouseholdSystem.hpp
 * @brief Assigns private beds to couples and manages lightweight household capacity.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

/**
 * @class HouseholdSystem
 * @brief Assigns available public rest spots to couples.
 *
 * V1 design:
 * - a family is identified by min(partnerA, partnerB);
 * - public beds can be claimed by a couple's family;
 * - claimed beds become private and keep ownerVillageId and ownerFamilyId;
 * - reproduction can later use family-owned bed capacity.
 */
class HouseholdSystem {
public:
    HouseholdSystem() = default;

    void Update(float deltaTime, EntityManager& em);

private:
    float m_updateAccumulator = 0.0f;
};
