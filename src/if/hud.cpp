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

void DrawPlayerHudPanel(const Fighter& fighter, float centerX, int playerIndex)
{
    const float shake = fighter.damageShake > 0.0F ? std::sin(static_cast<float>(GetTime()) * 95.0F) * 3.0F : 0.0F;
    const float panelW = 210.0F;
    const float panelH = 58.0F;
    const float panelY = 466.0F;
    const Rectangle panelRec = {
        std::floor(centerX - panelW * 0.5F + shake),
        panelY,
        panelW,
        panelH
    };

    const Color accentColor = (playerIndex == 0) ? UITheme::COLOR_ACCENT_CYAN : UITheme::COLOR_ACCENT_MAIN;
    const char* const playerName = (playerIndex == 0) ? UITheme::P1_NAME : UITheme::P2_NAME;

    // 1. Marco biselado estilizado con fondo oscuro
    const Color bgFill = ColorAlpha(UITheme::COLOR_BG_BASE, 0.90F);
    UIComponents::DrawCutCornerRect(panelRec, 6.0F, bgFill, accentColor, 2.0F);

    // 2. Nombre del jugador (PackMAN / Shadow) en su color
    UIComponents::DrawTextModern(playerName, panelRec.x + 14.0F, panelRec.y + 8.0F,
                                 UITheme::FONT_SIZE_BODY, accentColor, false, false);

    // 3. Circulos de stocks (vidas restantes)
    const float stockStartX = panelRec.x + 16.0F;
    const float stockY = panelRec.y + 36.0F;
    for (int stock = 0; stock < fighter.stocks; ++stock)
    {
        const float sx = stockStartX + static_cast<float>(stock) * 16.0F;
        DrawCircle(static_cast<int>(sx), static_cast<int>(stockY), 5.0F, accentColor);
        DrawCircleLines(static_cast<int>(sx), static_cast<int>(stockY), 5.0F, UITheme::COLOR_TEXT_MAIN);
    }

    // 4. Porcentaje de dano en Amarillo Oro (#FFC400) a 24 px alineado a la derecha
    const char* const percentStr = TextFormat("%05.1f%%", fighter.percent);
    const Vector2 percentSize = UIComponents::MeasureTextModern(percentStr, UITheme::FONT_SIZE_BUTTON);
    UIComponents::DrawTextModern(percentStr, panelRec.x + panelW - 14.0F - percentSize.x, panelRec.y + 12.0F,
                                 UITheme::FONT_SIZE_BUTTON, UITheme::COLOR_HIGHLIGHT, false, false);

    // 5. Barra compacta de escudo en la base del panel
    const float shieldW = panelW - 24.0F;
    const float shieldH = 4.0F;
    const Rectangle shieldBgRec = { panelRec.x + 12.0F, panelRec.y + panelH - 8.0F, shieldW, shieldH };
    DrawRectangleRec(shieldBgRec, Color{ 22, 28, 42, 255 });

    const float shieldRatio = std::clamp(fighter.shieldHealth / 100.0F, 0.0F, 1.0F);
    const Rectangle shieldFillRec = { shieldBgRec.x, shieldBgRec.y, shieldW * shieldRatio, shieldH };
    const Color shieldColor = fighter.shielding ? UITheme::COLOR_ACCENT_CYAN : ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.60F);
    DrawRectangleRec(shieldFillRec, shieldColor);
}
}

void DrawHud(const std::array<Fighter, 2>& fighters, OpponentMode mode, bool debugMode, float fps)
{
    // Paneles compactos inferiores (PackMAN a x=320, Shadow a x=640)
    DrawPlayerHudPanel(fighters[0], 320.0F, 0);
    DrawPlayerHudPanel(fighters[1], 640.0F, 1);

    // Overlay de debug: EXCLUSIVAMENTE visible al presionar F1, en una esquina y semitransparente
    if (debugMode)
    {
        const Rectangle debugBoxRec = { 14.0F, 14.0F, 280.0F, 78.0F };
        DrawRectangleRec(debugBoxRec, ColorAlpha(UITheme::COLOR_BG_BASE, 0.85F));
        UIComponents::DrawCutCornerRect(debugBoxRec, 4.0F, ColorAlpha(UITheme::COLOR_BG_BASE, 0.0F),
                                        ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.40F), 1.0F);

        const char* const modeLabel = (UITheme::s_language == UITheme::Language::Spanish) ? "MODO" : "MODE";
        UIComponents::DrawTextModern(TextFormat("FPS: %.0f | %s: %s", fps, modeLabel, OpponentModeDisplay(mode)),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 8.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_HIGHLIGHT, false, false);

        UIComponents::DrawTextModern(TextFormat("P1: %s [%d] | KB %.1f", ActionStateName(fighters[0].state),
                                                fighters[0].stateFrame, fighters[0].launchKnockback),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 28.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, false, false);

        UIComponents::DrawTextModern(TextFormat("P2: %s [%d] | KB %.1f", ActionStateName(fighters[1].state),
                                                fighters[1].stateFrame, fighters[1].launchKnockback),
                                     debugBoxRec.x + 10.0F, debugBoxRec.y + 48.0F,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, false, false);
    }
}
