/**
 * @file TextureCache.hpp
 * @brief Shared texture cache for all rendering systems.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

/**
 * @class TextureCache
 * @brief Loads, stores and unloads textures shared by all render systems.
 *
 * The cache owns all loaded Texture2D instances.
 * Rendering systems should request textures from this cache instead of loading
 * them directly.
 */
class TextureCache {
public:
    TextureCache() = default;
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    const Texture2D* GetTexture(const std::string& texturePath);

    void Clear();

private:
    std::unordered_map<std::string, Texture2D> m_textures;
    std::unordered_set<std::string> m_failedTextures;
};
