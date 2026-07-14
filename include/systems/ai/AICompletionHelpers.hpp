/**
 * @file AICompletionHelpers.hpp
 * @brief Shared helpers for AI completion handlers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

#include <algorithm>
#include <string>

namespace ai::completion {

inline float Clamp100(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

inline bool IsActiveEntity(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity];
}

inline bool IsHuman(const EntityManager& em, EntityID entity) {
    return IsActiveEntity(em, entity) && em.hasTag[entity] && em.tags[entity].species == "human";
}

inline RelationshipEntry& GetOrCreateRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

inline float GetKindness(const EntityManager& em, EntityID entity) {
    if (!IsActiveEntity(em, entity) || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].kindness;
}

inline float GetAggression(const EntityManager& em, EntityID entity) {
    if (!IsActiveEntity(em, entity) || !em.hasPersonality[entity]) {
        return 0.0f;
    }

    return em.personalities[entity].aggression;
}

inline float GetPatience(const EntityManager& em, EntityID entity) {
    if (!IsActiveEntity(em, entity) || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].patience;
}

inline bool HasTrait(const EntityManager& em, EntityID entity, const std::string& traitId) {
    if (!IsActiveEntity(em, entity) || !em.hasPersonality[entity]) {
        return false;
    }

    const std::vector<std::string>& traits = em.personalities[entity].traits;

    return std::find(traits.begin(), traits.end(), traitId) != traits.end();
}

} // namespace ai::completion
