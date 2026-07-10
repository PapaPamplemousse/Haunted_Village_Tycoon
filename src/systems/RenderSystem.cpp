#include "systems/RenderSystem.hpp"

#include <cmath> // For std::sin
#include <raymath.h>

void RenderSystem::Render(const EntityManager& em, const Camera2D& camera) const {
    double time = GetTime();

    // 1. Calcul de la zone visible à l'écran (avec une marge de sécurité de 100 pixels)
    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    float buffer = 100.0f; // Pour ne pas faire clignoter les entités sur les bords
    topLeft.x -= buffer;
    topLeft.y -= buffer;
    bottomRight.x += buffer;
    bottomRight.y += buffer;

    for (EntityID i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasTransform[i] || !em.hasSprite[i])
            continue;

        Vector2 drawPos = em.transforms[i].position;

        // 2. CULLING : Si l'entité est hors de l'écran, on saute la boucle ! (Zéro calcul)
        if (drawPos.x < topLeft.x || drawPos.x > bottomRight.x || drawPos.y < topLeft.y || drawPos.y > bottomRight.y) {
            continue;
        }

        const auto& sprite = em.sprites[i];

        if (sprite.isAnimated) {
            float bounceOffset = std::sin(time * 5.0f + i) * 3.0f;
            drawPos.y += bounceOffset;
        }

        // 3. Draw Geometry based on Data-Driven 'texturePath'
        if (sprite.texturePath == "square") {
            // Centered square
            Rectangle rec = {drawPos.x - sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f, sprite.width, sprite.height};
            DrawRectangleRec(rec, sprite.tint);
            DrawRectangleLinesEx(rec, 1.0f, BLACK); // Outline for better visibility
        } else if (sprite.texturePath == "circle") {
            // Centered circle
            DrawCircleV(drawPos, sprite.width / 2.0f, sprite.tint);
            DrawCircleLines(drawPos.x, drawPos.y, sprite.width / 2.0f, BLACK);
        } else if (sprite.texturePath == "triangle") {
            // Centered upward-pointing triangle
            Vector2 v1 = {drawPos.x, drawPos.y - sprite.height / 2.0f};                       // Top
            Vector2 v2 = {drawPos.x - sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f}; // Bottom Left
            Vector2 v3 = {drawPos.x + sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f}; // Bottom Right
            DrawTriangle(v1, v2, v3, sprite.tint);
            DrawTriangleLines(v1, v2, v3, BLACK);
        } else {
            // TODO later: Use AssetManager to draw actual textures like 'assets/villager.png'
            DrawRectangle(drawPos.x, drawPos.y, sprite.width, sprite.height, MAGENTA);
        }

        // 4. Draw Blueprint Ghost effect
        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            // Draw a spinning translucent icon over blueprints
            DrawPoly(drawPos, 6, sprite.width / 1.5f, time * 50.0f, ColorAlpha(YELLOW, 0.6f));
        }
    }
}
