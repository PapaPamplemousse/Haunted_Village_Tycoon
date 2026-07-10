#pragma once
#include <vector>

/**
 * @class WorldMap
 * @brief Represents the 2D grid of the game world.
 * Stores tile IDs in a contiguous 1D array for CPU cache performance.
 */
class WorldMap {
public:
    WorldMap() = default;
    ~WorldMap() = default;

    /**
     * @brief Initializes the map with the given dimensions.
     * @param width The width of the map in tiles.
     * @param height The height of the map in tiles.
     */
    void Initialize(int width, int height);

    /**
     * @brief Gets the tile ID at the specified coordinates.
     * @return int The ID of the tile, or -1 if out of bounds.
     */
    int GetTile(int x, int y) const;

    /**
     * @brief Sets the tile ID at the specified coordinates.
     */
    void SetTile(int x, int y, int tileId);

    int GetWidth() const {
        return m_width;
    }
    int GetHeight() const {
        return m_height;
    }

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<int> m_tiles;
};
