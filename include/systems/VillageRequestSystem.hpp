/**
 * @file VillageRequestSystem.hpp
 * @brief Utility system for village social/economic requests.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

#include <string>

class VillageRequestSystem {
public:
    static EntityID CreateRequest(EntityManager& em, VillageRequestType type, EntityID requesterId, EntityID villageId,
                                  const std::string& requestedItemId, int amount, float priority);

    static bool HasOpenRequestByRequester(const EntityManager& em, VillageRequestType type, EntityID requesterId,
                                          const std::string& requestedItemId);

    static EntityID FindOpenRequest(const EntityManager& em, VillageRequestType type, EntityID villageId);

    static bool AssignRequest(EntityManager& em, EntityID requestId, EntityID assigneeId);

    static void CompleteRequest(EntityManager& em, EntityID requestId);

    static void CancelRequest(EntityManager& em, EntityID requestId);

    static EntityID FindAssignedRequestForAssignee(const EntityManager& em, VillageRequestType type, EntityID assigneeId);

private:
    static void ApplySocialImpact(EntityManager& em, EntityID requesterId, EntityID assigneeId, bool success);
};
