/**
 * @file EventSystemFormat.cpp
 * @brief Message formatting helpers for EventSystem.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/EventSystem.hpp"

namespace {

void ReplaceAll(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }

    size_t pos = 0;

    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.length(), to);
        pos += to.length();
    }
}

} // namespace

std::string EventSystem::FormatEventMessage(const std::string& message, int count, const std::string& itemId, int amount) const {
    std::string result = message;

    ReplaceAll(result, "{count}", std::to_string(count));
    ReplaceAll(result, "{item}", itemId);
    ReplaceAll(result, "{amount}", std::to_string(amount));

    return result;
}
