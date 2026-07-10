#include "systems/RenderSystem.hpp"

#include <cmath>
#include <raymath.h>

void RenderSystem::Render(const EntityManager& em, const Camera2D& camera, bool showNames) const {
    double time = GetTime();

    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    float buffer = 100.0f;
    topLeft.x -= buffer;
    topLeft.y -= buffer;
    bottomRight.x += buffer;
    bottomRight.y += buffer;

    // ==========================================
    // PASS 1 : DESSINER LA GÉOMÉTRIE
    // ==========================================
    for (EntityID i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasTransform[i] || !em.hasSprite[i])
            continue;

        Vector2 drawPos = em.transforms[i].position;
        if (drawPos.x < topLeft.x || drawPos.x > bottomRight.x || drawPos.y < topLeft.y || drawPos.y > bottomRight.y)
            continue;

        const auto& sprite = em.sprites[i];

        if (sprite.isAnimated) {
            float bounceOffset = std::sin(time * 5.0f + i) * 3.0f;
            drawPos.y += bounceOffset;
        }

        if (sprite.texturePath == "square") {
            Rectangle rec = {drawPos.x - sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f, sprite.width, sprite.height};
            DrawRectangleRec(rec, sprite.tint);
            DrawRectangleLinesEx(rec, 1.0f, BLACK);
        } else if (sprite.texturePath == "circle") {
            DrawCircleV(drawPos, sprite.width / 2.0f, sprite.tint);
            DrawCircleLines(drawPos.x, drawPos.y, sprite.width / 2.0f, BLACK);
        } else if (sprite.texturePath == "triangle") {
            Vector2 v1 = {drawPos.x, drawPos.y - sprite.height / 2.0f};
            Vector2 v2 = {drawPos.x - sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            Vector2 v3 = {drawPos.x + sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            DrawTriangle(v1, v2, v3, sprite.tint);
            DrawTriangleLines(v1, v2, v3, BLACK);
        } else {
            DrawRectangle(drawPos.x, drawPos.y, sprite.width, sprite.height, MAGENTA);
        }

        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            DrawPoly(drawPos, 6, sprite.width / 1.5f, time * 50.0f, ColorAlpha(YELLOW, 0.6f));
        }
        if (em.hasDeconstruct[i]) {
            Vector2 p1 = {drawPos.x - sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f};
            Vector2 p2 = {drawPos.x + sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            Vector2 p3 = {drawPos.x + sprite.width / 2.0f, drawPos.y - sprite.height / 2.0f};
            Vector2 p4 = {drawPos.x - sprite.width / 2.0f, drawPos.y + sprite.height / 2.0f};
            DrawLineEx(p1, p2, 4.0f, RED);
            DrawLineEx(p3, p4, 4.0f, RED);
        }
    }

    // ==========================================
    // PASS 2 : DESSINER LES NOMS (Toujours au-dessus !)
    // ==========================================
    if (showNames) {
        for (EntityID i = 0; i < em.active.size(); ++i) {
            // FILTRE : Seules les entités avec des STATS (PNJ/Monstres) affichent leur nom !
            if (!em.active[i] || !em.hasTransform[i] || !em.hasTag[i] || !em.hasStats[i])
                continue;

            Vector2 drawPos = em.transforms[i].position;
            if (drawPos.x < topLeft.x || drawPos.x > bottomRight.x || drawPos.y < topLeft.y || drawPos.y > bottomRight.y)
                continue;

            const char* name = em.tags[i].name.c_str();
            int fontSize = 20; // Plus grand !
            int textWidth = MeasureText(name, fontSize);
            int padding = 4;

            float textX = drawPos.x - textWidth / 2.0f;
            float textY = drawPos.y - 30.0f; // Remonté au-dessus de la tête

            // Fond noir transparent pour une lisibilité parfaite
            DrawRectangle(textX - padding, textY - padding, textWidth + padding * 2, fontSize + padding * 2, ColorAlpha(BLACK, 0.7f));
            DrawText(name, textX, textY, fontSize, RAYWHITE);
        }
    }
}
