/**
 * @file TextureCache.cpp
 * @brief Implementation of shared texture cache.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "graphics/TextureCache.hpp"

#include <iostream>

TextureCache::~TextureCache() {
    Clear();
}

const Texture2D* TextureCache::GetTexture(const std::string& texturePath) {
    if (texturePath.empty() || texturePath == "square" || texturePath == "circle" || texturePath == "triangle") {
        return nullptr;
    }

    auto existing = m_textures.find(texturePath);

    if (existing != m_textures.end()) {
        return &existing->second;
    }

    if (m_failedTextures.find(texturePath) != m_failedTextures.end()) {
        return nullptr;
    }

    Texture2D texture = LoadTexture(texturePath.c_str());

    if (texture.id == 0) {
        std::cerr << "[WARNING] Failed to load texture: " << texturePath << std::endl;
        m_failedTextures.insert(texturePath);
        return nullptr;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    auto inserted = m_textures.emplace(texturePath, texture);
    return &inserted.first->second;
}

void TextureCache::Clear() {
    for (auto& pair : m_textures) {
        if (pair.second.id != 0) {
            UnloadTexture(pair.second);
        }
    }

    m_textures.clear();
    m_failedTextures.clear();
}
