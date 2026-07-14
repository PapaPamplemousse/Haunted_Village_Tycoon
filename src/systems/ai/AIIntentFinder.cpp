/**
 * @file AIIntentFinder.cpp
 * @brief Routes AI task types to domain-specific intent finders.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AIIntentFinder.hpp"

#include "systems/ai/AIIntentFinders.hpp"

namespace AIIntentFinder {

std::optional<AIIntent> FindIntentForTask(EntityID entity, AITaskType taskType, EntityManager& em, const WorldMap& map,
                                          const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                          const EntitySpatialGrid& spatialGrid) {
    switch (taskType) {
        case AITaskType::Wander:
            return ai::intents::FindWanderIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::ReturnToVillageCore:
            return ai::intents::FindReturnToVillageCoreIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Socialize:
            return ai::intents::FindSocializeIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Pray:
            return ai::intents::FindPrayIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::AvoidPerson:
            return ai::intents::FindAvoidPersonIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::ConfrontPerson:
            return ai::intents::FindConfrontPersonIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Preach:
            return ai::intents::FindPreachIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::HoldRitual:
            return ai::intents::FindHoldRitualIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::ComfortFrightened:
            return ai::intents::FindComfortFrightenedIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Intimidate:
            return ai::intents::FindIntimidateIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::FightNonLethal:
            return ai::intents::FindFightNonLethalIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Murder:
            return ai::intents::FindMurderIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::SeekFood:
            return ai::intents::FindSeekFoodIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Rest:
            return ai::intents::FindRestIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::CareChildFood:
            return ai::intents::FindCareChildFoodIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Store:
            return ai::intents::FindStoreIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::EquipWeapon:
            return ai::intents::FindEquipWeaponIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::RequestWeapon:
            return ai::intents::FindRequestWeaponIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::FulfillWeaponRequest:
            return ai::intents::FindFulfillWeaponRequestIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Haul:
            return ai::intents::FindHaulIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Flee:
            return ai::intents::FindFleeIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Defend:
            return ai::intents::FindDefendIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Hunt:
            return ai::intents::FindHuntIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Guard:
            return ai::intents::FindGuardIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Patrol:
            return ai::intents::FindPatrolIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Build:
            return ai::intents::FindBuildIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Dismantle:
            return ai::intents::FindDismantleIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Repair:
            return ai::intents::FindRepairIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Harvest:
            return ai::intents::FindHarvestIntent(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        default:
            return std::nullopt;
    }
}

} // namespace AIIntentFinder
