/**
 * @file ChronicleRenderSystem.cpp
 * @brief Implementation of the chronicle UI renderer.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ChronicleRenderSystem.hpp"

#include <algorithm>
#include <raylib.h>

void ChronicleRenderSystem::Render(const Chronicle& chronicle, const SettlementMetrics& metrics) const {
    const int startX = 10;
    int y = 70;

    DrawText(TextFormat("Faith %.0f | Fear %.0f | Corruption %.0f | Reputation %.0f", metrics.faith, metrics.fear, metrics.corruption,
                        metrics.reputation),
             startX, y, 18, LIGHTGRAY);

    y += 26;

    const auto& entries = chronicle.GetEntries();

    if (entries.empty()) {
        return;
    }

    DrawText("Chronicle", startX, y, 18, GOLD);
    y += 24;

    const int maxEntries = 4;
    const int firstIndex = std::max(0, static_cast<int>(entries.size()) - maxEntries);

    for (int i = firstIndex; i < static_cast<int>(entries.size()); ++i) {
        const ChronicleEntry& entry = entries[i];

        const std::string line = "D" + std::to_string(entry.day) + " [" + entry.phase + "] " + entry.message;

        DrawText(line.c_str(), startX, y, 16, RAYWHITE);

        y += 20;
    }
}
