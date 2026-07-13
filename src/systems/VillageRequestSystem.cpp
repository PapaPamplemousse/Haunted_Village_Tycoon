/**
 * @file VillageRequestSystem.cpp
 * @brief Implementation of village social/economic request utilities.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/VillageRequestSystem.hpp"

#include <algorithm>
#include <iostream>

namespace {

bool IsValidEntity(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity];
}

RelationshipEntry& GetOrCreateRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

const char* RequestTypeToString(VillageRequestType type) {
    switch (type) {
        case VillageRequestType::WeaponNeeded:
            return "WeaponNeeded";
        case VillageRequestType::ToolNeeded:
            return "ToolNeeded";
        case VillageRequestType::FoodNeeded:
            return "FoodNeeded";
        case VillageRequestType::ChildFoodNeeded:
            return "ChildFoodNeeded";
        case VillageRequestType::RepairNeeded:
            return "RepairNeeded";
        case VillageRequestType::MedicineNeeded:
            return "MedicineNeeded";
        case VillageRequestType::FuelNeeded:
            return "FuelNeeded";
        default:
            return "Unknown";
    }
}

} // namespace

EntityID VillageRequestSystem::CreateRequest(EntityManager& em, VillageRequestType type, EntityID requesterId, EntityID villageId,
                                             const std::string& requestedItemId, int amount, float priority) {
    if (!IsValidEntity(em, requesterId) || !IsValidEntity(em, villageId)) {
        return static_cast<EntityID>(-1);
    }

    if (HasOpenRequestByRequester(em, type, requesterId, requestedItemId)) {
        return static_cast<EntityID>(-1);
    }

    EntityID requestId = em.CreateEntity();

    em.hasVillageRequest[requestId] = true;
    em.villageRequests[requestId] = {};

    VillageRequestComponent& request = em.villageRequests[requestId];

    request.type = type;
    request.status = VillageRequestStatus::Open;
    request.requesterId = requesterId;
    request.assigneeId = static_cast<EntityID>(-1);
    request.targetEntityId = static_cast<EntityID>(-1);
    request.villageId = villageId;
    request.requestedItemId = requestedItemId;
    request.amount = std::max(1, amount);
    request.priority = priority;
    request.socialImpactApplied = false;

    std::cout << "[REQUEST] Created " << RequestTypeToString(type) << " request for " << requestedItemId << " by entity #" << requesterId
              << "." << std::endl;

    return requestId;
}

bool VillageRequestSystem::HasOpenRequestByRequester(const EntityManager& em, VillageRequestType type, EntityID requesterId,
                                                     const std::string& requestedItemId) {
    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasVillageRequest[id]) {
            continue;
        }

        const VillageRequestComponent& request = em.villageRequests[id];

        if (request.status != VillageRequestStatus::Open && request.status != VillageRequestStatus::Assigned) {
            continue;
        }

        if (request.type == type && request.requesterId == requesterId && request.requestedItemId == requestedItemId) {
            return true;
        }
    }

    return false;
}

EntityID VillageRequestSystem::FindOpenRequest(const EntityManager& em, VillageRequestType type, EntityID villageId) {
    EntityID bestRequest = static_cast<EntityID>(-1);
    float bestPriority = -1.0f;

    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasVillageRequest[id]) {
            continue;
        }

        const VillageRequestComponent& request = em.villageRequests[id];

        if (request.status != VillageRequestStatus::Open) {
            continue;
        }

        if (request.type != type || request.villageId != villageId) {
            continue;
        }

        if (request.priority > bestPriority) {
            bestPriority = request.priority;
            bestRequest = id;
        }
    }

    return bestRequest;
}

bool VillageRequestSystem::AssignRequest(EntityManager& em, EntityID requestId, EntityID assigneeId) {
    if (!IsValidEntity(em, requestId) || !em.hasVillageRequest[requestId] || !IsValidEntity(em, assigneeId)) {
        return false;
    }

    VillageRequestComponent& request = em.villageRequests[requestId];

    if (request.status == VillageRequestStatus::Assigned && request.assigneeId == assigneeId) {
        return true;
    }

    if (request.status != VillageRequestStatus::Open) {
        return false;
    }

    request.status = VillageRequestStatus::Assigned;
    request.assigneeId = assigneeId;

    return true;
}

void VillageRequestSystem::CompleteRequest(EntityManager& em, EntityID requestId) {
    if (!IsValidEntity(em, requestId) || !em.hasVillageRequest[requestId]) {
        return;
    }

    VillageRequestComponent& request = em.villageRequests[requestId];

    request.status = VillageRequestStatus::Completed;

    ApplySocialImpact(em, request.requesterId, request.assigneeId, true);
    request.socialImpactApplied = true;

    std::cout << "[REQUEST] Completed request #" << requestId << "." << std::endl;

    em.DestroyEntity(requestId);
}

void VillageRequestSystem::CancelRequest(EntityManager& em, EntityID requestId) {
    if (!IsValidEntity(em, requestId) || !em.hasVillageRequest[requestId]) {
        return;
    }

    VillageRequestComponent& request = em.villageRequests[requestId];

    request.status = VillageRequestStatus::Cancelled;

    ApplySocialImpact(em, request.requesterId, request.assigneeId, false);
    request.socialImpactApplied = true;

    std::cout << "[REQUEST] Cancelled request #" << requestId << "." << std::endl;

    em.DestroyEntity(requestId);
}

void VillageRequestSystem::ApplySocialImpact(EntityManager& em, EntityID requesterId, EntityID assigneeId, bool success) {
    if (!IsValidEntity(em, requesterId) || !IsValidEntity(em, assigneeId) || requesterId == assigneeId || !em.hasSocial[requesterId] ||
        !em.hasSocial[assigneeId]) {
        return;
    }

    RelationshipEntry& requesterToAssignee = GetOrCreateRelationship(em, requesterId, assigneeId);
    RelationshipEntry& assigneeToRequester = GetOrCreateRelationship(em, assigneeId, requesterId);

    if (success) {
        requesterToAssignee.friendship = std::min(100.0f, requesterToAssignee.friendship + 8.0f);
        assigneeToRequester.friendship = std::min(100.0f, assigneeToRequester.friendship + 4.0f);
    } else {
        requesterToAssignee.friendship = std::max(0.0f, requesterToAssignee.friendship - 6.0f);
        assigneeToRequester.friendship = std::max(0.0f, assigneeToRequester.friendship - 2.0f);
    }
}

EntityID VillageRequestSystem::FindAssignedRequestForAssignee(const EntityManager& em, VillageRequestType type, EntityID assigneeId) {
    EntityID bestRequest = static_cast<EntityID>(-1);
    float bestPriority = -1.0f;

    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasVillageRequest[id]) {
            continue;
        }

        const VillageRequestComponent& request = em.villageRequests[id];

        if (request.status != VillageRequestStatus::Assigned) {
            continue;
        }

        if (request.type != type || request.assigneeId != assigneeId) {
            continue;
        }

        if (request.priority > bestPriority) {
            bestPriority = request.priority;
            bestRequest = id;
        }
    }

    return bestRequest;
}
