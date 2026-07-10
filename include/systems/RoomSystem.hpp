#pragma once
#include "data/StructureRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/WorldMap.hpp"

#include <vector>

class RoomSystem {
public:
    RoomSystem() = default;

    /**
     * @brief Call this whenever a wall or door is built or destroyed!
     */
    void MarkDirty() {
        m_isDirty = true;
    }

    void Update(EntityManager& em, const WorldMap& map, const StructureRegistry& structReg);

    // Retourne true si on a recalculé les pièces à cette frame
    bool DidRecalculate() const {
        return m_justRecalculated;
    }

private:
    bool m_isDirty = true; // True au démarrage pour scanner la map initiale
    bool m_justRecalculated = false;
};
