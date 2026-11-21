#pragma once

#include <raylib.h>
#include <algorithm>
#include "ui/ui_theme.hpp"

namespace UIComponents
{
    // Estado del menu con cursor deslizante unico y feedback de pulsacion (80 ms a 0.97x)
    struct SlidingMenuState
    {
        float currentY = -1.0f;      // Posicion Y actual del cursor deslizante
        int lastSelected = 0;        // Indice previo para detectar cambios
        int pressedIndex = -1;       // Indice del boton actualmente presionado
        float pressTimer = 0.0f;     // Temporizador de feedback (80 ms)
        Vector2 lastMousePos = { -999.0f, -999.0f }; // Rastreo de movimiento del raton
    };

    // Gestor de transicion con fundido suave (150-200 ms) entre pantallas
    struct TransitionManager
    {
        bool active = false;
        bool fadingOut = true;
        float alpha = 0.0f;
        int targetScene = -1;
        float flashTimer = 0.0f;
        float shakeTimer = 0.0f;

        void Start(int nextScene, bool withJuice = false)
        {
            if (active) return;
            active = true;
            fadingOut = true;
            alpha = 0.0f;
            targetScene = nextScene;
            if (withJuice)
            {
                flashTimer = 0.12f; // Breve flash blanco de 120 ms
                shakeTimer = 0.18f; // Screen shake de 180 ms
            }
        }

        template <typename TSceneEnum>
        void Update(float dt, TSceneEnum& currentScene)
        {
            if (flashTimer > 0.0f) flashTimer = std::max(0.0f, flashTimer - dt);
            if (shakeTimer > 0.0f) shakeTimer = std::max(0.0f, shakeTimer - dt);

            if (!active) return;
            const float speed = 5.5f; // ~180 ms para cada fase
            if (fadingOut)
            {
                alpha += dt * speed;
                if (alpha >= 1.0f)
                {
                    alpha = 1.0f;
                    fadingOut = false;
                    currentScene = static_cast<TSceneEnum>(targetScene);
                }
            }
            else
            {
                alpha -= dt * speed;
                if (alpha <= 0.0f)
                {
                    alpha = 0.0f;
                    active = false;
                }
            }
        }

        Vector2 GetShakeOffset() const
        {
            if (shakeTimer <= 0.0f) return { 0.0f, 0.0f };
            const float progress = shakeTimer / 0.18f;
            const float mag = 4.0f * progress;
            const float t = static_cast<float>(GetTime()) * 65.0f;
            return { std::sin(t) * mag, std::cos(t * 1.3f) * mag };
        }

        void DrawOverlay(int width, int height) const
        {
            if (flashTimer > 0.0f)
            {
                const float flashAlpha = (flashTimer / 0.12f) * 0.75f;
                DrawRectangle(0, 0, width, height, ColorAlpha(WHITE, flashAlpha));
            }
            if (active && alpha > 0.001f)
            {
                DrawRectangle(0, 0, width, height, ColorAlpha(UITheme::COLOR_BG_BASE, std::clamp(alpha, 0.0f, 1.0f)));
            }
        }
    };

    inline TransitionManager g_transition{};

    // Dibuja el fondo completo "full-bleed" que cubre TODA la ventana
    void DrawFullBleedBackground(int winW, int winH);

    // Dibuja un rectangulo biselado estilo arcade
    void DrawCutCornerRect(Rectangle rect, float chamfer, Color fill, Color border, float borderWidth = 2.0f);

    // Dibuja texto moderno usando Bungee o Chakra Petch con sombra de 2 px
    void DrawTextModern(const char* text, float x, float y, float logicalSize, Color color,
                        bool centerHoriz = false, bool isTitle = false);

    // Mide las dimensiones de un texto moderno
    Vector2 MeasureTextModern(const char* text, float logicalSize, bool isTitle = false);

    // Dibuja el logo "FIGHTERXD" con Bungee, contorno grueso, degradado y haz de brillo
    void DrawLogoFighterXD(float centerX, float centerY);

    // Dibuja una columna de botones con cursor unico deslizante (ease-out) y feedback de 80 ms
    int DrawSlidingButtonColumn(const char* const labels[], int count, float centerX, float startY,
                                float width, float height, float spacing, int& selectedIndex,
                                float dt, SlidingMenuState& state);

    // Dibuja un boton arcade individual (por ejemplo el boton BACK)
    bool DrawSingleArcadeButton(const char* label, float centerX, float centerY,
                                float width, float height, bool selected, float dt);

    // Dibuja la placa de identificacion biselada bajo PackMAN o Shadow
    void DrawCharacterNameplate(const char* name, float centerX, float y, Color accentColor);

    // Dibuja la barra de ayuda inferior (28 px)
    void DrawHelpBar(const char* helpText, int screenWidth = UITheme::VIRTUAL_WIDTH,
                     int screenHeight = UITheme::VIRTUAL_HEIGHT);
}
