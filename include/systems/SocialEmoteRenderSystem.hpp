/**
 * @file SocialEmoteRenderSystem.hpp
 * @brief Renders small visual emotes above entities during social interactions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

class SocialEmoteRenderSystem {
public:
    void Render(const EntityManager& em) const;
};
