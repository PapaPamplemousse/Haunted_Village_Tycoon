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

        default:
            return std::nullopt;
    }
}

} // namespace AIIntentFinder
