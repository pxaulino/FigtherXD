#if __has_include("if/hud.hpp")
#include "if/hud.hpp"
#else
#include "hud.hpp"
#endif

#include <algorithm>
#include <cmath>

#include "ui/ui_components.hpp"
#include "ui/ui_theme.hpp"

namespace
{
const char* OpponentModeDisplay(OpponentMode mode)
{
    if (UITheme::s_language == UITheme::Language::Spanish)
    {
        switch (mode)
        {
        case OpponentMode::DummyQuiet: return "INMOVIL";
        case OpponentMode::CpuBasic: return "CPU BASICA";
        case OpponentMode::DummyDiShield: return "DI + ESCUDO";
        }
    }
    else
    {
        switch (mode)
        {
        case OpponentMode::DummyQuiet: return "DUMMY (STAND)";
        case OpponentMode::CpuBasic: return "BASIC CPU";
        case OpponentMode::DummyDiShield: return "DUMMY (DI + SHIELD)";
        }
    }
    return "UNKNOWN";
}

void DrawPlayerHudPanel(const SpriteRenderer& sprites, const Fighter& fighter, float centerX, float flash)
{
    const float shake = fighter.damageShake > 0.0F ? std::sin(static_cast<float>(GetTime()) * 95.0F) * 3.0F : 0.0F;
    const float panelW = 268.0F;
    const float panelH = 58.0F;
    const float panelY = 6.0F;
    const Rectangle panelRec = {
        std::floor(centerX - panelW * 0.5F + shake),
        panelY,
        panelW,
        panelH
    };

    const Color accentColor = (fighter.playerIndex == 0) ? UITheme::COLOR_ACCENT_CYAN : UITheme::COLOR_ACCENT_MAIN;
    const char* const playerName = Describe(fighter.character).name;

    // 1. Marco biselado estilizado con fondo oscuro
    const Color bgFill = UITheme::PANEL;
    UIComponents::DrawCutCornerRect(panelRec, 6.0F, bgFill, UITheme::EDGE, 1.0F);

    DrawRectangleRec({panelRec.x + 8, panelY, panelW - 16, 3}, accentColor);

    sprites.Draw(int(fighter.character), SpriteAnimation::Idle, 0, true, fighter.playerIndex ? -1 : 1,
                 {panelRec.x + 6, panelY + 5, 39, 42}, panelY + 48);
    // 2. Nombre del jugador (PackMAN / Shadow) en su color
    UIComponents::DrawTextModern(playerName, panelRec.x + 49.0F, panelRec.y + 5.0F,
                                 UITheme::FONT_SIZE_BODY, accentColor, false, false);

    // 3. Circulos de stocks (vidas restantes)
    UIComponents::DrawTextModern(fighter.playerIndex ? "CPU" : "P1", panelRec.x + 49, panelY + 29,
                                 16, accentColor, false, false);
    const float stockStartX = panelRec.x + 85;
    const float stockY = panelRec.y + 36.0F;
    for (int stock = 0; stock < fighter.stocks; ++stock)
    {
        const float sx = stockStartX + static_cast<float>(stock) * 12.0F;
        DrawCircle(static_cast<int>(sx), static_cast<int>(stockY), 4.0F, accentColor);
        DrawCircleLines(static_cast<int>(sx), static_cast<int>(stockY), 4.0F, UITheme::COLOR_TEXT_MAIN);
    }

    if (flash > 0)
    {
        const float t = std::clamp(flash / 0.35f, 0.0f, 1.0f);
        DrawCircleLines(int(stockStartX + fighter.stocks * 12), int(stockY), 4 + (1-t)*5, ColorAlpha(accentColor,t));
    }

    // 4. Porcentaje de dano en Amarillo Oro (#FFC400) a 24 px alineado a la derecha
    const char* const percentStr = TextFormat("%.1f%%", fighter.percent);
    const float width = UIComponents::MeasureTextModern(percentStr, UITheme::DAMAGE_SIZE, true).x;
    const float size = UITheme::DAMAGE_SIZE * std::min(1.0f, 126.0f / width);
    const Vector2 percentSize = UIComponents::MeasureTextModern(percentStr, size, true);
    constexpr Color stops[]{UITheme::COLOR_TEXT_MAIN, UITheme::COLOR_HIGHLIGHT,
                            {255,147,55,255}, {255,79,99,255}};
    const float risk = std::clamp(fighter.percent / 50.0f, 0.0f, 3.0f);
    const int segment = std::min(2, int(risk));
    const Color damage = ColorLerp(stops[segment], stops[segment+1], risk-segment);
    UIComponents::DrawTextModern(percentStr, panelRec.x + panelW - 14.0F - percentSize.x, panelRec.y + 12.0F,
                                 size, damage, false, true);

    // 5. Barra compacta de escudo en la base del panel
    const float shieldW = panelW - 24.0F;
    const float shieldH = 4.0F;
    const Rectangle shieldBgRec = { panelRec.x + UITheme::INSET, panelRec.y + panelH - 8.0F, shieldW, shieldH };
    DrawRectangleRec(shieldBgRec, Color{ 22, 28, 42, 255 });

    const float shieldRatio = std::clamp(fighter.shieldHealth / 100.0F, 0.0F, 1.0F);
    const Rectangle shieldFillRec = { shieldBgRec.x, shieldBgRec.y, shieldW * shieldRatio, shieldH };
    const Color shieldColor = fighter.shielding ? UITheme::COLOR_ACCENT_CYAN : ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.60F);
    DrawRectangleRec(shieldFillRec, shieldColor);
}
}

void DrawHud(const SpriteRenderer& sprites, const std::array<float,2>& stockFlash, const std::array<Fighter, 2>& fighters, OpponentMode mode, bool debugMode, float fps)
{
    DrawRectangle(0, 0, 960, HudBandHeight, UITheme::COLOR_BG_BASE);
    DrawLine(0,HudBandHeight-1,960,HudBandHeight-1,UITheme::EDGE);
    DrawPlayerHudPanel(sprites, fighters[0], 188, stockFlash[0]);
    DrawPlayerHudPanel(sprites, fighters[1], 772, stockFlash[1]);

    // Overlay de debug: EXCLUSIVAMENTE visible al presionar F1, en una esquina y semitransparente
    if (debugMode)
    {
        const Rectangle debugBoxRec = { 325.0F, 2.0F, 310.0F, 66.0F };
        DrawRectangleRec(debugBoxRec, ColorAlpha(UITheme::COLOR_BG_BASE, 0.85F));
        UIComponents::DrawCutCornerRect(debugBoxRec, 4.0F, ColorAlpha(UITheme::COLOR_BG_BASE, 0.0F),
                                        ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.40F), 1.0F);

        const char* const modeLabel = (UITheme::s_language == UITheme::Language::Spanish) ? "MODO" : "MODE";
        UIComponents::DrawTextModern(TextFormat("FPS: %.0f | %s: %s", fps, modeLabel, OpponentModeDisplay(mode)),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 4.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_HIGHLIGHT, false, false);

        UIComponents::DrawTextModern(TextFormat("P1: %s [%d] | KB %.1f", ActionStateName(fighters[0].state),
                                                fighters[0].stateFrame, fighters[0].launchKnockback),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 24.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, false, false);

        UIComponents::DrawTextModern(TextFormat("P2: %s [%d] | KB %.1f", ActionStateName(fighters[1].state),
                                                fighters[1].stateFrame, fighters[1].launchKnockback),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 44.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, false, false);
    }
}
