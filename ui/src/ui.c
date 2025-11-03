/**
 * @file ui.c
 * @brief Renders and updates the in-game UI (inventory, pause menu, settings).
 */

#include "ui.h"
#include "ui_theme.h"
#include "tile.h"
#include "object.h"
#include "music.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SLOT_SIZE 40.0f
#define SLOT_MARGIN 8.0f
#define MAX_SLOTS_PER_ROW 10
#define INVENTORY_TABS 3
#define SETTINGS_SECTION_COUNT 2

enum
{
    TAB_TILES = 0,
    TAB_OBJECTS,
    TAB_ENTITIES
};

enum
{
    SETTINGS_SECTION_AUDIO = 0,
    SETTINGS_SECTION_KEYS  = 1
};

static const char* TAB_NAMES[INVENTORY_TABS]              = {"Tuiles", "Objets", "Entités"};
static const char* SETTINGS_NAMES[SETTINGS_SECTION_COUNT] = {"Audio", "Commandes"};
static const char* PAUSE_BUTTONS[]                        = {"Continuer", "Réglages", "Quitter"};

typedef struct UiState
{
    bool        inventoryOpen;
    int         inventoryTab;
    bool        pauseOpen;
    bool        settingsOpen;
    bool        requestExit;
    int         settingsSection;
    bool        capturingBinding;
    InputAction bindingAction;
    bool        volumeDragging;
    int         selectedGroupIndex;
    float       masterVolume;
} UiState;

static UiState g_ui = {0};

static inline const UiTheme* theme(void)
{
    return ui_theme_get();
}

static const char* display_group_name(const char* name)
{
    return (name && name[0] != '\0') ? name : "Tous";
}

static const char* key_to_text(KeyboardKey key)
{
    switch (key)
    {
        case KEY_NULL:
            return "Aucun";
        case KEY_SPACE:
            return "Espace";
        case KEY_ENTER:
            return "Entrée";
        case KEY_TAB:
            return "Tab";
        case KEY_BACKSPACE:
            return "Retour";
        case KEY_ESCAPE:
            return "Echap";
        case KEY_LEFT:
            return "←";
        case KEY_RIGHT:
            return "→";
        case KEY_UP:
            return "↑";
        case KEY_DOWN:
            return "↓";
        case KEY_LEFT_SHIFT:
        case KEY_RIGHT_SHIFT:
            return "Shift";
        case KEY_LEFT_CONTROL:
        case KEY_RIGHT_CONTROL:
            return "Ctrl";
        case KEY_LEFT_ALT:
        case KEY_RIGHT_ALT:
            return "Alt";
        default:
            break;
    }

    if (key >= KEY_A && key <= KEY_Z)
    {
        static char buf[2];
        buf[0] = (char)('A' + (key - KEY_A));
        buf[1] = '\0';
        return buf;
    }
    if (key >= KEY_ZERO && key <= KEY_NINE)
    {
        static char buf[2];
        buf[0] = (char)('0' + (key - KEY_ZERO));
        buf[1] = '\0';
        return buf;
    }

    return TextFormat("#%d", key);
}

static bool is_modal_open(void)
{
    return g_ui.inventoryOpen || g_ui.pauseOpen || g_ui.settingsOpen || g_ui.capturingBinding;
}

static Rectangle pause_panel_rect(void)
{
    const int   screenW = GetScreenWidth();
    const int   screenH = GetScreenHeight();
    const float width   = fminf(420.0f, screenW - 140.0f);
    const float height  = 280.0f;
    Rectangle   rect    = {(screenW - width) * 0.5f, (screenH - height) * 0.5f, width, height};
    return rect;
}

static Rectangle settings_panel_rect(void)
{
    const int   screenW = GetScreenWidth();
    const int   screenH = GetScreenHeight();
    const float width   = fminf(560.0f, screenW - 160.0f);
    const float height  = fminf(500.0f, screenH - 160.0f);
    Rectangle   rect    = {(screenW - width) * 0.5f, (screenH - height) * 0.5f, width, height};
    return rect;
}

static void draw_text_centered(const char* text, Rectangle area, int fontSize, Color color)
{
    int   textWidth = MeasureText(text, fontSize);
    float x         = area.x + (area.width - (float)textWidth) * 0.5f;
    float y         = area.y + (area.height - (float)fontSize) * 0.5f;
    DrawText(text, (int)x, (int)y, fontSize, color);
}

static bool draw_button(Rectangle bounds, const char* label, bool enabled)
{
    const UiTheme* ui = theme();
    if (!ui || !ui_theme_is_ready())
        return false;

    Vector2           mouse   = GetMousePosition();
    bool              hovered = CheckCollisionPointRec(mouse, bounds);
    bool              pressed = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool              clicked = hovered && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    Color             tint    = WHITE;
    const NPatchInfo* source  = &ui->buttonNormal;
    if (!enabled)
    {
        tint    = ColorAlpha(ui->textSecondary, 0.5f);
        hovered = false;
        pressed = false;
        clicked = false;
    }
    else if (pressed)
        source = &ui->buttonPressed;
    else if (hovered)
        source = &ui->buttonHover;

    DrawTextureNPatch(ui->atlas, *source, bounds, (Vector2){0.0f, 0.0f}, 0.0f, tint);

    int   fontSize  = 22;
    int   textWidth = MeasureText(label, fontSize);
    float textX     = bounds.x + (bounds.width - textWidth) * 0.5f;
    float textY     = bounds.y + (bounds.height - fontSize) * 0.5f;
    Color textColor = enabled ? ui->textPrimary : ColorAlpha(ui->textSecondary, 0.7f);
    DrawText(label, (int)textX, (int)textY, fontSize, textColor);
    return enabled && clicked;
}

static bool draw_tab(Rectangle bounds, const char* label, bool active)
{
    const UiTheme* ui = theme();
    if (!ui || !ui_theme_is_ready())
        return false;

    const NPatchInfo* patch = active ? &ui->tabActive : &ui->tabInactive;
    Color             tint  = active ? WHITE : ColorAlpha(WHITE, 0.85f);
    DrawTextureNPatch(ui->atlas, *patch, bounds, (Vector2){0.0f, 0.0f}, 0.0f, tint);

    int   fontSize  = 20;
    int   textWidth = MeasureText(label, fontSize);
    float textX     = bounds.x + (bounds.width - textWidth) * 0.5f;
    float textY     = bounds.y + (bounds.height - fontSize) * 0.5f;
    Color textColor = active ? theme()->accentBright : ColorAlpha(theme()->textPrimary, 0.8f);
    DrawText(label, (int)textX, (int)textY, fontSize, textColor);

    Vector2 mouse = GetMousePosition();
    return CheckCollisionPointRec(mouse, bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

static float slot_texture_scale(float srcWidth, float srcHeight)
{
    float w          = (srcWidth <= 0.0f) ? 1.0f : srcWidth;
    float h          = (srcHeight <= 0.0f) ? 1.0f : srcHeight;
    float availableW = SLOT_SIZE - 12.0f;
    float availableH = SLOT_SIZE - 12.0f;
    float scale      = fminf(availableW / w, availableH / h);
    if (!isfinite(scale) || scale <= 0.0f)
        scale = 1.0f;
    return scale;
}

static int inventory_slot_count(const EntitySystem* entities)
{
    switch (g_ui.inventoryTab)
    {
        case TAB_TILES:
            return TILE_MAX;
        case TAB_OBJECTS:
            return OBJ_COUNT;
        case TAB_ENTITIES:
            return entities ? entity_system_type_count(entities) : 0;
        default:
            return 0;
    }
}

static void draw_inventory(InputState* input, const EntitySystem* entities)
{
    const UiTheme* ui = theme();
    if (!ui)
        return;

    const int screenW = GetScreenWidth();
    const int screenH = GetScreenHeight();

    const int   totalSlots   = inventory_slot_count(entities);
    const int   rows         = (totalSlots > 0) ? (totalSlots + MAX_SLOTS_PER_ROW - 1) / MAX_SLOTS_PER_ROW : 1;
    const float headerHeight = 88.0f;

    float panelW = SLOT_MARGIN + (SLOT_SIZE + SLOT_MARGIN) * MAX_SLOTS_PER_ROW;
    float panelH = headerHeight + (SLOT_SIZE + SLOT_MARGIN) * rows + SLOT_MARGIN * 2.0f;

    Rectangle panel = {(screenW - panelW) * 0.5f, (screenH - panelH) * 0.5f, panelW, panelH};
    DrawTextureNPatch(ui->atlas, ui->panelLarge, panel, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    // Tabs
    float tabWidth  = (panel.width - SLOT_MARGIN * 2.0f - 16.0f) / INVENTORY_TABS;
    float tabHeight = 34.0f;
    float tabY      = panel.y + 20.0f;
    float tabX      = panel.x + SLOT_MARGIN + 8.0f;
    for (int i = 0; i < INVENTORY_TABS; ++i)
    {
        Rectangle tabRect = {tabX + i * (tabWidth + 8.0f), tabY, tabWidth, tabHeight};
        if (draw_tab(tabRect, TAB_NAMES[i], g_ui.inventoryTab == i))
        {
            g_ui.inventoryTab = i;
        }
    }

    Rectangle titleArea = {panel.x, tabY + tabHeight + 4.0f, panel.width, 28.0f};
    draw_text_centered("Inventaire", titleArea, 24, ui->textPrimary);

    if (totalSlots <= 0)
    {
        Rectangle message = {panel.x, panel.y + headerHeight + 20.0f, panel.width, 40.0f};
        draw_text_centered("Aucune entrée disponible", message, 20, ui->textSecondary);
        return;
    }

    Vector2 mouse   = GetMousePosition();
    float   gridTop = panel.y + headerHeight;

    for (int index = 0; index < totalSlots; ++index)
    {
        int row = index / MAX_SLOTS_PER_ROW;
        int col = index % MAX_SLOTS_PER_ROW;

        float     posX = panel.x + SLOT_MARGIN + col * (SLOT_SIZE + SLOT_MARGIN);
        float     posY = gridTop + row * (SLOT_SIZE + SLOT_MARGIN);
        Rectangle slot = {posX, posY, SLOT_SIZE, SLOT_SIZE};

        DrawRectangleRounded(slot, 0.2f, 4, ColorAlpha(ui->textSecondary, 0.15f));

        Rectangle frameDest = {slot.x + 4.0f, slot.y + 4.0f, slot.width - 8.0f, slot.height - 8.0f};
        DrawTexturePro(ui->atlas, ui->slotFrame, frameDest, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

        bool hovered  = CheckCollisionPointRec(mouse, slot);
        bool selected = false;

        Texture2D         texture    = {0};
        bool              hasTexture = false;
        Rectangle         src        = {0};
        Rectangle         dst        = {0};
        const EntityType* entityType = NULL;

        if (g_ui.inventoryTab == TAB_TILES)
        {
            const TileType* tile = get_tile_type((TileTypeID)index);
            if (tile && tile->texture.id != 0)
            {
                texture     = tile->texture;
                src         = (Rectangle){0.0f, 0.0f, (float)texture.width, (float)texture.height};
                float scale = slot_texture_scale(src.width, src.height);
                dst         = (Rectangle){slot.x + (slot.width - src.width * scale) * 0.5f, slot.y + (slot.height - src.height * scale) * 0.5f, src.width * scale, src.height * scale};
                hasTexture  = true;
            }
            selected = (input->selectedTile == index);
        }
        else if (g_ui.inventoryTab == TAB_OBJECTS)
        {
            const ObjectType* obj = get_object_type((ObjectTypeID)index);
            if (obj && obj->texture.id != 0)
            {
                texture     = obj->texture;
                int frameW  = obj->spriteFrameWidth > 0 ? obj->spriteFrameWidth : texture.width;
                int frameH  = obj->spriteFrameHeight > 0 ? obj->spriteFrameHeight : texture.height;
                src         = (Rectangle){0.0f, 0.0f, (float)frameW, (float)frameH};
                float scale = slot_texture_scale(src.width, src.height);
                dst         = (Rectangle){slot.x + (slot.width - src.width * scale) * 0.5f, slot.y + (slot.height - src.height * scale) * 0.5f, src.width * scale, src.height * scale};
                hasTexture  = true;
            }
            selected = (input->selectedObject == index);
        }
        else if (g_ui.inventoryTab == TAB_ENTITIES)
        {
            entityType = entities ? entity_system_type_at(entities, index) : NULL;
            if (entityType)
            {
                const EntitySprite* sprite = &entityType->sprite;
                if (sprite->texture.id != 0)
                {
                    texture     = sprite->texture;
                    int frameW  = sprite->frameWidth > 0 ? sprite->frameWidth : texture.width;
                    int frameH  = sprite->frameHeight > 0 ? sprite->frameHeight : texture.height;
                    src         = (Rectangle){0.0f, 0.0f, (float)frameW, (float)frameH};
                    float scale = slot_texture_scale(src.width, src.height);
                    dst         = (Rectangle){slot.x + (slot.width - src.width * scale) * 0.5f, slot.y + (slot.height - src.height * scale) * 0.5f, src.width * scale, src.height * scale};
                    hasTexture  = true;
                }
                else
                {
                    DrawCircle((int)(slot.x + slot.width * 0.5f), (int)(slot.y + slot.height * 0.5f), slot.width * 0.35f, entityType->tint);
                }
                selected = (input->selectedEntity == entityType->id);
            }
        }

        if (hasTexture)
        {
            DrawTexturePro(texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        }

        if (selected)
        {
            Rectangle highlight = {slot.x - 2.0f, slot.y - 2.0f, slot.width + 4.0f, slot.height + 4.0f};
            DrawTexturePro(ui->atlas, ui->tileHighlight, highlight, (Vector2){0.0f, 0.0f}, 0.0f, ColorAlpha(WHITE, 0.85f));
        }
        else if (hovered)
        {
            DrawRectangleLinesEx(slot, 2.0f, ColorAlpha(ui->accentBright, 0.6f));
        }

        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (g_ui.inventoryTab == TAB_TILES)
            {
                input->selectedTile   = (TileTypeID)index;
                input->selectedObject = OBJ_NONE;
                input->selectedEntity = ENTITY_TYPE_INVALID;
                input->currentMode    = MODE_TILE;
            }
            else if (g_ui.inventoryTab == TAB_OBJECTS)
            {
                input->selectedObject = (ObjectTypeID)index;
                input->selectedTile   = TILE_GRASS;
                input->selectedEntity = ENTITY_TYPE_INVALID;
                input->currentMode    = MODE_OBJECT;
            }
            else if (g_ui.inventoryTab == TAB_ENTITIES && entityType)
            {
                input->selectedEntity = entityType->id;
                input->selectedTile   = TILE_MAX;
                input->selectedObject = OBJ_NONE;
                input->currentMode    = MODE_ENTITY;
            }
        }
    }
}

static void draw_audio_settings(Rectangle content)
{
    const UiTheme* ui      = theme();
    const float    padding = 6.0f;

    int groupCount = music_system_get_group_count();
    if (groupCount <= 0)
        groupCount = 1;
    if (g_ui.selectedGroupIndex >= groupCount)
        g_ui.selectedGroupIndex = groupCount - 1;
    if (g_ui.selectedGroupIndex < 0)
        g_ui.selectedGroupIndex = 0;

    const float lineHeight  = 56.0f;
    Rectangle   volumeLabel = {content.x, content.y, content.width, 24.0f};
    DrawText("Volume maître", (int)volumeLabel.x, (int)volumeLabel.y, 20, ui->textPrimary);

    Rectangle slider = {content.x, content.y + 28.0f, content.width - 2 * padding, 10.0f};
    slider.x += padding;
    Rectangle sliderHitbox = {slider.x - 4.0f, slider.y - 10.0f, slider.width + 8.0f, slider.height + 24.0f};

    DrawRectangleRounded(slider, 0.5f, 6, ColorAlpha(ui->textSecondary, 0.25f));
    Rectangle fill = slider;
    fill.width     = fmaxf(0.0f, fminf(slider.width, slider.width * g_ui.masterVolume));
    DrawRectangleRounded(fill, 0.5f, 6, ColorAlpha(ui->accent, 0.7f));

    float     knobX = slider.x + slider.width * g_ui.masterVolume;
    Rectangle knob  = {knobX - 6.0f, slider.y - 6.0f, 12.0f, slider.height + 12.0f};
    DrawRectangleRounded(knob, 0.5f, 6, ColorAlpha(ui->accentBright, 0.9f));

    Vector2 mouse       = GetMousePosition();
    bool    sliderHover = CheckCollisionPointRec(mouse, sliderHitbox);
    if (!g_ui.volumeDragging && sliderHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        g_ui.volumeDragging = true;
    if (g_ui.volumeDragging)
    {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON))
        {
            float t           = (mouse.x - slider.x) / slider.width;
            g_ui.masterVolume = fminf(1.0f, fmaxf(0.0f, t));
            music_system_set_master_volume(g_ui.masterVolume);
        }
        else
        {
            g_ui.volumeDragging = false;
        }
    }

    char volumeValue[32];
    snprintf(volumeValue, sizeof(volumeValue), "%d %%", (int)roundf(g_ui.masterVolume * 100.0f));
    DrawText(volumeValue, (int)(sliderHitbox.x + sliderHitbox.width + 8.0f), (int)(sliderHitbox.y + 6.0f), 20, ui->textPrimary);

    float     controlsTop = content.y + lineHeight + 32.0f;
    Rectangle labelRect   = {content.x, controlsTop, content.width, 24.0f};
    DrawText("Boucle de jeu", (int)labelRect.x, (int)labelRect.y, 20, ui->textPrimary);

    Rectangle groupArea  = {content.x, controlsTop + 28.0f, content.width, 48.0f};
    Rectangle prevBtn    = {groupArea.x, groupArea.y, 60.0f, groupArea.height};
    Rectangle nextBtn    = {groupArea.x + groupArea.width - 60.0f, groupArea.y, 60.0f, groupArea.height};
    Rectangle groupLabel = {prevBtn.x + prevBtn.width + 12.0f, groupArea.y, groupArea.width - (prevBtn.width + nextBtn.width + 24.0f), groupArea.height};

    if (draw_button(prevBtn, "<", groupCount > 0))
    {
        int previous            = g_ui.selectedGroupIndex;
        g_ui.selectedGroupIndex = (g_ui.selectedGroupIndex + groupCount - 1) % groupCount;
        if (!music_system_set_gameplay_group_index(g_ui.selectedGroupIndex, true))
            g_ui.selectedGroupIndex = previous;
    }
    if (draw_button(nextBtn, ">", groupCount > 0))
    {
        int previous            = g_ui.selectedGroupIndex;
        g_ui.selectedGroupIndex = (g_ui.selectedGroupIndex + 1) % groupCount;
        if (!music_system_set_gameplay_group_index(g_ui.selectedGroupIndex, true))
            g_ui.selectedGroupIndex = previous;
    }

    const char* groupName = music_system_get_group_name(g_ui.selectedGroupIndex);
    Rectangle   labelBg   = groupLabel;
    DrawRectangleRounded(labelBg, 0.2f, 4, ColorAlpha(ui->textSecondary, 0.1f));
    draw_text_centered(display_group_name(groupName), labelBg, 22, ui->textPrimary);

    Rectangle nextTrackBtn = {content.x, groupArea.y + groupArea.height + 16.0f, content.width * 0.45f, 44.0f};
    if (draw_button(nextTrackBtn, "Piste suivante", true))
        music_system_force_next(MUSIC_TRANSITION_CROSSFADE, 1.0f);

    const char* currentTrack = music_system_get_current_track_name();
    if (!currentTrack)
        currentTrack = "Aucune piste active";
    DrawText(TextFormat("Actuellement: %s", currentTrack), (int)(content.x), (int)(nextTrackBtn.y + nextTrackBtn.height + 12.0f), 18, ui->textSecondary);
}

static void draw_key_settings(Rectangle content, InputState* input)
{
    const UiTheme* ui        = theme();
    const float    rowHeight = 54.0f;

    for (int action = 0; action < INPUT_ACTION_COUNT; ++action)
    {
        float     rowTop = content.y + action * (rowHeight + 6.0f);
        Rectangle row    = {content.x, rowTop, content.width, rowHeight};
        DrawRectangleRounded(row, 0.2f, 4, ColorAlpha(ui->textSecondary, 0.08f));

        const char* actionName = input_action_display_name((InputAction)action);
        DrawText(actionName, (int)(row.x + 12.0f), (int)(row.y + 14.0f), 20, ui->textPrimary);

        Rectangle   buttonRect    = {row.x + row.width - 160.0f, row.y + 6.0f, 150.0f, row.height - 12.0f};
        KeyboardKey boundKey      = input_get_binding(&input->bindings, (InputAction)action);
        bool        capturingThis = g_ui.capturingBinding && g_ui.bindingAction == action;
        const char* label         = capturingThis ? "..." : key_to_text(boundKey);
        bool        clicked       = draw_button(buttonRect, label, !g_ui.capturingBinding || capturingThis);

        if (clicked)
        {
            g_ui.capturingBinding = true;
            g_ui.bindingAction    = (InputAction)action;
        }
    }

    Rectangle resetBtn = {content.x, content.y + INPUT_ACTION_COUNT * (rowHeight + 6.0f) + 16.0f, 260.0f, 46.0f};
    if (draw_button(resetBtn, "Remettre par défaut", !g_ui.capturingBinding))
    {
        input_bindings_reset_default(&input->bindings);
    }
}

static void draw_settings(InputState* input)
{
    const UiTheme* ui    = theme();
    Rectangle      panel = settings_panel_rect();
    DrawTextureNPatch(ui->atlas, ui->panelLarge, panel, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    Rectangle title = {panel.x, panel.y + 16.0f, panel.width, 32.0f};
    draw_text_centered("Réglages", title, 26, ui->textPrimary);

    float tabWidth  = (panel.width - 64.0f) / SETTINGS_SECTION_COUNT;
    float tabHeight = 36.0f;
    float tabX      = panel.x + 32.0f;
    float tabY      = panel.y + 62.0f;
    for (int section = 0; section < SETTINGS_SECTION_COUNT; ++section)
    {
        Rectangle tabRect = {tabX + section * (tabWidth + 12.0f), tabY, tabWidth, tabHeight};
        if (draw_tab(tabRect, SETTINGS_NAMES[section], g_ui.settingsSection == section))
            g_ui.settingsSection = section;
    }

    Rectangle content = {panel.x + 32.0f, tabY + tabHeight + 24.0f, panel.width - 64.0f, panel.height - (tabY - panel.y) - tabHeight - 96.0f};

    if (g_ui.settingsSection == SETTINGS_SECTION_AUDIO)
        draw_audio_settings(content);
    else
        draw_key_settings(content, input);

    Rectangle backBtn = {panel.x + panel.width - 180.0f, panel.y + panel.height - 64.0f, 160.0f, 46.0f};
    if (draw_button(backBtn, "Retour", !g_ui.capturingBinding))
    {
        g_ui.settingsOpen   = false;
        g_ui.volumeDragging = false;
    }
}

static void draw_pause_menu(InputState* input)
{
    (void)input;

    if (g_ui.settingsOpen)
    {
        draw_settings(input);
        return;
    }

    const UiTheme* ui    = theme();
    Rectangle      panel = pause_panel_rect();
    DrawTextureNPatch(ui->atlas, ui->panelMedium, panel, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);

    Rectangle title = {panel.x, panel.y + 20.0f, panel.width, 40.0f};
    draw_text_centered("Pause", title, 30, ui->textPrimary);

    float buttonWidth  = panel.width - 80.0f;
    float buttonHeight = 52.0f;
    float startY       = panel.y + 80.0f;

    for (int i = 0; i < (int)(sizeof(PAUSE_BUTTONS) / sizeof(PAUSE_BUTTONS[0])); ++i)
    {
        Rectangle btn = {panel.x + (panel.width - buttonWidth) * 0.5f, startY + i * (buttonHeight + 16.0f), buttonWidth, buttonHeight};
        if (draw_button(btn, PAUSE_BUTTONS[i], true))
        {
            switch (i)
            {
                case 0: // Continuer
                    g_ui.pauseOpen      = false;
                    g_ui.settingsOpen   = false;
                    g_ui.volumeDragging = false;
                    break;
                case 1: // Réglages
                    g_ui.settingsOpen       = true;
                    g_ui.settingsSection    = SETTINGS_SECTION_AUDIO;
                    g_ui.masterVolume       = music_system_get_master_volume();
                    g_ui.selectedGroupIndex = music_system_get_selected_group_index();
                    break;
                case 2: // Quitter
                    g_ui.requestExit = true;
                    break;
            }
        }
    }
}

static void draw_building_hint(const InputState* input)
{
    const UiTheme* ui = theme();
    if (!ui)
        return;

    Rectangle badge = {20.0f, 138.0f, 260.0f, 40.0f};
    DrawTextureNPatch(ui->atlas, ui->panelSmall, badge, (Vector2){0.0f, 0.0f}, 0.0f, ColorAlpha(WHITE, 0.9f));

    KeyboardKey toggleKey = input_get_binding(&input->bindings, INPUT_ACTION_TOGGLE_BUILDING_NAMES);
    const char* keyName   = key_to_text(toggleKey);

    char text[128];
    snprintf(text, sizeof(text), "Noms (%s): %s", keyName, input->showBuildingNames ? "activés" : "désactivés");

    Color color = input->showBuildingNames ? ui->accentBright : ui->textSecondary;
    DrawText(text, (int)(badge.x + 12.0f), (int)(badge.y + 12.0f), 20, color);
}

static void draw_capture_prompt(void)
{
    if (!g_ui.capturingBinding)
        return;

    const UiTheme* ui  = theme();
    Rectangle      bar = {0.0f, (float)GetScreenHeight() - 70.0f, (float)GetScreenWidth(), 50.0f};
    DrawRectangleRec(bar, ColorAlpha(BLACK, 0.65f));

    char buffer[160];
    snprintf(buffer, sizeof(buffer), "Appuyez sur une touche pour \"%s\" (clic droit pour annuler)", input_action_display_name(g_ui.bindingAction));

    draw_text_centered(buffer, bar, 22, ui->textPrimary);
}

bool ui_init(const char* atlasPath)
{
    if (!ui_theme_init(atlasPath))
        return false;

    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.inventoryTab       = TAB_TILES;
    g_ui.masterVolume       = music_system_get_master_volume();
    g_ui.selectedGroupIndex = music_system_get_selected_group_index();
    return true;
}

void ui_shutdown(void)
{
    ui_theme_shutdown();
    memset(&g_ui, 0, sizeof(g_ui));
}

void ui_update(InputState* input, const EntitySystem* entities, float deltaTime)
{
    (void)entities;
    (void)deltaTime;

    if (!input || !ui_theme_is_ready())
        return;

    if (g_ui.capturingBinding)
    {
        int pressed = GetKeyPressed();
        while (pressed != 0)
        {
            KeyboardKey key = (KeyboardKey)pressed;
            if (key == KEY_BACKSPACE || key == KEY_DELETE)
            {
                input_set_binding(&input->bindings, g_ui.bindingAction, KEY_NULL);
                g_ui.capturingBinding = false;
                break;
            }
            if (key != KEY_NULL)
            {
                InputAction conflict;
                if (input_is_key_already_bound(&input->bindings, key, &conflict) && conflict != g_ui.bindingAction)
                    input_set_binding(&input->bindings, conflict, KEY_NULL);

                input_set_binding(&input->bindings, g_ui.bindingAction, key);
                g_ui.capturingBinding = false;
                break;
            }
            pressed = GetKeyPressed();
        }

        if (g_ui.capturingBinding && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
            g_ui.capturingBinding = false;

        if (g_ui.capturingBinding)
            return;
    }

    KeyboardKey pauseKey = input_get_binding(&input->bindings, INPUT_ACTION_TOGGLE_PAUSE);
    if (pauseKey != KEY_NULL && IsKeyPressed(pauseKey))
    {
        if (g_ui.settingsOpen)
        {
            g_ui.settingsOpen   = false;
            g_ui.volumeDragging = false;
        }
        else
        {
            g_ui.pauseOpen      = !g_ui.pauseOpen;
            g_ui.inventoryOpen  = false;
            g_ui.volumeDragging = false;
        }
    }

    KeyboardKey inventoryKey = input_get_binding(&input->bindings, INPUT_ACTION_TOGGLE_INVENTORY);
    if (!g_ui.pauseOpen && inventoryKey != KEY_NULL && IsKeyPressed(inventoryKey))
        g_ui.inventoryOpen = !g_ui.inventoryOpen;

    KeyboardKey namesKey = input_get_binding(&input->bindings, INPUT_ACTION_TOGGLE_BUILDING_NAMES);
    if (namesKey != KEY_NULL && IsKeyPressed(namesKey))
        input->showBuildingNames = !input->showBuildingNames;

    if (!g_ui.pauseOpen)
    {
        g_ui.settingsOpen   = false;
        g_ui.volumeDragging = false;
    }

    if (!g_ui.volumeDragging)
        g_ui.masterVolume = music_system_get_master_volume();

    if (!g_ui.pauseOpen)
        g_ui.selectedGroupIndex = music_system_get_selected_group_index();

    if (g_ui.inventoryOpen)
    {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_Q))
            g_ui.inventoryTab = (g_ui.inventoryTab + INVENTORY_TABS - 1) % INVENTORY_TABS;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_E))
            g_ui.inventoryTab = (g_ui.inventoryTab + 1) % INVENTORY_TABS;
    }
}

void ui_draw(InputState* input, const EntitySystem* entities)
{
    if (!input || !ui_theme_is_ready())
        return;

    if (g_ui.inventoryOpen || g_ui.pauseOpen)
    {
        const UiTheme* ui = theme();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ui->overlayDim);
    }

    if (g_ui.inventoryOpen)
        draw_inventory(input, entities);

    if (g_ui.pauseOpen)
        draw_pause_menu(input);

    draw_building_hint(input);
    draw_capture_prompt();
}

bool ui_is_inventory_open(void)
{
    return g_ui.inventoryOpen;
}

bool ui_is_input_blocked(void)
{
    return is_modal_open();
}

bool ui_is_paused(void)
{
    return g_ui.pauseOpen;
}

bool ui_should_close_application(void)
{
    return g_ui.requestExit;
}
