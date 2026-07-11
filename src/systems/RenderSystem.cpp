#include "systems/RenderSystem.hpp"

#include "core/Config.hpp"

#include <cmath>
#include <raymath.h>
#include <string>
#include <vector>

void RenderSystem::Render(const EntityManager& em, const EntitySpatialGrid& spatialGrid, const Camera2D& camera, bool showNames) const {
    double time = GetTime();

    Vector2 topLeft = GetScreenToWorld2D({0, 0}, camera);
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, camera);

    const float margin = static_cast<float>(Config::RENDER_ENTITY_MARGIN_TILES) * Config::TILE_SIZE;

    topLeft.x -= margin;
    topLeft.y -= margin;
    bottomRight.x += margin;
    bottomRight.y += margin;

    Rectangle visibleRect = {topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};

    const std::vector<EntityID> visibleEntities = spatialGrid.GetEntitiesInRect(visibleRect, em);

    // ==========================================
    // PASS 1 : DESSINER LA GEOMETRIE
    // ==========================================
    for (EntityID i : visibleEntities) {
        if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i] || !em.hasSprite[i]) {
            continue;
        }

        Vector2 drawPos = em.transforms[i].position;
        const auto& sprite = em.sprites[i];

        if (sprite.isAnimated) {
            float bounceOffset = std::sin(time * 5.0f + static_cast<float>(i)) * 3.0f;
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
    // PASS 2 : DESSINER LES NOMS
    // ==========================================
    if (showNames) {
        for (EntityID i : visibleEntities) {
            if (i >= em.active.size() || !em.active[i] || !em.hasTransform[i] || !em.hasTag[i] || !em.hasStats[i]) {
                continue;
            }

            Vector2 drawPos = em.transforms[i].position;
            const auto& tag = em.tags[i];

            std::string displayName = !tag.firstName.empty() ? tag.firstName : tag.name;

            if (displayName.empty()) {
                continue;
            }

            int fontSize = 20;
            int textWidth = MeasureText(displayName.c_str(), fontSize);
            int padding = 4;

            float textX = drawPos.x - textWidth / 2.0f;
            float textY = drawPos.y - 30.0f;

            DrawRectangle(textX - padding, textY - padding, textWidth + padding * 2, fontSize + padding * 2, ColorAlpha(BLACK, 0.7f));

            DrawText(displayName.c_str(), textX, textY, fontSize, RAYWHITE);
        }
    }
}
