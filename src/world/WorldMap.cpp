#include "world/WorldMap.hpp"

void WorldMap::Initialize(int width, int height) {
    m_width = width;
    m_height = height;
    // Fill the map with 0 (default tile, e.g., Water)
    m_tiles.assign(m_width * m_height, 0);
}

int WorldMap::GetTile(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return -1;
    return m_tiles[y * m_width + x];
}

void WorldMap::SetTile(int x, int y, int tileId) {
    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_tiles[y * m_width + x] = tileId;
    }
}
