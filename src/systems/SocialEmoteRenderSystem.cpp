/**
 * @file SocialEmoteRenderSystem.cpp
 * @brief Implementation of social emote rendering.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/SocialEmoteRenderSystem.hpp"

#include <raylib.h>
#include <string>

namespace {

struct EmoteView {
    std::string text;
    Color color = WHITE;
};

bool GetEmoteForTask(const std::string& task, EmoteView& outEmote) {
    if (task == "socializing") {
        outEmote = {"...", SKYBLUE};
        return true;
    }

    if (task == "moving_to_socialize") {
        outEmote = {"...", LIGHTGRAY};
        return true;
    }

    if (task == "requesting_weapon") {
        outEmote = {"REQ", GOLD};
        return true;
    }

    if (task == "moving_to_request_weapon") {
        outEmote = {"?", GOLD};
        return true;
    }

    if (task == "waiting_for_weapon") {
        outEmote = {"?", ORANGE};
        return true;
    }

    if (task == "crafting_weapon") {
        outEmote = {"MAKE", ORANGE};
        return true;
    }

    if (task == "depositing_crafted_weapon") {
        outEmote = {"OK", GREEN};
        return true;
    }

    if (task == "confronting_person") {
        outEmote = {"!", ORANGE};
        return true;
    }

    if (task == "moving_to_confront") {
        outEmote = {"!", ORANGE};
        return true;
    }

    if (task == "intimidating_person") {
        outEmote = {"!!", RED};
        return true;
    }

    if (task == "moving_to_intimidate") {
        outEmote = {"!!", RED};
        return true;
    }

    if (task == "fighting_non_lethal") {
        outEmote = {"FIGHT", RED};
        return true;
    }

    if (task == "moving_to_fight_non_lethal") {
        outEmote = {"FIGHT", RED};
        return true;
    }

    if (task == "murdering_person") {
        outEmote = {"X", MAROON};
        return true;
    }

    if (task == "moving_to_murder") {
        outEmote = {"X", MAROON};
        return true;
    }

    if (task == "feeding_child") {
        outEmote = {"FOOD", GREEN};
        return true;
    }

    if (task == "moving_to_feed_child") {
        outEmote = {"FOOD", GREEN};
        return true;
    }
    if (task == "praying" || task == "moving_to_pray") {
        outEmote = {"PRAY", GOLD};
        return true;
    }

    if (task == "preaching" || task == "moving_to_preach") {
        outEmote = {"PREACH", GOLD};
        return true;
    }

    if (task == "holding_ritual" || task == "moving_to_ritual") {
        outEmote = {"RITUAL", PURPLE};
        return true;
    }

    if (task == "comforting_frightened" || task == "moving_to_comfort") {
        outEmote = {"CALM", SKYBLUE};
        return true;
    }

    return false;
}

void DrawEmoteBubble(Vector2 worldPos, const EmoteView& emote) {
    constexpr float bubbleW = 42.0f;
    constexpr float bubbleH = 22.0f;
    constexpr float offsetY = 34.0f;

    const float bubbleX = worldPos.x - bubbleW * 0.5f;
    const float bubbleY = worldPos.y - offsetY - bubbleH;

    Rectangle bubbleRect = {bubbleX, bubbleY, bubbleW, bubbleH};

    DrawRectangleRounded(bubbleRect, 0.35f, 8, ColorAlpha(BLACK, 0.78f));
    DrawRectangleRoundedLines(bubbleRect, 0.35f, 8, 1.0f, emote.color);

    const int fontSize = 12;
    const int textWidth = MeasureText(emote.text.c_str(), fontSize);

    DrawText(emote.text.c_str(), static_cast<int>(bubbleX + bubbleW * 0.5f - static_cast<float>(textWidth) * 0.5f),
             static_cast<int>(bubbleY + 5.0f), fontSize, emote.color);
}

} // namespace

void SocialEmoteRenderSystem::Render(const EntityManager& em) const {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasTransform[entity] || !em.hasBehavior[entity]) {
            continue;
        }

        const BehaviorComponent& behavior = em.behaviors[entity];

        EmoteView emote;

        if (!GetEmoteForTask(behavior.currentTask, emote)) {
            continue;
        }

        DrawEmoteBubble(em.transforms[entity].position, emote);
    }
}
