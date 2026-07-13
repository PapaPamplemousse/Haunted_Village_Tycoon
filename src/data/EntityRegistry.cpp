/**
 * @file EntityRegistry.cpp
 * @brief Implementation of the EntityRegistry.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/EntityRegistry.hpp"

#include "data/BehaviorRegistry.hpp"
#include "data/STVParser.hpp"
#include "ecs/EntityManager.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string Trim(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");

    if (first == std::string::npos) {
        return "";
    }

    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

Color ParseColor(const std::string& value) {
    Color c = {255, 255, 255, 255};

    std::stringstream ss(value);
    std::string token;
    int i = 0;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (i == 0) {
            c.r = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 1) {
            c.g = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 2) {
            c.b = static_cast<unsigned char>(std::stoi(token));
        } else if (i == 3) {
            c.a = static_cast<unsigned char>(std::stoi(token));
        }

        i++;
    }

    return c;
}

std::vector<std::string> ParseStringList(const std::string& value) {
    std::vector<std::string> result;

    std::stringstream ss(value);
    std::string token;

    while (std::getline(ss, token, ',')) {
        token = Trim(token);

        if (!token.empty()) {
            result.push_back(token);
        }
    }
    return result;
}

std::string PickRandomString(const std::vector<std::string>& values) {
    if (values.empty()) {
        return "";
    }

    const int index = GetRandomValue(0, static_cast<int>(values.size()) - 1);
    return values[static_cast<size_t>(index)];
}

std::string SelectSpriteTexturePath(const EntityDef& def, const std::string& gender) {
    if (gender == "male") {
        const std::string selected = PickRandomString(def.spriteVariantsMale);

        if (!selected.empty()) {
            return selected;
        }
    }

    if (gender == "female") {
        const std::string selected = PickRandomString(def.spriteVariantsFemale);

        if (!selected.empty()) {
            return selected;
        }
    }

    const std::string undefinedSelected = PickRandomString(def.spriteVariantsUndefined);

    if (!undefinedSelected.empty()) {
        return undefinedSelected;
    }

    return def.texturePath;
}

/**
 * @brief Splits a behavior list while ignoring commas inside parentheses.
 *
 * Example:
 *   "wander, hunt(human,rabbit), gather"
 * becomes:
 *   ["wander", "hunt(human,rabbit)", "gather"]
 */
std::vector<std::string> SplitBehaviorList(const std::string& value) {
    std::vector<std::string> result;

    std::string current;
    int parenthesisDepth = 0;

    for (char c : value) {
        if (c == '(') {
            parenthesisDepth++;
            current.push_back(c);
            continue;
        }

        if (c == ')') {
            parenthesisDepth = std::max(0, parenthesisDepth - 1);
            current.push_back(c);
            continue;
        }

        if (c == ',' && parenthesisDepth == 0) {
            const std::string token = Trim(current);

            if (!token.empty()) {
                result.push_back(token);
            }

            current.clear();
            continue;
        }

        current.push_back(c);
    }

    const std::string token = Trim(current);

    if (!token.empty()) {
        result.push_back(token);
    }

    return result;
}

BehaviorRule ParseBehaviorRule(const std::string& rawBehavior) {
    BehaviorRule rule;

    const std::string behavior = Trim(rawBehavior);

    const size_t openParen = behavior.find('(');
    const size_t closeParen = behavior.find_last_of(')');

    if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen) {
        rule.name = behavior;
        return rule;
    }

    rule.name = Trim(behavior.substr(0, openParen));

    const std::string argumentList = behavior.substr(openParen + 1, closeParen - openParen - 1);

    std::stringstream ss(argumentList);
    std::string argument;

    while (std::getline(ss, argument, ',')) {
        argument = Trim(argument);

        if (!argument.empty()) {
            rule.arguments.push_back(argument);
        }
    }

    return rule;
}

std::vector<DropEntry> ParseDrops(const std::string& value) {
    std::vector<DropEntry> drops;

    std::stringstream ss(value);
    std::string dropToken;

    while (std::getline(ss, dropToken, ',')) {
        std::stringstream dropSS(dropToken);

        std::string itemId;
        std::string amount;
        std::string chance;

        if (std::getline(dropSS, itemId, ':') && std::getline(dropSS, amount, ':') && std::getline(dropSS, chance, ':')) {
            DropEntry drop;
            drop.itemId = Trim(itemId);
            drop.amount = std::stoi(Trim(amount));
            drop.chance = std::stof(Trim(chance));

            if (!drop.itemId.empty() && drop.amount > 0) {
                drops.push_back(drop);
            }
        }
    }

    return drops;
}

SpriteSheetMode ParseSpriteSheetMode(const std::string& value) {
    const std::string mode = Trim(value);

    if (mode == "directional_action") {
        return SpriteSheetMode::DirectionalAction;
    }

    if (mode == "furniture_state") {
        return SpriteSheetMode::FurnitureState;
    }

    if (mode == "seasonal") {
        return SpriteSheetMode::Seasonal;
    }

    return SpriteSheetMode::None;
}

} // namespace

bool EntityRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);

    if (blocks.empty()) {
        return false;
    }

    for (const auto& block : blocks) {
        EntityDef def;
        def.id = block.id;

        if (block.properties.count("name")) {
            def.name = block.properties.at("name");
        }

        if (block.properties.count("max_hp")) {
            def.maxHp = std::stof(block.properties.at("max_hp"));
        }

        if (block.properties.count("base_atk")) {
            def.baseAtk = std::stof(block.properties.at("base_atk"));
        }

        if (block.properties.count("max_speed")) {
            def.maxSpeed = std::stof(block.properties.at("max_speed"));
        }

        if (block.properties.count("action_radius")) {
            def.actionRadiusTiles = std::stof(block.properties.at("action_radius"));
        }

        if (block.properties.count("activity_period")) {
            def.activityPeriod = Trim(block.properties.at("activity_period"));
        }

        if (block.properties.count("work_start_hour")) {
            def.workStartHour = std::stof(Trim(block.properties.at("work_start_hour")));
        }

        if (block.properties.count("work_end_hour")) {
            def.workEndHour = std::stof(Trim(block.properties.at("work_end_hour")));
        }

        if (block.properties.count("store_threshold")) {
            def.storeThreshold = std::stoi(Trim(block.properties.at("store_threshold")));
        }

        if (block.properties.count("category")) {
            def.category = block.properties.at("category");
        }

        if (block.properties.count("species")) {
            def.species = block.properties.at("species");
        }

        if (block.properties.count("gender")) {
            def.genderModel = block.properties.at("gender");

            if (def.genderModel != "undefined" && def.genderModel != "binary") {
                std::cerr << "[WARNING] Invalid gender model for entity " << def.id << ": " << def.genderModel
                          << ". Falling back to undefined." << std::endl;

                def.genderModel = "undefined";
            }
        }

        if (block.properties.count("default_profession")) {
            def.defaultProfession = block.properties.at("default_profession");
        }

        if (block.properties.count("drops")) {
            def.drops = ParseDrops(block.properties.at("drops"));
        }

        if (block.properties.count("texture")) {
            def.texturePath = block.properties.at("texture");
        }

        if (block.properties.count("color")) {
            def.color = ParseColor(block.properties.at("color"));
        }

        if (block.properties.count("sprite_size")) {
            std::stringstream ss(block.properties.at("sprite_size"));
            std::string token;

            if (std::getline(ss, token, ',')) {
                def.spriteWidth = std::stof(Trim(token));
            }

            if (std::getline(ss, token, ',')) {
                def.spriteHeight = std::stof(Trim(token));
            }
        }

        if (block.properties.count("sprite_mode")) {
            def.spriteSheetMode = ParseSpriteSheetMode(block.properties.at("sprite_mode"));
            def.useSpriteSheet = def.spriteSheetMode != SpriteSheetMode::None;
        }

        if (block.properties.count("sprite_sheet_columns")) {
            def.spriteSheetColumns = std::stoi(Trim(block.properties.at("sprite_sheet_columns")));
        }

        if (block.properties.count("sprite_sheet_rows")) {
            def.spriteSheetRows = std::stoi(Trim(block.properties.at("sprite_sheet_rows")));
        }

        if (block.properties.count("sprite_column_gap")) {
            def.spriteColumnGap = std::stof(Trim(block.properties.at("sprite_column_gap")));
        }

        if (block.properties.count("sprite_row_gap")) {
            def.spriteRowGap = std::stof(Trim(block.properties.at("sprite_row_gap")));
        }

        if (block.properties.count("sprite_frame_size")) {
            std::stringstream ss(block.properties.at("sprite_frame_size"));
            std::string token;

            if (std::getline(ss, token, ',')) {
                def.spriteFrameWidth = std::stof(Trim(token));
            }

            if (std::getline(ss, token, ',')) {
                def.spriteFrameHeight = std::stof(Trim(token));
            }
        }

        if (block.properties.count("sprite_variants_male")) {
            def.spriteVariantsMale = ParseStringList(block.properties.at("sprite_variants_male"));
        }

        if (block.properties.count("sprite_variants_female")) {
            def.spriteVariantsFemale = ParseStringList(block.properties.at("sprite_variants_female"));
        }

        if (block.properties.count("sprite_variants_undefined")) {
            def.spriteVariantsUndefined = ParseStringList(block.properties.at("sprite_variants_undefined"));
        }

        m_templates[block.id] = def;
    }

    std::cout << "[INFO] Loaded " << m_templates.size() << " entity definitions." << std::endl;

    return true;
}

EntityID EntityRegistry::SpawnEntity(EntityManager& em, const std::string& prefabId, Vector2 position, const NameRegistry& nameReg,
                                     const BehaviorRegistry& behaviorReg) {
    auto it = m_templates.find(prefabId);

    if (it == m_templates.end()) {
        std::cerr << "[WARNING] Cannot spawn unknown entity prefab: " << prefabId << std::endl;
        return 0;
    }

    const EntityDef& def = it->second;
    EntityID id = em.CreateEntity();

    // 1. Identity and location
    em.hasTag[id] = true;
    std::string generatedGender = "undefined";
    if (def.genderModel == "binary") {
        generatedGender = (GetRandomValue(0, 1) == 0) ? "male" : "female";
    }
    em.tags[id] = {def.name, def.id, nameReg.GetRandomName(def.species), def.species, def.category, def.genderModel, generatedGender, 0};

    // Social relationships are enabled for human entities for now.
    // Other species can get their own social rules later.
    if (def.species == "human") {
        em.hasSocial[id] = true;
        em.socials[id] = {};
    }

    em.hasTransform[id] = true;
    em.transforms[id] = {position};

    // 2. Health and needs
    em.hasHealth[id] = true;
    em.healths[id] = {def.maxHp, def.maxHp};

    em.hasNeeds[id] = true;
    em.needs[id] = {100.0f, 100.0f};

    // 3. AI
    em.hasProfession[id] = true;
    em.professions[id] = {def.defaultProfession};

    std::vector<BehaviorRule> rules = behaviorReg.GetBehaviorsFor(def.category, def.species, def.defaultProfession);

    std::vector<std::string> capabilities;
    for (const auto& rule : rules) {
        capabilities.push_back(rule.name);
    }

    em.hasBehavior[id] = true;
    em.behaviors[id] = {};
    em.behaviors[id].innateCapabilities = capabilities;
    em.behaviors[id].innateBehaviorRules = rules;
    em.behaviors[id].activityPeriod = def.activityPeriod;
    em.behaviors[id].workStartHour = def.workStartHour;
    em.behaviors[id].workEndHour = def.workEndHour;
    em.behaviors[id].storeThreshold = def.storeThreshold;
    em.hasAIContext[id] = true;
    em.aiContexts[id] = {};

    // 4. Inventory
    em.hasInventory[id] = true;
    em.inventories[id] = {};

    if (!def.drops.empty()) {
        em.hasLoot[id] = true;
        em.loots[id] = {def.drops};
    }

    // 5. Visual representation
    em.hasSprite[id] = true;

    const std::string selectedTexturePath = SelectSpriteTexturePath(def, generatedGender);

    em.sprites[id] = {selectedTexturePath, def.color, def.spriteWidth, def.spriteHeight, true};

    em.sprites[id].useSpriteSheet = def.useSpriteSheet;
    em.sprites[id].sheetMode = def.spriteSheetMode;
    em.sprites[id].sheetColumns = std::max(1, def.spriteSheetColumns);
    em.sprites[id].sheetRows = std::max(1, def.spriteSheetRows);
    em.sprites[id].frameWidth = def.spriteFrameWidth;
    em.sprites[id].frameHeight = def.spriteFrameHeight;
    em.sprites[id].columnGap = def.spriteColumnGap;
    em.sprites[id].rowGap = def.spriteRowGap;
    em.sprites[id].facing = SpriteFacing::Down;
    em.sprites[id].pose = SpritePose::Normal;
    em.sprites[id].isInUse = false;

    // 6. Stats
    em.hasStats[id] = true;
    em.stats[id] = {def.maxSpeed, def.baseAtk, def.actionRadiusTiles};

    std::cout << "[ECS] Spawned Entity: " << def.name << " at Position (" << position.x << ", " << position.y << ")" << std::endl;

    return id;
}

const EntityDef* EntityRegistry::GetEntityDef(const std::string& prefabId) const {
    auto it = m_templates.find(prefabId);
    return it != m_templates.end() ? &(it->second) : nullptr;
}
