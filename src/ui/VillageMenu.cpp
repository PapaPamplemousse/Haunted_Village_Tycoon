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

constexpr int PANEL_WIDTH = 900;
constexpr int PANEL_HEIGHT = 560;
constexpr int PANEL_PADDING = 24;
constexpr int TAB_COUNT = 4;

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

int CountUsedBedsForVillage(const EntityManager& em, EntityID villageId) {
    int used = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasRestSpot[entity]) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (restSpot.ownerVillageId != static_cast<EntityID>(-1) && restSpot.ownerVillageId != villageId) {
            continue;
        }

        used += static_cast<int>(restSpot.occupants.size());
    }

    return used;
}

int CountTotalBedsForVillage(const EntityManager& em, EntityID villageId) {
    int total = 0;

    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasRestSpot[entity]) {
            continue;
        }

        if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
            continue;
        }

        const RestSpotComponent& restSpot = em.restSpots[entity];

        if (restSpot.ownerVillageId != static_cast<EntityID>(-1) && restSpot.ownerVillageId != villageId) {
            continue;
        }

        total += restSpot.capacity;
    }

    return total;
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

void DrawPanelBackground() {
    const int x = (GetScreenWidth() - PANEL_WIDTH) / 2;
    const int y = (GetScreenHeight() - PANEL_HEIGHT) / 2;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.55f));
    DrawRectangle(x, y, PANEL_WIDTH, PANEL_HEIGHT, ColorAlpha(BLACK, 0.92f));
    DrawRectangleLines(x, y, PANEL_WIDTH, PANEL_HEIGHT, DARKGRAY);
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

std::string TruncateText(const std::string& text, std::size_t maxLength) {
    if (text.size() <= maxLength) {
        return text;
    }

    if (maxLength <= 3) {
        return text.substr(0, maxLength);
    }

    return text.substr(0, maxLength - 3) + "...";
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

    if (IsWorkerAssignedToAnySlot(em, candidate)) {
        return false;
    }

    const ProfessionComponent& currentProfession = em.professions[candidate];

    if (currentProfession.currentProfession != "none") {
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
    const EntityID workerId = slot.workerId;

    slot.workerId = static_cast<EntityID>(-1);

    if (workerId < em.active.size() && em.active[workerId] && em.hasProfession[workerId]) {
        em.professions[workerId].currentProfession = "none";
        ApplyProfessionBehaviorRules(em, workerId, behaviorReg);
    }

    return true;
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

    if (m_currentTab == VillageMenuTab::Villagers) {
        const std::vector<EntityID> members = GetVillageMembers(em, villageId);

        if (!members.empty()) {
            if (IsKeyPressed(KEY_DOWN)) {
                m_selectedVillagerIndex = (m_selectedVillagerIndex + 1) % static_cast<int>(members.size());
            }

            if (IsKeyPressed(KEY_UP)) {
                m_selectedVillagerIndex =
                    (m_selectedVillagerIndex - 1 + static_cast<int>(members.size())) % static_cast<int>(members.size());
            }

            if (IsKeyPressed(KEY_ENTER)) {
                const EntityID selected = GetSelectedVillager(em, villageId);

                if (selected != static_cast<EntityID>(-1) && selected < em.active.size() && em.active[selected] &&
                    em.hasTransform[selected]) {
                    camera.SetTarget(em.transforms[selected].position);
                }
            }
        } else {
            m_selectedVillagerIndex = 0;
        }

        return;
    }

    if (m_currentTab == VillageMenuTab::Professions) {
        const std::vector<ProfessionSlotView> slots = GetProfessionSlots(em);

        if (slots.empty()) {
            m_selectedProfessionSlotIndex = 0;
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
        }

        if (IsKeyPressed(KEY_UP)) {
            m_selectedProfessionSlotIndex =
                (m_selectedProfessionSlotIndex - 1 + static_cast<int>(slots.size())) % static_cast<int>(slots.size());
        }

        const ProfessionSlotView& selectedSlot = slots[m_selectedProfessionSlotIndex];

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_R)) {
            ReleaseProfessionSlot(em, selectedSlot, behaviorReg);
            return;
        }

        if (IsKeyPressed(KEY_ENTER)) {
            if (selectedSlot.workerId != static_cast<EntityID>(-1)) {
                const EntityID worker = selectedSlot.workerId;

                if (worker < em.active.size() && em.active[worker] && em.hasTransform[worker]) {
                    camera.SetTarget(em.transforms[worker].position);
                }

                return;
            }

            const EntityID worker = FindFirstEligibleWorker(em, selectedSlot.profession, professionReg, villageId);

            if (worker == static_cast<EntityID>(-1)) {
                return;
            }

            AssignWorkerToProfessionSlot(em, selectedSlot, worker, behaviorReg);
        }
    }
}

void VillageMenu::Render(const EntityManager& em, const ResourceRegistry& resourceReg, const TimeSystem& timeSystem) const {
    if (!m_isOpen) {
        return;
    }

    DrawPanelBackground();

    const int panelX = (GetScreenWidth() - PANEL_WIDTH) / 2;
    const int panelY = (GetScreenHeight() - PANEL_HEIGHT) / 2;

    const float x = static_cast<float>(panelX + PANEL_PADDING);
    const float y = static_cast<float>(panelY + PANEL_PADDING);

    DrawText("VILLAGE MANAGEMENT", static_cast<int>(x), static_cast<int>(y), 28, GOLD);
    DrawText("V: close | Left/Right: tabs | Up/Down: select | Enter: focus villager", static_cast<int>(x), static_cast<int>(y + 34), 16,
             LIGHTGRAY);

    RenderTabs(x, y + 70.0f);

    const EntityID villageId = FindPrimaryVillage(em);

    if (villageId == static_cast<EntityID>(-1)) {
        DrawText("No active village found.", static_cast<int>(x), static_cast<int>(y + 130.0f), 20, RED);
        return;
    }

    const float contentX = x;
    const float contentY = y + 125.0f;

    switch (m_currentTab) {
        case VillageMenuTab::Overview:
            RenderOverview(em, resourceReg, timeSystem, villageId, contentX, contentY);
            break;
        case VillageMenuTab::Villagers:
            RenderVillagers(em, villageId, contentX, contentY);
            break;
        case VillageMenuTab::Professions:
            RenderProfessions(em, contentX, contentY);
            break;
        case VillageMenuTab::Storage:
            RenderStorage(em, resourceReg, villageId, contentX, contentY);
            break;
        default:
            break;
    }
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
    for (int i = 0; i < TAB_COUNT; ++i) {
        const VillageMenuTab tab = static_cast<VillageMenuTab>(i);
        const bool selected = tab == m_currentTab;

        const float tabX = x + static_cast<float>(i) * 160.0f;
        const Color color = selected ? YELLOW : GRAY;

        DrawText(TabName(tab), static_cast<int>(tabX), static_cast<int>(y), 22, color);

        if (selected) {
            DrawLine(static_cast<int>(tabX), static_cast<int>(y + 26.0f), static_cast<int>(tabX + 120.0f), static_cast<int>(y + 26.0f),
                     color);
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

    const int usedBeds = CountUsedBedsForVillage(em, villageId);
    const int totalBeds = CountTotalBedsForVillage(em, villageId);

    DrawKeyValue("Beds", std::to_string(usedBeds) + " / " + std::to_string(totalBeds), x, currentY, totalBeds > 0 ? RAYWHITE : ORANGE);
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
    constexpr float COL_HUNGER = 390.0f;
    constexpr float COL_FATIGUE = 530.0f;
    constexpr float COL_TASK = 680.0f;

    constexpr float BAR_WIDTH = 110.0f;

    DrawText("Name", static_cast<int>(x + COL_NAME), static_cast<int>(currentY), 16, GRAY);
    DrawText("Age", static_cast<int>(x + COL_AGE), static_cast<int>(currentY), 16, GRAY);
    DrawText("Profession", static_cast<int>(x + COL_PROFESSION), static_cast<int>(currentY), 16, GRAY);
    DrawText("Hunger", static_cast<int>(x + COL_HUNGER), static_cast<int>(currentY), 16, GRAY);
    DrawText("Fatigue", static_cast<int>(x + COL_FATIGUE), static_cast<int>(currentY), 16, GRAY);
    DrawText("Task", static_cast<int>(x + COL_TASK), static_cast<int>(currentY), 16, GRAY);

    currentY += 26.0f;

    if (members.empty()) {
        DrawText("No villagers found.", static_cast<int>(x), static_cast<int>(currentY), 18, LIGHTGRAY);
        return;
    }

    const int visibleMax = 16;
    const int selected = std::max(0, std::min(m_selectedVillagerIndex, static_cast<int>(members.size()) - 1));
    const int start = std::max(0, selected - visibleMax + 1);
    const int end = std::min(static_cast<int>(members.size()), start + visibleMax);

    for (int row = start; row < end; ++row) {
        const EntityID entity = members[row];
        const bool isSelected = row == selected;

        const Color rowColor = isSelected ? YELLOW : RAYWHITE;

        if (isSelected) {
            DrawRectangle(static_cast<int>(x - 8.0f), static_cast<int>(currentY - 3.0f), PANEL_WIDTH - PANEL_PADDING * 2, 24,
                          ColorAlpha(DARKGRAY, 0.65f));
        }

        DrawText(TruncateText(GetDisplayName(em, entity), 18).c_str(), static_cast<int>(x + COL_NAME), static_cast<int>(currentY), 16,
                 rowColor);

        if (em.hasTag[entity]) {
            DrawText(std::to_string(em.tags[entity].age).c_str(), static_cast<int>(x + COL_AGE), static_cast<int>(currentY), 16, rowColor);
        }

        if (em.hasProfession[entity]) {
            DrawText(TruncateText(em.professions[entity].currentProfession, 14).c_str(), static_cast<int>(x + COL_PROFESSION),
                     static_cast<int>(currentY), 16, rowColor);
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

void VillageMenu::RenderProfessions(const EntityManager& em, float x, float y) const {
    DrawText("Professions", static_cast<int>(x), static_cast<int>(y), 24, SKYBLUE);

    const std::vector<ProfessionSlotView> slots = GetProfessionSlots(em);

    float currentY = y + 42.0f;

    DrawText("Up/Down: select slot | Enter: assign/focus | Backspace/R: release", static_cast<int>(x), static_cast<int>(currentY), 16,
             LIGHTGRAY);

    currentY += 34.0f;

    if (slots.empty()) {
        DrawText("No profession slots available.", static_cast<int>(x), static_cast<int>(currentY), 18, LIGHTGRAY);
        return;
    }

    DrawText("Profession", static_cast<int>(x), static_cast<int>(currentY), 16, GRAY);
    DrawText("Worker", static_cast<int>(x + 220.0f), static_cast<int>(currentY), 16, GRAY);
    DrawText("Workplace", static_cast<int>(x + 460.0f), static_cast<int>(currentY), 16, GRAY);
    DrawText("Status", static_cast<int>(x + 620.0f), static_cast<int>(currentY), 16, GRAY);

    currentY += 26.0f;

    const int visibleMax = 14;
    int selected = std::max(0, std::min(m_selectedProfessionSlotIndex, static_cast<int>(slots.size()) - 1));

    const int start = std::max(0, selected - visibleMax + 1);
    const int end = std::min(static_cast<int>(slots.size()), start + visibleMax);

    for (int row = start; row < end; ++row) {
        const ProfessionSlotView& slot = slots[row];
        const bool isSelected = row == selected;

        const Color rowColor = isSelected ? YELLOW : RAYWHITE;

        if (isSelected) {
            DrawRectangle(static_cast<int>(x - 8.0f), static_cast<int>(currentY - 3.0f), PANEL_WIDTH - PANEL_PADDING * 2, 24,
                          ColorAlpha(DARKGRAY, 0.65f));
        }

        DrawText(slot.profession.c_str(), static_cast<int>(x), static_cast<int>(currentY), 16, rowColor);

        std::string workerName = "empty";

        if (slot.workerId != static_cast<EntityID>(-1) && slot.workerId < em.active.size() && em.active[slot.workerId]) {
            workerName = GetDisplayName(em, slot.workerId);
        }

        DrawText(workerName.c_str(), static_cast<int>(x + 220.0f), static_cast<int>(currentY), 16,
                 slot.workerId == static_cast<EntityID>(-1) ? ORANGE : rowColor);

        DrawText(("#" + std::to_string(slot.workplaceId)).c_str(), static_cast<int>(x + 460.0f), static_cast<int>(currentY), 16, LIGHTGRAY);

        const bool occupied = slot.workerId != static_cast<EntityID>(-1);

        DrawText(occupied ? "occupied" : "available", static_cast<int>(x + 620.0f), static_cast<int>(currentY), 16,
                 occupied ? GREEN : ORANGE);

        currentY += 26.0f;
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

        if (currentY > static_cast<float>((GetScreenHeight() + PANEL_HEIGHT) / 2 - 40)) {
            DrawText("Storage list truncated.", static_cast<int>(x), static_cast<int>(currentY), 16, ORANGE);
            break;
        }
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
