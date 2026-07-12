/**
 * @file HouseholdSystem.hpp
 * @brief Assigns private family bedrooms and beds to stable couples.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

/**
 * @class HouseholdSystem
 * @brief Handles lightweight household ownership.
 *
 * V1 responsibilities:
 * - detect stable couples from FamilyComponent::partnerId;
 * - assign an available bedroom to the couple;
 * - mark all valid beds inside that bedroom as private family beds;
 * - store ownership through RestSpotComponent::ownerFamilyId and RoomComponent::ownerFamilyId.
 *
 * This system does not create rooms or furniture.
 * It only assigns ownership to already existing detected rooms and beds.
 */
class HouseholdSystem {
public:
    HouseholdSystem() = default;

    void Update(float deltaTime, EntityManager& em);

private:
    float m_updateAccumulator = 0.0f;
};
