/**
 * @file VillageMenu.cpp
 * @brief Implementation of the village management menu.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "ui/VillageMenu.hpp"

#include "data/BehaviorRegistry.hpp"
#include "data/ProfessionRegistry.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int PANEL_PADDING = 24;
constexpr int TAB_COUNT = 5;                       // Updated for Social Tab
constexpr float BIRTH_FOOD_NUTRITION_COST = 80.0f; // Sourced from VillageSystem

int GetPanelWidth() {
    return std::min(1180, std::max(860, GetScreenWidth() - 80));
}

int GetPanelHeight() {
    return std::min(760, std::max(560, GetScreenHeight() - 80));
}

float GetContentBottomY() {
    const int panelY = (GetScreenHeight() - GetPanelHeight()) / 2;
    return static_cast<float>(panelY + GetPanelHeight() - PANEL_PADDING);
}

float GetContentHeight(float contentY) {
    return std::max(0.0f, GetContentBottomY() - contentY);
}

const char* TabName(VillageMenuTab tab) {
    switch (tab) {
        case VillageMenuTab::Overview:
            return "Overview";
        case VillageMenuTab::Villagers:
            return "Villagers";
        case VillageMenuTab::Professions:
            return "Professions";
        case VillageMenuTab::Storage:
            return "Storage";
        case VillageMenuTab::Social:
            return "Social"; // Added Social label
        default:
            return "Unknown";
    }
}

float ComputeFoodNutrition(const InventoryComponent& inventory, const ResourceRegistry& resourceReg) {
    float total = 0.0f;

    for (const auto& item : inventory.items) {
        if (item.second <= 0) {
            continue;
        }

        const ResourceDef* resource = resourceReg.GetResourceDef(item.first);

        if (resource == nullptr || !resource->isConsumable || resource->nutrition <= 0.0f) {
            continue;
        }

        total += resource->nutrition * static_cast<float>(item.second);
    }

    return total;
}

int CountInventoryItems(const InventoryComponent& inventory) {
    int total = 0;

    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            total += item.second;
        }
    }

    return total;
}

bool IsRestSpotUsableByVillage(const EntityManager& em, EntityID restSpotEntity, EntityID villageId) {
    if (restSpotEntity >= em.active.size() || !em.active[restSpotEntity] || !em.hasRestSpot[restSpotEntity]) {
        return false;
    }

    if (em.hasBlueprint[restSpotEntity] && !em.blueprints[restSpotEntity].isFinished) {
        return false;
    }

    const RestSpotComponent& restSpot = em.restSpots[restSpotEntity];

    if (restSpot.isPrivate) {
        return restSpot.ownerVillageId == villageId;
    }

    if (restSpot.ownerVillageId == static_cast<EntityID>(-1)) {
        return true;
    }

    return restSpot.ownerVillageId == villageId;
}

int CountHousingCapacityForVillage(const EntityManager& em, EntityID villageId) {
    int capacity = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsRestSpotUsableByVillage(em, entity, villageId)) {
            continue;
        }

        capacity += std::max(0, em.restSpots[entity].capacity);
    }

    return capacity;
}

int CountSleepingOccupantsForVillage(const EntityManager& em, EntityID villageId) {
    int sleeping = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!IsRestSpotUsableByVillage(em, entity, villageId)) {
            continue;
        }

        sleeping += static_cast<int>(em.restSpots[entity].occupants.size());
    }

    return sleeping;
}

int GetFreeHousingSlots(const EntityManager& em, EntityID villageId) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId]) {
        return 0;
    }

    const int housingCapacity = CountHousingCapacityForVillage(em, villageId);
    const int population = em.villages[villageId].currentPopulation;

    return std::max(0, housingCapacity - population);
}

std::string GetBirthStatusText(const EntityManager& em, const ResourceRegistry& resourceReg, EntityID villageId) {
    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId]) {
        return "no village";
    }

    const VillageComponent& village = em.villages[villageId];

    if (village.currentPopulation >= village.populationLimit) {
        return "blocked: population limit";
    }

    const int housingCapacity = CountHousingCapacityForVillage(em, villageId);

    if (village.currentPopulation + 1 > housingCapacity) {
        return "blocked: no free housing";
    }

    if (!em.hasInventory[villageId]) {
        return "blocked: no village storage";
    }

    const float foodNutrition = ComputeFoodNutrition(em.inventories[villageId], resourceReg);

    if (foodNutrition < BIRTH_FOOD_NUTRITION_COST) {
        return "blocked: not enough food";
    }

    if (village.adultPopulation < 2) {
        return "blocked: not enough adults";
    }

    return "possible";
}

std::vector<EntityID> GetVillageMembers(const EntityManager& em, EntityID villageId) {
    std::vector<EntityID> members;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasVillageMember[entity] || !em.hasTag[entity]) {
            continue;
        }

        if (em.villageMembers[entity].villageId != villageId) {
            continue;
        }

        members.push_back(entity);
    }

    std::sort(members.begin(), members.end(), [&em](EntityID lhs, EntityID rhs) {
        const std::string& lhsName = em.tags[lhs].firstName.empty() ? em.tags[lhs].name : em.tags[lhs].firstName;
        const std::string& rhsName = em.tags[rhs].firstName.empty() ? em.tags[rhs].name : em.tags[rhs].firstName;

        return lhsName < rhsName;
    });

    return members;
}

std::string GetDisplayName(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTag[entity]) {
        return "Unknown";
    }

    const TagComponent& tag = em.tags[entity];

    if (!tag.firstName.empty()) {
        return tag.firstName;
    }

    if (!tag.name.empty()) {
        return tag.name;
    }

    return "Entity #" + std::to_string(entity);
}

// ---------------------------------------------------------
// Social Helpers
// ---------------------------------------------------------

const RelationshipEntry* FindRelationship(const EntityManager& em, EntityID owner, EntityID other) {
    if (owner >= em.active.size() || !em.active[owner] || !em.hasSocial[owner]) {
        return nullptr;
    }

    const SocialComponent& social = em.socials[owner];

    for (const RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return &relationship;
        }
    }
    return nullptr;
}

bool HasPartnerLink(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasFamily[a] || !em.hasFamily[b]) {
        return false;
    }

    return em.families[a].partnerId == b && em.families[b].partnerId == a;
}

bool IsParentOf(const EntityManager& em, EntityID possibleParent, EntityID possibleChild) {
    if (possibleChild >= em.active.size() || !em.active[possibleChild] || !em.hasFamily[possibleChild]) {
        return false;
    }

    const FamilyComponent& family = em.families[possibleChild];
    return family.parentA == possibleParent || family.parentB == possibleParent;
}

bool IsChildOf(const EntityManager& em, EntityID possibleChild, EntityID possibleParent) {
    return IsParentOf(em, possibleParent, possibleChild);
}

std::string GetFamilyRelationLabel(const EntityManager& em, EntityID a, EntityID b) {
    if (a == b) {
        return "self";
    }
    if (HasPartnerLink(em, a, b)) {
        return "partner";
    }
    if (IsParentOf(em, a, b)) {
        return "parent";
    }
    if (IsChildOf(em, a, b)) {
        return "child";
    }
    return "none";
}

float Clamp01Ratio(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

float GetTrustValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->trust : 0.0f;
}

float GetRespectValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->respect : 0.0f;
}

float GetResentmentValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->resentment : 0.0f;
}

float GetFearValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->fear : 0.0f;
}

bool IsHostileRelationship(const RelationshipEntry& relationship) {
    return relationship.resentment >= 70.0f && relationship.friendship <= 20.0f;
}

std::string TruncateText(const std::string& text, std::size_t maxLength) {
    if (text.size() <= maxLength) {
        return text;
    }

    if (maxLength <= 3) {
        return text.substr(0, maxLength);
    }

    return text.substr(0, maxLength - 3) + "...";
}

std::string GetTraitsText(const EntityManager& em, EntityID entity, std::size_t maxLength = 42) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return "none";
    }

    const PersonalityComponent& personality = em.personalities[entity];

    if (personality.traits.empty()) {
        return "none";
    }

    std::string result;

    for (std::size_t i = 0; i < personality.traits.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }

        result += personality.traits[i];
    }

    return TruncateText(result, maxLength);
}

struct SocialSummary {
    int relations = 0;
    int friends = 0;
    int hostile = 0;
    int fearLinks = 0;

    float avgFriendship = 0.0f;
    float avgTrust = 0.0f;
    float avgResentment = 0.0f;
};

SocialSummary ComputeSocialSummary(const EntityManager& em, EntityID entity) {
    SocialSummary summary;

    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity]) {
        return summary;
    }

    const SocialComponent& social = em.socials[entity];

    float friendshipSum = 0.0f;
    float trustSum = 0.0f;
    float resentmentSum = 0.0f;

    for (const RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId >= em.active.size() || !em.active[relationship.otherId]) {
            continue;
        }

        summary.relations++;

        friendshipSum += relationship.friendship;
        trustSum += relationship.trust;
        resentmentSum += relationship.resentment;

        if (relationship.friendship >= 50.0f) {
            summary.friends++;
        }

        if (IsHostileRelationship(relationship)) {
            summary.hostile++;
        }

        if (relationship.fear >= 50.0f) {
            summary.fearLinks++;
        }
    }

    if (summary.relations > 0) {
        summary.avgFriendship = friendshipSum / static_cast<float>(summary.relations);
        summary.avgTrust = trustSum / static_cast<float>(summary.relations);
        summary.avgResentment = resentmentSum / static_cast<float>(summary.relations);
    }

    return summary;
}

struct SocialAlert {
    EntityID owner = static_cast<EntityID>(-1);
    EntityID other = static_cast<EntityID>(-1);
    float score = 0.0f;
    std::string label;
};

std::vector<SocialAlert> GetTopSocialAlerts(const EntityManager& em, EntityID villageId, int maxCount) {
    std::vector<SocialAlert> alerts;

    for (EntityID owner = 0; owner < em.active.size(); ++owner) {
        if (!em.active[owner] || !em.hasVillageMember[owner] || !em.hasSocial[owner]) {
            continue;
        }

        if (em.villageMembers[owner].villageId != villageId) {
            continue;
        }

        for (const RelationshipEntry& relationship : em.socials[owner].relationships) {
            const EntityID other = relationship.otherId;

            if (other >= em.active.size() || !em.active[other] || !em.hasVillageMember[other]) {
                continue;
            }

            if (em.villageMembers[other].villageId != villageId) {
                continue;
            }

            const float hostilityScore = relationship.resentment + relationship.fear - relationship.friendship * 0.4f;

            if (hostilityScore < 60.0f) {
                continue;
            }

            std::string label = "tension";

            if (relationship.resentment >= 90.0f && relationship.friendship <= 10.0f) {
                label = "critical hatred";
            } else if (relationship.resentment >= 70.0f) {
                label = "resentment";
            } else if (relationship.fear >= 60.0f) {
                label = "fear";
            }

            alerts.push_back({owner, other, hostilityScore, label});
        }
    }

    std::sort(alerts.begin(), alerts.end(), [](const SocialAlert& lhs, const SocialAlert& rhs) { return lhs.score > rhs.score; });

    if (static_cast<int>(alerts.size()) > maxCount) {
        alerts.resize(maxCount);
    }

    return alerts;
}

std::string GetSocialRelationLabel(const EntityManager& em, EntityID a, EntityID b) {
    const std::string familyRelation = GetFamilyRelationLabel(em, a, b);

    if (familyRelation != "none") {
        return familyRelation;
    }

    const RelationshipEntry* relationship = FindRelationship(em, a, b);

    if (relationship == nullptr) {
        return "neutral";
    }

    if (relationship->resentment >= 90.0f && relationship->friendship <= 10.0f) {
        return "hated";
    }

    if (relationship->resentment >= 70.0f && relationship->friendship <= 20.0f) {
        return "hostile";
    }

    if (relationship->fear >= 60.0f) {
        return "feared";
    }

    if (relationship->romance >= 50.0f) {
        return "romantic interest";
    }

    if (relationship->friendship >= 50.0f) {
        return "friend";
    }

    if (relationship->trust >= 50.0f) {
        return "trusted";
    }

    return "neutral";
}

float GetFriendshipValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->friendship : 0.0f;
}

float GetRomanceValue(const EntityManager& em, EntityID a, EntityID b) {
    const RelationshipEntry* relationship = FindRelationship(em, a, b);
    return relationship != nullptr ? relationship->romance : 0.0f;
}

std::string GetPartnerName(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return "none";
    }

    const EntityID partner = em.families[entity].partnerId;

    if (partner == static_cast<EntityID>(-1) || partner >= em.active.size() || !em.active[partner]) {
        return "none";
    }

    return GetDisplayName(em, partner);
}

std::string GetParentsText(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return "none";
    }

    const FamilyComponent& family = em.families[entity];
    std::string result;

    if (family.parentA != static_cast<EntityID>(-1) && family.parentA < em.active.size() && em.active[family.parentA]) {
        result += GetDisplayName(em, family.parentA);
    }

    if (family.parentB != static_cast<EntityID>(-1) && family.parentB < em.active.size() && em.active[family.parentB]) {
        if (!result.empty()) {
            result += ", ";
        }
        result += GetDisplayName(em, family.parentB);
    }

    return result.empty() ? "none" : result;
}

std::string GetChildrenText(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return "none";
    }

    const FamilyComponent& family = em.families[entity];

    if (family.children.empty()) {
        return "none";
    }

    std::string result;

    for (EntityID child : family.children) {
        if (child >= em.active.size() || !em.active[child]) {
            continue;
        }
        if (!result.empty()) {
            result += ", ";
        }
        result += GetDisplayName(em, child);
    }

    return result.empty() ? "none" : result;
}

// ---------------------------------------------------------
// Drawing Helpers
// ---------------------------------------------------------

void DrawPanelBackground() {
    const int panelW = GetPanelWidth();
    const int panelH = GetPanelHeight();

    const int x = (GetScreenWidth() - panelW) / 2;
    const int y = (GetScreenHeight() - panelH) / 2;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.55f));
    DrawRectangle(x, y, panelW, panelH, ColorAlpha(BLACK, 0.92f));
    DrawRectangleLines(x, y, panelW, panelH, DARKGRAY);
}

void DrawKeyValue(const char* key, const std::string& value, float x, float y, Color valueColor = RAYWHITE) {
    DrawText(key, static_cast<int>(x), static_cast<int>(y), 18, GRAY);

    const int valueWidth = MeasureText(value.c_str(), 18);
    DrawText(value.c_str(), static_cast<int>(x + 330 - valueWidth), static_cast<int>(y), 18, valueColor);
}

void DrawProgressBar(const char* label, float current, float max, float x, float y, Color color) {
    DrawText(label, static_cast<int>(x), static_cast<int>(y), 16, GRAY);

    const float barX = x + 80.0f;
    const float barY = y + 3.0f;
    const float barW = 120.0f;
    const float barH = 14.0f;

    float ratio = max > 0.0f ? current / max : 0.0f;
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    DrawRectangle(static_cast<int>(barX), static_cast<int>(barY), static_cast<int>(barW), static_cast<int>(barH), Fade(DARKGRAY, 0.6f));
    DrawRectangle(static_cast<int>(barX), static_cast<int>(barY), static_cast<int>(barW * ratio), static_cast<int>(barH), color);

    const std::string value = TextFormat("%.0f%%", ratio * 100.0f);
    const int valueWidth = MeasureText(value.c_str(), 14);
    DrawText(value.c_str(), static_cast<int>(barX + barW + 8.0f), static_cast<int>(y), 14, RAYWHITE);
}

void DrawCompactProgressBar(float current, float max, float x, float y, float width, Color color) {
    const float barHeight = 14.0f;

    float ratio = max > 0.0f ? current / max : 0.0f;
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    DrawRectangle(static_cast<int>(x), static_cast<int>(y + 3.0f), static_cast<int>(width), static_cast<int>(barHeight),
                  Fade(DARKGRAY, 0.6f));

    DrawRectangle(static_cast<int>(x), static_cast<int>(y + 3.0f), static_cast<int>(width * ratio), static_cast<int>(barHeight), color);

    const std::string value = TextFormat("%.0f%%", ratio * 100.0f);
    const int valueWidth = MeasureText(value.c_str(), 14);

    DrawText(value.c_str(), static_cast<int>(x + width - valueWidth - 4.0f), static_cast<int>(y + 3.0f), 14, WHITE);
}

struct ProfessionSlotView {
    EntityID workplaceId = static_cast<EntityID>(-1);
    int slotIndex = -1;
    std::string profession;
    EntityID workerId = static_cast<EntityID>(-1);
};

std::vector<ProfessionSlotView> GetProfessionSlots(const EntityManager& em) {
    std::vector<ProfessionSlotView> result;

    for (EntityID workplaceId = 0; workplaceId < em.active.size(); ++workplaceId) {
        if (!em.active[workplaceId] || !em.hasWorkplace[workplaceId]) {
            continue;
        }

        const WorkplaceComponent& workplace = em.workplaces[workplaceId];

        for (int slotIndex = 0; slotIndex < static_cast<int>(workplace.slots.size()); ++slotIndex) {
            const JobSlot& slot = workplace.slots[slotIndex];

            result.push_back({workplaceId, slotIndex, slot.profession, slot.workerId});
        }
    }

    std::sort(result.begin(), result.end(), [](const ProfessionSlotView& lhs, const ProfessionSlotView& rhs) {
        if (lhs.profession != rhs.profession) {
            return lhs.profession < rhs.profession;
        }

        if (lhs.workplaceId != rhs.workplaceId) {
            return lhs.workplaceId < rhs.workplaceId;
        }

        return lhs.slotIndex < rhs.slotIndex;
    });

    return result;
}

bool IsWorkerAssignedToAnySlot(const EntityManager& em, EntityID workerId) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasWorkplace[entity]) {
            continue;
        }

        const WorkplaceComponent& workplace = em.workplaces[entity];

        for (const JobSlot& slot : workplace.slots) {
            if (slot.workerId == workerId) {
                return true;
            }
        }
    }

    return false;
}

void ApplyProfessionBehaviorRules(EntityManager& em, EntityID workerId, const BehaviorRegistry& behaviorReg) {
    if (workerId >= em.active.size() || !em.active[workerId] || !em.hasTag[workerId] || !em.hasProfession[workerId] ||
        !em.hasBehavior[workerId]) {
        return;
    }

    const TagComponent& tag = em.tags[workerId];
    const ProfessionComponent& profession = em.professions[workerId];

    const std::vector<BehaviorRule> rules = behaviorReg.GetBehaviorsFor(tag.category, tag.species, profession.currentProfession);

    em.behaviors[workerId].innateBehaviorRules = rules;
    em.behaviors[workerId].innateCapabilities.clear();

    for (const BehaviorRule& rule : rules) {
        em.behaviors[workerId].innateCapabilities.push_back(rule.name);
    }
}

bool IsEligibleForProfession(const EntityManager& em, EntityID candidate, const std::string& profession,
                             const ProfessionRegistry& professionReg, EntityID villageId) {
    if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTag[candidate] || !em.hasProfession[candidate] ||
        !em.hasBehavior[candidate] || !em.hasVillageMember[candidate]) {
        return false;
    }

    if (em.villageMembers[candidate].villageId != villageId) {
        return false;
    }

    const ProfessionDef* professionDef = professionReg.GetProfession(profession);

    if (professionDef == nullptr) {
        return false;
    }

    const TagComponent& tag = em.tags[candidate];

    if (tag.age < professionDef->minAge) {
        return false;
    }

    if (tag.species != professionDef->reqSpecies) {
        return false;
    }

    return true;
}

std::vector<EntityID> GetEligibleWorkersForProfession(const EntityManager& em, const std::string& profession,
                                                      const ProfessionRegistry& professionReg, EntityID villageId) {
    std::vector<EntityID> result;

    for (EntityID candidate = 0; candidate < em.active.size(); ++candidate) {
        if (IsEligibleForProfession(em, candidate, profession, professionReg, villageId)) {
            result.push_back(candidate);
        }
    }

    std::sort(result.begin(), result.end(),
              [&em](EntityID lhs, EntityID rhs) { return GetDisplayName(em, lhs) < GetDisplayName(em, rhs); });

    return result;
}

bool ReleaseWorkerFromAnySlot(EntityManager& em, EntityID workerId, const BehaviorRegistry& behaviorReg) {
    bool released = false;

    for (EntityID workplaceId = 0; workplaceId < em.active.size(); ++workplaceId) {
        if (!em.active[workplaceId] || !em.hasWorkplace[workplaceId]) {
            continue;
        }

        WorkplaceComponent& workplace = em.workplaces[workplaceId];

        for (JobSlot& slot : workplace.slots) {
            if (slot.workerId == workerId) {
                slot.workerId = static_cast<EntityID>(-1);
                released = true;
            }
        }
    }

    if (workerId < em.active.size() && em.active[workerId] && em.hasProfession[workerId]) {
        em.professions[workerId].currentProfession = "none";
        em.professions[workerId].assignmentMode = ProfessionAssignmentMode::Manual;
        ApplyProfessionBehaviorRules(em, workerId, behaviorReg);
    }

    return released;
}

bool AssignSpecificWorkerToProfessionSlot(EntityManager& em, const ProfessionSlotView& slotView, EntityID workerId,
                                          const BehaviorRegistry& behaviorReg) {
    if (slotView.workplaceId >= em.active.size() || !em.active[slotView.workplaceId] || !em.hasWorkplace[slotView.workplaceId]) {
        return false;
    }

    if (workerId >= em.active.size() || !em.active[workerId] || !em.hasProfession[workerId] || !em.hasBehavior[workerId]) {
        return false;
    }

    WorkplaceComponent& workplace = em.workplaces[slotView.workplaceId];

    if (slotView.slotIndex < 0 || slotView.slotIndex >= static_cast<int>(workplace.slots.size())) {
        return false;
    }

    JobSlot& slot = workplace.slots[slotView.slotIndex];

    if (slot.workerId != static_cast<EntityID>(-1) && slot.workerId != workerId) {
        const EntityID previousWorker = slot.workerId;

        if (previousWorker < em.active.size() && em.active[previousWorker] && em.hasProfession[previousWorker]) {
            em.professions[previousWorker].currentProfession = "none";
            em.professions[previousWorker].assignmentMode = ProfessionAssignmentMode::Manual;
            ApplyProfessionBehaviorRules(em, previousWorker, behaviorReg);
        }
    }

    ReleaseWorkerFromAnySlot(em, workerId, behaviorReg);

    slot.workerId = workerId;
    em.professions[workerId].currentProfession = slot.profession;
    em.professions[workerId].assignmentMode = ProfessionAssignmentMode::Manual;

    ApplyProfessionBehaviorRules(em, workerId, behaviorReg);

    return true;
}

EntityID FindFirstEligibleWorker(const EntityManager& em, const std::string& profession, const ProfessionRegistry& professionReg,
                                 EntityID villageId) {
    for (EntityID candidate = 0; candidate < em.active.size(); ++candidate) {
        if (IsEligibleForProfession(em, candidate, profession, professionReg, villageId)) {
            return candidate;
        }
    }

    return static_cast<EntityID>(-1);
}

bool AssignWorkerToProfessionSlot(EntityManager& em, const ProfessionSlotView& slotView, EntityID workerId,
                                  const BehaviorRegistry& behaviorReg) {
    if (slotView.workplaceId >= em.active.size() || !em.active[slotView.workplaceId] || !em.hasWorkplace[slotView.workplaceId]) {
        return false;
    }

    WorkplaceComponent& workplace = em.workplaces[slotView.workplaceId];

    if (slotView.slotIndex < 0 || slotView.slotIndex >= static_cast<int>(workplace.slots.size())) {
        return false;
    }

    JobSlot& slot = workplace.slots[slotView.slotIndex];

    if (slot.workerId != static_cast<EntityID>(-1)) {
        return false;
    }

    if (workerId >= em.active.size() || !em.active[workerId] || !em.hasProfession[workerId]) {
        return false;
    }

    slot.workerId = workerId;
    em.professions[workerId].currentProfession = slot.profession;

    ApplyProfessionBehaviorRules(em, workerId, behaviorReg);

    return true;
}

bool ReleaseProfessionSlot(EntityManager& em, const ProfessionSlotView& slotView, const BehaviorRegistry& behaviorReg) {
    if (slotView.workplaceId >= em.active.size() || !em.active[slotView.workplaceId] || !em.hasWorkplace[slotView.workplaceId]) {
        return false;
    }

    WorkplaceComponent& workplace = em.workplaces[slotView.workplaceId];

    if (slotView.slotIndex < 0 || slotView.slotIndex >= static_cast<int>(workplace.slots.size())) {
        return false;
    }

    JobSlot& slot = workplace.slots[slotView.slotIndex];
    const EntityID previousWorker = slot.workerId;

    slot.workerId = static_cast<EntityID>(-1);

    if (previousWorker < em.active.size() && em.active[previousWorker] && em.hasProfession[previousWorker]) {
        em.professions[previousWorker].currentProfession = "none";
        em.professions[previousWorker].assignmentMode = ProfessionAssignmentMode::Manual;
        ApplyProfessionBehaviorRules(em, previousWorker, behaviorReg);
    }

    return true;
}

const char* ProfessionModeToString(ProfessionAssignmentMode mode) {
    switch (mode) {
        case ProfessionAssignmentMode::Auto:
            return "auto";
        case ProfessionAssignmentMode::Manual:
            return "manual";
        default:
            return "unknown";
    }
}

EntityID GetFamilyKeyForEntity(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return static_cast<EntityID>(-1);
    }

    const EntityID partner = em.families[entity].partnerId;

    if (partner == static_cast<EntityID>(-1)) {
        return static_cast<EntityID>(-1);
    }

    return std::min(entity, partner);
}

int CountPrivateBedCapacityForFamilyMenu(const EntityManager& em, EntityID familyKey) {
    int capacity = 0;

    if (familyKey == static_cast<EntityID>(-1)) {
        return 0;
    }

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasRestSpot[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (!restSpot.isPrivate || restSpot.ownerFamilyId != familyKey) {
            continue;
        }

        capacity += std::max(0, restSpot.capacity);
    }

    return capacity;
}

int CountOwnedBedsForFamilyMenu(const EntityManager& em, EntityID familyKey) {
    int count = 0;

    if (familyKey == static_cast<EntityID>(-1)) {
        return 0;
    }

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasRestSpot[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (restSpot.isPrivate && restSpot.ownerFamilyId == familyKey) {
            count++;
        }
    }

    return count;
}

int CountFamilyMembersForMenu(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity]) {
        return 1;
    }

    const EntityID partner = em.families[entity].partnerId;

    if (partner == static_cast<EntityID>(-1)) {
        return 1;
    }

    int count = 2;

    for (EntityID child : em.families[entity].children) {
        if (child < em.active.size() && em.active[child]) {
            count++;
        }
    }

    return count;
}

} // namespace

void VillageMenu::Update(EntityManager& em, GameCamera& camera, const ProfessionRegistry& professionReg,
                         const BehaviorRegistry& behaviorReg) {
    if (IsKeyPressed(KEY_V)) {
        m_isOpen = !m_isOpen;
    }

    if (!m_isOpen) {
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        m_isOpen = false;
        return;
    }

    if (IsKeyPressed(KEY_RIGHT)) {
        SetTabIndex((GetTabIndex() + 1) % TAB_COUNT);
    }

    if (IsKeyPressed(KEY_LEFT)) {
        SetTabIndex((GetTabIndex() - 1 + TAB_COUNT) % TAB_COUNT);
    }

    const EntityID villageId = FindPrimaryVillage(em);

    if (villageId == static_cast<EntityID>(-1)) {
        return;
    }

    // =========================================================
    // Villagers tab
    // =========================================================
    if (m_currentTab == VillageMenuTab::Villagers) {
        const std::vector<EntityID> members = GetVillageMembers(em, villageId);

        if (members.empty()) {
            m_selectedVillagerIndex = 0;
            return;
        }

        if (m_selectedVillagerIndex >= static_cast<int>(members.size())) {
            m_selectedVillagerIndex = static_cast<int>(members.size()) - 1;
        }

        if (m_selectedVillagerIndex < 0) {
            m_selectedVillagerIndex = 0;
        }

        if (IsKeyPressed(KEY_DOWN)) {
            m_selectedVillagerIndex = (m_selectedVillagerIndex + 1) % static_cast<int>(members.size());
        }

        if (IsKeyPressed(KEY_UP)) {
            m_selectedVillagerIndex = (m_selectedVillagerIndex - 1 + static_cast<int>(members.size())) % static_cast<int>(members.size());
        }

        const EntityID selected = members[m_selectedVillagerIndex];

        if (IsKeyPressed(KEY_ENTER)) {
            if (selected < em.active.size() && em.active[selected] && em.hasTransform[selected]) {
                camera.SetTarget(em.transforms[selected].position);
            }
        }

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_R)) {
            ReleaseWorkerFromAnySlot(em, selected, behaviorReg);
        }

        if (IsKeyPressed(KEY_A)) {
            if (selected < em.active.size() && em.active[selected] && em.hasProfession[selected]) {
                ReleaseWorkerFromAnySlot(em, selected, behaviorReg);
                em.professions[selected].assignmentMode = ProfessionAssignmentMode::Auto;
            }
        }

        return;
    }

    // =========================================================
    // Professions tab
    // =========================================================
    if (m_currentTab == VillageMenuTab::Professions) {
        const std::vector<ProfessionSlotView> slots = GetProfessionSlots(em);

        if (slots.empty()) {
            m_selectedProfessionSlotIndex = 0;
            m_selectedProfessionCandidateIndex = 0;
            return;
        }

        if (m_selectedProfessionSlotIndex >= static_cast<int>(slots.size())) {
            m_selectedProfessionSlotIndex = static_cast<int>(slots.size()) - 1;
        }

        if (m_selectedProfessionSlotIndex < 0) {
            m_selectedProfessionSlotIndex = 0;
        }

        if (IsKeyPressed(KEY_DOWN)) {
            m_selectedProfessionSlotIndex = (m_selectedProfessionSlotIndex + 1) % static_cast<int>(slots.size());

            m_selectedProfessionCandidateIndex = 0;
        }

        if (IsKeyPressed(KEY_UP)) {
            m_selectedProfessionSlotIndex =
                (m_selectedProfessionSlotIndex - 1 + static_cast<int>(slots.size())) % static_cast<int>(slots.size());

            m_selectedProfessionCandidateIndex = 0;
        }

        const ProfessionSlotView& selectedSlot = slots[m_selectedProfessionSlotIndex];

        const std::vector<EntityID> candidates = GetEligibleWorkersForProfession(em, selectedSlot.profession, professionReg, villageId);

        if (!candidates.empty()) {
            if (m_selectedProfessionCandidateIndex >= static_cast<int>(candidates.size())) {
                m_selectedProfessionCandidateIndex = static_cast<int>(candidates.size()) - 1;
            }

            if (m_selectedProfessionCandidateIndex < 0) {
                m_selectedProfessionCandidateIndex = 0;
            }

            if (IsKeyPressed(KEY_S)) {
                m_selectedProfessionCandidateIndex = (m_selectedProfessionCandidateIndex + 1) % static_cast<int>(candidates.size());
            }

            if (IsKeyPressed(KEY_W)) {
                m_selectedProfessionCandidateIndex =
                    (m_selectedProfessionCandidateIndex - 1 + static_cast<int>(candidates.size())) % static_cast<int>(candidates.size());
            }
        } else {
            m_selectedProfessionCandidateIndex = 0;
        }

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_R)) {
            ReleaseProfessionSlot(em, selectedSlot, behaviorReg);
            return;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (candidates.empty()) {
                return;
            }

            const EntityID selectedWorker = candidates[m_selectedProfessionCandidateIndex];

            AssignSpecificWorkerToProfessionSlot(em, selectedSlot, selectedWorker, behaviorReg);

            return;
        }

        if (IsKeyPressed(KEY_F)) {
            EntityID focusTarget = static_cast<EntityID>(-1);

            if (selectedSlot.workerId != static_cast<EntityID>(-1)) {
                focusTarget = selectedSlot.workerId;
            } else if (!candidates.empty()) {
                focusTarget = candidates[m_selectedProfessionCandidateIndex];
            }

            if (focusTarget != static_cast<EntityID>(-1) && focusTarget < em.active.size() && em.active[focusTarget] &&
                em.hasTransform[focusTarget]) {
                camera.SetTarget(em.transforms[focusTarget].position);
            }
        }

        return;
    }

    // =========================================================
    // Social tab
    // =========================================================
    if (m_currentTab == VillageMenuTab::Social) {
        const std::vector<EntityID> members = GetVillageMembers(em, villageId);

        if (members.empty()) {
            m_selectedSocialPrimaryIndex = 0;
            m_selectedSocialTargetIndex = 0;
            return;
        }

        if (m_selectedSocialPrimaryIndex >= static_cast<int>(members.size())) {
            m_selectedSocialPrimaryIndex = static_cast<int>(members.size()) - 1;
        }

        if (m_selectedSocialTargetIndex >= static_cast<int>(members.size())) {
            m_selectedSocialTargetIndex = static_cast<int>(members.size()) - 1;
        }

        if (m_selectedSocialPrimaryIndex < 0) {
            m_selectedSocialPrimaryIndex = 0;
        }

        if (m_selectedSocialTargetIndex < 0) {
            m_selectedSocialTargetIndex = 0;
        }

        if (IsKeyPressed(KEY_DOWN)) {
            m_selectedSocialPrimaryIndex = (m_selectedSocialPrimaryIndex + 1) % static_cast<int>(members.size());
        }

        if (IsKeyPressed(KEY_UP)) {
            m_selectedSocialPrimaryIndex =
                (m_selectedSocialPrimaryIndex - 1 + static_cast<int>(members.size())) % static_cast<int>(members.size());
        }

        if (IsKeyPressed(KEY_S)) {
            m_selectedSocialTargetIndex = (m_selectedSocialTargetIndex + 1) % static_cast<int>(members.size());
        }

        if (IsKeyPressed(KEY_W)) {
            m_selectedSocialTargetIndex =
                (m_selectedSocialTargetIndex - 1 + static_cast<int>(members.size())) % static_cast<int>(members.size());
        }

        if (IsKeyPressed(KEY_F)) {
            const EntityID selected = members[m_selectedSocialPrimaryIndex];

            if (selected < em.active.size() && em.active[selected] && em.hasTransform[selected]) {
                camera.SetTarget(em.transforms[selected].position);
            }
        }

        return;
    }
}

void VillageMenu::Render(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem,
                         const ProfessionRegistry& professionReg) const {
    if (!m_isOpen) {
        return;
    }

    DrawPanelBackground();

    const int panelW = GetPanelWidth();
    const int panelH = GetPanelHeight();

    const int panelX = (GetScreenWidth() - panelW) / 2;
    const int panelY = (GetScreenHeight() - panelH) / 2;

    const float x = static_cast<float>(panelX + PANEL_PADDING);
    const float y = static_cast<float>(panelY + PANEL_PADDING);

    DrawText("VILLAGE MANAGEMENT", static_cast<int>(x), static_cast<int>(y), 28, GOLD);
    DrawText("V/Esc: close | Left/Right: tabs | Up/Down: select | W/S: candidate | Enter: action | R: unassign | A: auto",
             static_cast<int>(x), static_cast<int>(y + 34), 16, LIGHTGRAY);

    RenderTabs(x, y + 70.0f);

    const EntityID villageId = FindPrimaryVillage(em);

    if (villageId == static_cast<EntityID>(-1)) {
        DrawText("No active village found.", static_cast<int>(x), static_cast<int>(y + 130.0f), 20, RED);
        return;
    }

    const float contentX = x;
    const float contentY = y + 125.0f;

    const int scissorX = panelX + PANEL_PADDING;
    const int scissorY = static_cast<int>(contentY);
    const int scissorW = panelW - PANEL_PADDING * 2;
    const int scissorH = panelY + panelH - PANEL_PADDING - scissorY;

    BeginScissorMode(scissorX, scissorY, scissorW, scissorH);

    switch (m_currentTab) {
        case VillageMenuTab::Overview:
            RenderOverview(em, resourceReg, timeSystem, villageId, contentX, contentY);
            break;
        case VillageMenuTab::Villagers:
            RenderVillagers(em, villageId, contentX, contentY);
            break;
        case VillageMenuTab::Professions:
            RenderProfessions(em, professionReg, contentX, contentY);
            break;
        case VillageMenuTab::Storage:
            RenderStorage(em, resourceReg, villageId, contentX, contentY);
            break;
        case VillageMenuTab::Social:
            RenderSocial(em, villageId, contentX, contentY);
            break;
        default:
            break;
    }

    EndScissorMode();
}

EntityID VillageMenu::FindPrimaryVillage(const EntityManager& em) const {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (em.active[entity] && em.hasVillage[entity]) {
            return entity;
        }
    }

    return static_cast<EntityID>(-1);
}

int VillageMenu::GetTabIndex() const {
    return static_cast<int>(m_currentTab);
}

void VillageMenu::SetTabIndex(int index) {
    if (index < 0) {
        index = 0;
    }

    if (index >= TAB_COUNT) {
        index = TAB_COUNT - 1;
    }

    m_currentTab = static_cast<VillageMenuTab>(index);
}

void VillageMenu::RenderTabs(float x, float y) const {
    const float tabSpacing = static_cast<float>(GetPanelWidth() - PANEL_PADDING * 2) / static_cast<float>(TAB_COUNT);
    for (int i = 0; i < TAB_COUNT; ++i) {
        const VillageMenuTab tab = static_cast<VillageMenuTab>(i);
        const bool selected = tab == m_currentTab;

        const float tabX = x + static_cast<float>(i) * tabSpacing;
        const Color color = selected ? YELLOW : GRAY;

        DrawText(TabName(tab), static_cast<int>(tabX), static_cast<int>(y), 22, color);

        if (selected) {
            DrawLine(static_cast<int>(tabX), static_cast<int>(y + 26.0f), static_cast<int>(tabX + tabSpacing - 30.0f),
                     static_cast<int>(y + 26.0f), color);
        }
    }
}

void VillageMenu::RenderOverview(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem,
                                 EntityID villageId, float x, float y) const {
    const VillageComponent& village = em.villages[villageId];

    DrawText("Overview", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    float currentY = y + 42.0f;

    DrawKeyValue("Village", village.name, x, currentY);
    currentY += 28.0f;

    DrawKeyValue("Population", std::to_string(village.currentPopulation) + " / " + std::to_string(village.populationLimit), x, currentY);
    currentY += 28.0f;

    DrawKeyValue("Adults / Children", std::to_string(village.adultPopulation) + " / " + std::to_string(village.childPopulation), x,
                 currentY);
    currentY += 28.0f;

    if (em.hasInventory[villageId]) {
        const int foodNutrition = static_cast<int>(ComputeFoodNutrition(em.inventories[villageId], resourceReg));
        DrawKeyValue("Food nutrition", std::to_string(foodNutrition), x, currentY, foodNutrition > 0 ? GREEN : RED);
        currentY += 28.0f;
    }

    const int housingCapacity = CountHousingCapacityForVillage(em, villageId);
    const int freeHousing = GetFreeHousingSlots(em, villageId);
    const int sleepingOccupants = CountSleepingOccupantsForVillage(em, villageId);

    DrawKeyValue("Housing", std::to_string(village.currentPopulation) + " / " + std::to_string(housingCapacity), x, currentY,
                 freeHousing > 0 ? GREEN : ORANGE);
    currentY += 28.0f;

    DrawKeyValue("Free beds", std::to_string(freeHousing), x, currentY, freeHousing > 0 ? GREEN : ORANGE);
    currentY += 28.0f;

    DrawKeyValue("Sleeping now", std::to_string(sleepingOccupants), x, currentY, sleepingOccupants > 0 ? SKYBLUE : LIGHTGRAY);
    currentY += 28.0f;

    DrawKeyValue("Birth status", GetBirthStatusText(em, resourceReg, villageId), x, currentY,
                 GetFreeHousingSlots(em, villageId) > 0 ? GREEN : ORANGE);
    currentY += 28.0f;

    DrawKeyValue("Day", std::to_string(timeSystem.GetDay()), x, currentY);
    currentY += 28.0f;

    DrawKeyValue("Season", std::string(timeSystem.GetSeasonName()) + " " + std::to_string(timeSystem.GetDayInSeason()) + "/5", x, currentY);
    currentY += 28.0f;

    DrawKeyValue("Hour", TextFormat("%02d:00", static_cast<int>(timeSystem.GetHour())), x, currentY);
}

void VillageMenu::RenderVillagers(const EntityManager& em, EntityID villageId, float x, float y) const {
    const std::vector<EntityID> members = GetVillageMembers(em, villageId);

    DrawText("Villagers", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    float currentY = y + 38.0f;

    constexpr float COL_NAME = 0.0f;
    constexpr float COL_AGE = 180.0f;

    constexpr float COL_PROFESSION = 240.0f;
    constexpr float COL_MODE = 350.0f;
    constexpr float COL_HUNGER = 430.0f;
    constexpr float COL_FATIGUE = 560.0f;
    constexpr float COL_TASK = 700.0f;

    constexpr float BAR_WIDTH = 110.0f;

    DrawText("Name", static_cast<int>(x + COL_NAME), static_cast<int>(currentY), 16, GRAY);
    DrawText("Age", static_cast<int>(x + COL_AGE), static_cast<int>(currentY), 16, GRAY);
    DrawText("Job", static_cast<int>(x + COL_PROFESSION), static_cast<int>(currentY), 16, GRAY);
    DrawText("Mode", static_cast<int>(x + COL_MODE), static_cast<int>(currentY), 16, GRAY);
    DrawText("Hunger", static_cast<int>(x + COL_HUNGER), static_cast<int>(currentY), 16, GRAY);
    DrawText("Fatigue", static_cast<int>(x + COL_FATIGUE), static_cast<int>(currentY), 16, GRAY);
    DrawText("Task", static_cast<int>(x + COL_TASK), static_cast<int>(currentY), 16, GRAY);

    currentY += 26.0f;

    if (members.empty()) {
        DrawText("No villagers found.", static_cast<int>(x), static_cast<int>(currentY), 18, LIGHTGRAY);
        return;
    }

    const int visibleMax = std::max(6, static_cast<int>((GetContentBottomY() - currentY) / 26.0f));
    const int selected = std::max(0, std::min(m_selectedVillagerIndex, static_cast<int>(members.size()) - 1));
    const int start = std::max(0, selected - visibleMax + 1);
    const int end = std::min(static_cast<int>(members.size()), start + visibleMax);

    for (int row = start; row < end; ++row) {
        const EntityID entity = members[row];
        const bool isSelected = row == selected;

        const Color rowColor = isSelected ? YELLOW : RAYWHITE;

        if (isSelected) {
            DrawRectangle(static_cast<int>(x - 8.0f), static_cast<int>(currentY - 3.0f), GetPanelWidth() - PANEL_PADDING * 2, 24,
                          ColorAlpha(DARKGRAY, 0.65f));
        }

        DrawText(TruncateText(GetDisplayName(em, entity), 18).c_str(), static_cast<int>(x + COL_NAME), static_cast<int>(currentY), 16,
                 rowColor);

        if (em.hasTag[entity]) {
            DrawText(std::to_string(em.tags[entity].age).c_str(), static_cast<int>(x + COL_AGE), static_cast<int>(currentY), 16, rowColor);
        }

        if (em.hasProfession[entity]) {
            const ProfessionComponent& profession = em.professions[entity];

            DrawText(TruncateText(profession.currentProfession, 10).c_str(), static_cast<int>(x + COL_PROFESSION),
                     static_cast<int>(currentY), 16, rowColor);

            const bool isManual = profession.assignmentMode == ProfessionAssignmentMode::Manual;

            DrawText(ProfessionModeToString(profession.assignmentMode), static_cast<int>(x + COL_MODE), static_cast<int>(currentY), 16,
                     isManual ? ORANGE : SKYBLUE);
        }

        if (em.hasNeeds[entity]) {
            DrawCompactProgressBar(em.needs[entity].hunger, em.needs[entity].maxHunger, x + COL_HUNGER, currentY, BAR_WIDTH, ORANGE);

            DrawCompactProgressBar(em.needs[entity].fatigue, em.needs[entity].maxFatigue, x + COL_FATIGUE, currentY, BAR_WIDTH, SKYBLUE);
        }

        if (em.hasBehavior[entity]) {
            DrawText(TruncateText(em.behaviors[entity].currentTask, 20).c_str(), static_cast<int>(x + COL_TASK), static_cast<int>(currentY),
                     16, rowColor);
        }

        currentY += 26.0f;
    }
}

void VillageMenu::RenderProfessions(const EntityManager& em, const ProfessionRegistry& professionReg, float x, float y) const {
    DrawText("Professions", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    const std::vector<ProfessionSlotView> slots = GetProfessionSlots(em);

    float currentY = y + 42.0f;

    DrawText("Up/Down: slot | W/S: candidate | Enter: assign | R/Backspace: unassign | F: focus", static_cast<int>(x),
             static_cast<int>(currentY), 16, LIGHTGRAY);

    currentY += 34.0f;

    if (slots.empty()) {
        DrawText("No profession slots available.", static_cast<int>(x), static_cast<int>(currentY), 18, LIGHTGRAY);
        return;
    }

    const int selectedSlotIndex = std::max(0, std::min(m_selectedProfessionSlotIndex, static_cast<int>(slots.size()) - 1));

    const ProfessionSlotView& selectedSlot = slots[selectedSlotIndex];

    const EntityID villageId = FindPrimaryVillage(em);

    const std::vector<EntityID> candidates = villageId == static_cast<EntityID>(-1)
                                                 ? std::vector<EntityID>{}
                                                 : GetEligibleWorkersForProfession(em, selectedSlot.profession, professionReg, villageId);

    const int selectedCandidateIndex =
        candidates.empty() ? 0 : std::max(0, std::min(m_selectedProfessionCandidateIndex, static_cast<int>(candidates.size()) - 1));

    const float leftX = x;
    const float rightX = x + 470.0f;

    DrawText("Slots", static_cast<int>(leftX), static_cast<int>(currentY), 20, YELLOW);
    DrawText("Candidates", static_cast<int>(rightX), static_cast<int>(currentY), 20, YELLOW);

    currentY += 30.0f;

    float slotY = currentY;
    float candidateY = currentY;

    const int visibleSlots = std::max(5, static_cast<int>((GetContentBottomY() - slotY) / 26.0f));
    const int slotStart = std::max(0, selectedSlotIndex - visibleSlots + 1);
    const int slotEnd = std::min(static_cast<int>(slots.size()), slotStart + visibleSlots);

    for (int row = slotStart; row < slotEnd; ++row) {
        const ProfessionSlotView& slot = slots[row];
        const bool isSelected = row == selectedSlotIndex;

        const Color rowColor = isSelected ? YELLOW : RAYWHITE;

        if (isSelected) {
            DrawRectangle(static_cast<int>(leftX - 8.0f), static_cast<int>(slotY - 3.0f), 430, 24, ColorAlpha(DARKGRAY, 0.65f));
        }

        std::string workerName = "empty";

        if (slot.workerId != static_cast<EntityID>(-1) && slot.workerId < em.active.size() && em.active[slot.workerId]) {
            workerName = GetDisplayName(em, slot.workerId);
        }

        const std::string line = slot.profession + " | " + TruncateText(workerName, 14);

        DrawText(line.c_str(), static_cast<int>(leftX), static_cast<int>(slotY), 16, rowColor);

        DrawText(slot.workerId == static_cast<EntityID>(-1) ? "available" : "occupied", static_cast<int>(leftX + 330.0f),
                 static_cast<int>(slotY), 16, slot.workerId == static_cast<EntityID>(-1) ? ORANGE : GREEN);

        slotY += 26.0f;
    }

    DrawText(("Selected: " + selectedSlot.profession).c_str(), static_cast<int>(rightX), static_cast<int>(candidateY), 17, RAYWHITE);

    candidateY += 26.0f;

    if (selectedSlot.workerId != static_cast<EntityID>(-1) && selectedSlot.workerId < em.active.size() &&
        em.active[selectedSlot.workerId]) {
        DrawText(("Current: " + GetDisplayName(em, selectedSlot.workerId)).c_str(), static_cast<int>(rightX), static_cast<int>(candidateY),
                 16, GREEN);
    } else {
        DrawText("Current: none", static_cast<int>(rightX), static_cast<int>(candidateY), 16, ORANGE);
    }

    candidateY += 34.0f;

    if (candidates.empty()) {
        DrawText("No eligible candidates.", static_cast<int>(rightX), static_cast<int>(candidateY), 16, ORANGE);
        return;
    }

    DrawText("Name", static_cast<int>(rightX), static_cast<int>(candidateY), 16, GRAY);
    DrawText("Age", static_cast<int>(rightX + 170.0f), static_cast<int>(candidateY), 16, GRAY);
    DrawText("Current Job", static_cast<int>(rightX + 220.0f), static_cast<int>(candidateY), 16, GRAY);
    DrawText("Mode", static_cast<int>(rightX + 330.0f), static_cast<int>(candidateY), 16, GRAY);

    candidateY += 24.0f;

    const int visibleCandidates = std::max(5, static_cast<int>((GetContentBottomY() - candidateY) / 26.0f));
    const int candidateStart = std::max(0, selectedCandidateIndex - visibleCandidates + 1);
    const int candidateEnd = std::min(static_cast<int>(candidates.size()), candidateStart + visibleCandidates);

    for (int row = candidateStart; row < candidateEnd; ++row) {
        const EntityID candidate = candidates[row];
        const bool isSelected = row == selectedCandidateIndex;

        const Color rowColor = isSelected ? YELLOW : RAYWHITE;

        if (isSelected) {
            DrawRectangle(static_cast<int>(rightX - 8.0f), static_cast<int>(candidateY - 3.0f), 390, 24, ColorAlpha(DARKGRAY, 0.65f));
        }

        DrawText(TruncateText(GetDisplayName(em, candidate), 16).c_str(), static_cast<int>(rightX), static_cast<int>(candidateY), 16,
                 rowColor);

        if (em.hasTag[candidate]) {
            DrawText(std::to_string(em.tags[candidate].age).c_str(), static_cast<int>(rightX + 170.0f), static_cast<int>(candidateY), 16,
                     rowColor);
        }

        std::string currentJob = "none";

        if (em.hasProfession[candidate]) {
            currentJob = em.professions[candidate].currentProfession;
        }

        DrawText(TruncateText(currentJob, 12).c_str(), static_cast<int>(rightX + 220.0f), static_cast<int>(candidateY), 16,
                 currentJob == "none" ? LIGHTGRAY : SKYBLUE);

        if (em.hasProfession[candidate]) {
            const ProfessionAssignmentMode mode = em.professions[candidate].assignmentMode;
            DrawText(ProfessionModeToString(mode), static_cast<int>(rightX + 330.0f), static_cast<int>(candidateY), 16,
                     mode == ProfessionAssignmentMode::Manual ? ORANGE : SKYBLUE);
        }

        candidateY += 26.0f;
    }
}

void VillageMenu::RenderStorage(const EntityManager& em, const ResourceRegistry&, EntityID villageId, float x, float y) const {
    DrawText("Storage", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    float currentY = y + 42.0f;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasInventory[entity] || !em.hasStorage[entity] || !em.hasTag[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        const StorageComponent& storage = em.storages[entity];
        const InventoryComponent& inventory = em.inventories[entity];

        const std::string title = em.tags[entity].name + " #" + std::to_string(entity);
        const int used = CountInventoryItems(inventory);

        DrawText(title.c_str(), static_cast<int>(x), static_cast<int>(currentY), 18, YELLOW);
        DrawText(("Capacity: " + std::to_string(used) + " / " + std::to_string(storage.capacity)).c_str(), static_cast<int>(x + 330.0f),
                 static_cast<int>(currentY), 16, LIGHTGRAY);

        currentY += 24.0f;

        if (!storage.acceptedItems.empty()) {
            std::string filterText = "Filter: ";

            for (size_t i = 0; i < storage.acceptedItems.size(); ++i) {
                if (i > 0) {
                    filterText += ", ";
                }

                filterText += storage.acceptedItems[i];
            }

            DrawText(filterText.c_str(), static_cast<int>(x + 16.0f), static_cast<int>(currentY), 15, GRAY);
            currentY += 22.0f;
        }

        if (inventory.items.empty()) {
            DrawText("Empty", static_cast<int>(x + 16.0f), static_cast<int>(currentY), 15, DARKGRAY);
            currentY += 22.0f;
        } else {
            for (const auto& item : inventory.items) {
                if (item.second <= 0) {
                    continue;
                }

                DrawText((item.first + ": " + std::to_string(item.second)).c_str(), static_cast<int>(x + 16.0f), static_cast<int>(currentY),
                         15, RAYWHITE);
                currentY += 20.0f;
            }
        }

        currentY += 12.0f;

        if (currentY > GetContentBottomY() - 30.0f) {
            DrawText("Storage list truncated.", static_cast<int>(x), static_cast<int>(currentY), 16, ORANGE);
            break;
        }
    }
}

void VillageMenu::RenderSocial(const EntityManager& em, EntityID villageId, float x, float y) const {
    const std::vector<EntityID> members = GetVillageMembers(em, villageId);

    const Color customPink = Color{255, 109, 194, 255};

    DrawText("Social Monitor", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    float currentY = y + 36.0f;

    DrawText("Up/Down: primary | W/S: target | F: focus primary", static_cast<int>(x), static_cast<int>(currentY), 16, LIGHTGRAY);

    currentY += 30.0f;

    if (members.empty()) {
        DrawText("No villagers found.", static_cast<int>(x), static_cast<int>(currentY), 18, LIGHTGRAY);
        return;
    }

    const int primaryIndex = std::max(0, std::min(m_selectedSocialPrimaryIndex, static_cast<int>(members.size()) - 1));
    const int targetIndex = std::max(0, std::min(m_selectedSocialTargetIndex, static_cast<int>(members.size()) - 1));

    const EntityID primary = members[primaryIndex];
    const EntityID target = members[targetIndex];

    const float panelW = static_cast<float>(GetPanelWidth() - PANEL_PADDING * 2);

    const float leftX = x;
    const float midX = x + panelW * 0.31f;
    const float rightX = x + panelW * 0.64f;

    const float leftW = panelW * 0.28f;
    const float midW = panelW * 0.30f;
    const float rightW = panelW * 0.34f;

    // =========================================================
    // Village social overview
    // =========================================================
    const std::vector<SocialAlert> alerts = GetTopSocialAlerts(em, villageId, 5);

    DrawText("Alerts", static_cast<int>(rightX), static_cast<int>(currentY), 18, YELLOW);

    float alertY = currentY + 26.0f;

    if (alerts.empty()) {
        DrawText("No major social tension.", static_cast<int>(rightX), static_cast<int>(alertY), 15, GREEN);
    } else {
        for (const SocialAlert& alert : alerts) {
            const std::string line = TruncateText(GetDisplayName(em, alert.owner), 10) + " -> " +
                                     TruncateText(GetDisplayName(em, alert.other), 10) + " | " + alert.label;

            Color color = ORANGE;

            if (alert.label == "critical hatred") {
                color = RED;
            } else if (alert.label == "fear") {
                color = PURPLE;
            }

            DrawText(line.c_str(), static_cast<int>(rightX), static_cast<int>(alertY), 15, color);
            alertY += 20.0f;
        }
    }

    // =========================================================
    // Left list
    // =========================================================
    DrawText("Villagers", static_cast<int>(leftX), static_cast<int>(currentY), 18, YELLOW);

    float listY = currentY + 26.0f;

    const float bottomY = GetContentBottomY();
    const int visibleMax = std::max(5, static_cast<int>((bottomY - listY) / 24.0f));

    const int start = std::max(0, primaryIndex - visibleMax + 1);
    const int end = std::min(static_cast<int>(members.size()), start + visibleMax);

    for (int row = start; row < end; ++row) {
        const EntityID entity = members[row];

        const bool isPrimary = row == primaryIndex;
        const bool isTarget = row == targetIndex;

        Color rowColor = RAYWHITE;

        if (isPrimary) {
            rowColor = YELLOW;
        } else if (isTarget) {
            rowColor = SKYBLUE;
        }

        if (isPrimary) {
            DrawRectangle(static_cast<int>(leftX - 8.0f), static_cast<int>(listY - 3.0f), static_cast<int>(leftW), 23,
                          ColorAlpha(DARKGRAY, 0.65f));
        }

        std::string prefix = "  ";

        if (isPrimary && isTarget) {
            prefix = "* ";
        } else if (isPrimary) {
            prefix = "> ";
        } else if (isTarget) {
            prefix = "- ";
        }

        std::string stateSuffix;

        if (em.hasNeeds[entity] && em.needs[entity].collapsedFromFatigue) {
            stateSuffix = " [down]";
        } else if (em.hasBehavior[entity]) {
            const std::string& task = em.behaviors[entity].currentTask;

            if (task == "confronting_person" || task == "intimidating_person" || task == "fighting_non_lethal") {
                stateSuffix = " [conflict]";
            } else if (task == "socializing") {
                stateSuffix = " [social]";
            }
        }

        DrawText((prefix + TruncateText(GetDisplayName(em, entity), 18) + stateSuffix).c_str(), static_cast<int>(leftX),
                 static_cast<int>(listY), 15, rowColor);

        listY += 24.0f;
    }

    if (end < static_cast<int>(members.size())) {
        DrawText(("... " + std::to_string(static_cast<int>(members.size()) - end) + " more").c_str(), static_cast<int>(leftX),
                 static_cast<int>(listY), 14, DARKGRAY);
    }

    // =========================================================
    // Middle: selected villager profile
    // =========================================================
    float profileY = currentY;

    DrawText("Profile", static_cast<int>(midX), static_cast<int>(profileY), 18, YELLOW);
    profileY += 26.0f;

    DrawText(("Selected: " + TruncateText(GetDisplayName(em, primary), 22)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 16,
             RAYWHITE);
    profileY += 24.0f;

    if (em.hasProfession[primary]) {
        DrawText(("Job: " + TruncateText(em.professions[primary].currentProfession, 18)).c_str(), static_cast<int>(midX),
                 static_cast<int>(profileY), 15, SKYBLUE);
        profileY += 22.0f;
    }

    if (em.hasPersonality[primary]) {
        const PersonalityComponent& personality = em.personalities[primary];

        DrawText(("Traits: " + GetTraitsText(em, primary, 34)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 15, LIGHTGRAY);
        profileY += 24.0f;

        DrawText("Kind", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
        DrawCompactProgressBar(personality.kindness, 1.0f, midX + 70.0f, profileY, midW - 90.0f, GREEN);
        profileY += 22.0f;

        DrawText("Aggro", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
        DrawCompactProgressBar(personality.aggression, 1.0f, midX + 70.0f, profileY, midW - 90.0f, RED);
        profileY += 22.0f;

        DrawText("Brave", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
        DrawCompactProgressBar(personality.bravery, 1.0f, midX + 70.0f, profileY, midW - 90.0f, ORANGE);
        profileY += 22.0f;

        DrawText("Social", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
        DrawCompactProgressBar(personality.sociability, 1.0f, midX + 70.0f, profileY, midW - 90.0f, SKYBLUE);
        profileY += 28.0f;
    } else {
        DrawText("Traits: none", static_cast<int>(midX), static_cast<int>(profileY), 15, DARKGRAY);
        profileY += 28.0f;
    }

    const SocialSummary summary = ComputeSocialSummary(em, primary);

    DrawText("Social summary", static_cast<int>(midX), static_cast<int>(profileY), 16, YELLOW);
    profileY += 24.0f;

    DrawText(("Relations: " + std::to_string(summary.relations)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 15,
             LIGHTGRAY);
    profileY += 20.0f;

    DrawText(("Friends: " + std::to_string(summary.friends)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 15,
             summary.friends > 0 ? SKYBLUE : LIGHTGRAY);
    profileY += 20.0f;

    DrawText(("Hostile: " + std::to_string(summary.hostile)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 15,
             summary.hostile > 0 ? RED : LIGHTGRAY);
    profileY += 20.0f;

    DrawText(("Fear links: " + std::to_string(summary.fearLinks)).c_str(), static_cast<int>(midX), static_cast<int>(profileY), 15,
             summary.fearLinks > 0 ? PURPLE : LIGHTGRAY);
    profileY += 26.0f;

    DrawText("Avg friendship", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
    DrawCompactProgressBar(summary.avgFriendship, 100.0f, midX + 115.0f, profileY, midW - 135.0f, SKYBLUE);
    profileY += 22.0f;

    DrawText("Avg trust", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
    DrawCompactProgressBar(summary.avgTrust, 100.0f, midX + 115.0f, profileY, midW - 135.0f, GREEN);
    profileY += 22.0f;

    DrawText("Avg resentment", static_cast<int>(midX), static_cast<int>(profileY), 14, GRAY);
    DrawCompactProgressBar(summary.avgResentment, 100.0f, midX + 115.0f, profileY, midW - 135.0f, RED);

    // =========================================================
    // Right: relationship detail
    // =========================================================
    float relationY = alertY + 22.0f;

    DrawText("Selected relationship", static_cast<int>(rightX), static_cast<int>(relationY), 18, YELLOW);
    relationY += 28.0f;

    DrawText(("Target: " + TruncateText(GetDisplayName(em, target), 22)).c_str(), static_cast<int>(rightX), static_cast<int>(relationY), 16,
             RAYWHITE);
    relationY += 26.0f;

    const std::string relationLabel = GetSocialRelationLabel(em, primary, target);

    Color relationColor = LIGHTGRAY;

    if (relationLabel == "partner") {
        relationColor = GREEN;
    } else if (relationLabel == "romantic interest") {
        relationColor = customPink;
    } else if (relationLabel == "friend" || relationLabel == "trusted") {
        relationColor = SKYBLUE;
    } else if (relationLabel == "parent" || relationLabel == "child") {
        relationColor = GOLD;
    } else if (relationLabel == "hostile" || relationLabel == "hated") {
        relationColor = RED;
    } else if (relationLabel == "feared") {
        relationColor = PURPLE;
    }

    DrawText(("Relation: " + relationLabel).c_str(), static_cast<int>(rightX), static_cast<int>(relationY), 17, relationColor);
    relationY += 30.0f;

    const float friendship = GetFriendshipValue(em, primary, target);
    const float romance = GetRomanceValue(em, primary, target);
    const float trust = GetTrustValue(em, primary, target);
    const float respect = GetRespectValue(em, primary, target);
    const float resentment = GetResentmentValue(em, primary, target);
    const float fear = GetFearValue(em, primary, target);

    DrawText("Friend", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(friendship, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, SKYBLUE);
    relationY += 23.0f;

    DrawText("Romance", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(romance, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, customPink);
    relationY += 23.0f;

    DrawText("Trust", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(trust, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, GREEN);
    relationY += 23.0f;

    DrawText("Respect", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(respect, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, GOLD);
    relationY += 23.0f;

    DrawText("Resent", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(resentment, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, RED);
    relationY += 23.0f;

    DrawText("Fear", static_cast<int>(rightX), static_cast<int>(relationY), 14, GRAY);
    DrawCompactProgressBar(fear, 100.0f, rightX + 95.0f, relationY, rightW - 120.0f, PURPLE);
    relationY += 30.0f;

    if (primary == target) {
        DrawText("Select another target to inspect relationship.", static_cast<int>(rightX), static_cast<int>(relationY), 15, DARKGRAY);
    } else if (resentment >= 95.0f && friendship <= 5.0f) {
        DrawText("Risk: extreme hostility.", static_cast<int>(rightX), static_cast<int>(relationY), 16, RED);
    } else if (resentment >= 70.0f) {
        DrawText("Risk: confrontation likely.", static_cast<int>(rightX), static_cast<int>(relationY), 16, ORANGE);
    } else if (fear >= 60.0f) {
        DrawText("Risk: avoidance likely.", static_cast<int>(rightX), static_cast<int>(relationY), 16, PURPLE);
    } else {
        DrawText("No major risk detected.", static_cast<int>(rightX), static_cast<int>(relationY), 15, GREEN);
    }
}

EntityID VillageMenu::GetSelectedVillager(const EntityManager& em, EntityID villageId) const {
    const std::vector<EntityID> members = GetVillageMembers(em, villageId);

    if (members.empty()) {
        return static_cast<EntityID>(-1);
    }

    const int index = std::max(0, std::min(m_selectedVillagerIndex, static_cast<int>(members.size()) - 1));

    return members[index];
}
