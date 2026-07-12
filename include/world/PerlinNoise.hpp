/**
 * @file PerlinNoise.hpp
 * @brief Standard C++ implementation of 2D Perlin Noise for map generation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

/**
 * @class PerlinNoise
 * @brief A standard C++ implementation of 2D Perlin Noise for procedural generation.
 */
class PerlinNoise {
public:
    /**
     * @brief Initializes the permutation vector with a specific seed.
     */
    PerlinNoise(unsigned int seed = 12345) {
        p.resize(256);
        std::iota(p.begin(), p.end(), 0);
        std::default_random_engine engine(seed);
        std::shuffle(p.begin(), p.end(), engine);
        p.insert(p.end(), p.begin(), p.end()); // Duplicate for overflow prevention
    }

    /**
     * @brief Generates a continuous noise value between -1.0 and 1.0.
     */
    float GetNoise(float x, float y) const {
        int X = (int)std::floor(x) & 255;
        int Y = (int)std::floor(y) & 255;
        x -= std::floor(x);
        y -= std::floor(y);
        float u = Fade(x);
        float v = Fade(y);
        int A = p[X] + Y, AA = p[A], AB = p[A + 1];
        int B = p[X + 1] + Y, BA = p[B], BB = p[B + 1];

        return Lerp(v, Lerp(u, Grad(p[AA], x, y), Grad(p[BA], x - 1, y)), Lerp(u, Grad(p[AB], x, y - 1), Grad(p[BB], x - 1, y - 1)));
    }

private:
    std::vector<int> p;
    float Fade(float t) const {
        return t * t * t * (t * (t * 6 - 15) + 10);
    }
    float Lerp(float t, float a, float b) const {
        return a + t * (b - a);
    }
    float Grad(int hash, float x, float y) const {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : h == 12 || h == 14 ? x : 0.0f;
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }
};
