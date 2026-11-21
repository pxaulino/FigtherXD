#include "ui/ui_components.hpp"

#include <algorithm>
#include <cmath>

namespace UIComponents
{
    void DrawFullBleedBackground(int winW, int winH)
    {
        const float w = static_cast<float>(winW);
        const float h = static_cast<float>(winH);
        const float time = static_cast<float>(GetTime());

        // 1. Degradado vertical completo que cubre TODA la ventana
        DrawRectangleGradientV(0, 0, winW, winH / 2, UITheme::COLOR_BG_GRAD_TOP, UITheme::COLOR_BG_GRAD_MID);
        DrawRectangleGradientV(0, winH / 2, winW, winH - (winH / 2), UITheme::COLOR_BG_GRAD_MID, UITheme::COLOR_BG_BASE);

        // 2. Capa Parallax 1: Siluetas de rascacielos lejanos (desplazamiento lento a 5 px/s)
        const float baseCityY = h * 0.74f;
        const float scrollFar = std::fmod(time * 5.0f, 200.0f);
        const Color farCityColor = Color{ 14, 8, 28, 175 };
        for (float bx = -200.0f; bx < w + 200.0f; bx += 200.0f)
        {
            const float curX = bx - scrollFar;
            DrawRectangle(static_cast<int>(curX + 8), static_cast<int>(baseCityY - 45), 36, static_cast<int>(h - baseCityY + 45), farCityColor);
            DrawRectangle(static_cast<int>(curX + 54), static_cast<int>(baseCityY - 80), 48, static_cast<int>(h - baseCityY + 80), farCityColor);
            DrawRectangle(static_cast<int>(curX + 112), static_cast<int>(baseCityY - 35), 32, static_cast<int>(h - baseCityY + 35), farCityColor);
            DrawRectangle(static_cast<int>(curX + 152), static_cast<int>(baseCityY - 65), 38, static_cast<int>(h - baseCityY + 65), farCityColor);
        }

        // 3. Capa Parallax 2: Siluetas de edificios medios (desplazamiento a 12 px/s)
        const float scrollMid = std::fmod(time * 12.0f, 240.0f);
        const Color midCityColor = Color{ 20, 10, 38, 220 };
        for (float bx = -240.0f; bx < w + 240.0f; bx += 240.0f)
        {
            const float curX = bx - scrollMid;
            DrawRectangle(static_cast<int>(curX + 12), static_cast<int>(baseCityY - 30), 50, static_cast<int>(h - baseCityY + 30), midCityColor);
            DrawRectangle(static_cast<int>(curX + 72), static_cast<int>(baseCityY - 60), 40, static_cast<int>(h - baseCityY + 60), midCityColor);
            DrawRectangle(static_cast<int>(curX + 122), static_cast<int>(baseCityY - 105), 55, static_cast<int>(h - baseCityY + 105), midCityColor);
            DrawRectangle(static_cast<int>(curX + 188), static_cast<int>(baseCityY - 42), 42, static_cast<int>(h - baseCityY + 42), midCityColor);
        }

        // 4. Rejilla sutil retro a lo largo de toda la ventana
        const Color gridColor = ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.04f);
        for (float x = 0.0f; x <= w; x += 60.0f)
        {
            DrawLineEx({ x, 0.0f }, { x, h }, 1.0f, gridColor);
        }
        for (float y = 0.0f; y <= h; y += 45.0f)
        {
            DrawLineEx({ 0.0f, y }, { w, y }, 1.0f, gridColor);
        }

        // 5. Chispas y particulas de energia flotando hacia arriba
        for (int i = 0; i < 30; ++i)
        {
            const float seed = static_cast<float>(i * 53);
            const float speed = 22.0f + std::fmod(seed * 1.6f, 30.0f);
            const float px = std::fmod(seed * 89.0f + std::sin(time * 0.9f + seed) * 35.0f, w);
            const float totalY = h + 50.0f;
            const float rawY = totalY - std::fmod(time * speed + seed * 23.0f, totalY);
            const float py = rawY - 25.0f;

            const float life = py / h;
            const float alpha = std::sin(std::clamp(life, 0.0f, 1.0f) * 3.14159f) * 0.38f;
            if (alpha <= 0.02f) continue;

            const Color pColor = (i % 3 == 0) ? UITheme::COLOR_ACCENT_CYAN :
                                 ((i % 3 == 1) ? UITheme::COLOR_ACCENT_MAIN : UITheme::COLOR_HIGHLIGHT);
            DrawCircleV({ px, py }, (i % 4 == 0) ? 2.5f : 1.5f, ColorAlpha(pColor, alpha));
        }

        // 6. Vineta perimetral suave sobre los bordes de la ventana
        const int vignetteSide = std::min(winW / 6, 160);
        const int vignetteTop = std::min(winH / 6, 120);

        DrawRectangleGradientH(0, 0, vignetteSide, winH, ColorAlpha(UITheme::COLOR_BG_BASE, 0.75f), ColorAlpha(UITheme::COLOR_BG_BASE, 0.0f));
        DrawRectangleGradientH(winW - vignetteSide, 0, vignetteSide, winH, ColorAlpha(UITheme::COLOR_BG_BASE, 0.0f), ColorAlpha(UITheme::COLOR_BG_BASE, 0.75f));
        DrawRectangleGradientV(0, 0, winW, vignetteTop, ColorAlpha(UITheme::COLOR_BG_BASE, 0.70f), ColorAlpha(UITheme::COLOR_BG_BASE, 0.0f));
        DrawRectangleGradientV(0, winH - vignetteTop, winW, vignetteTop, ColorAlpha(UITheme::COLOR_BG_BASE, 0.0f), ColorAlpha(UITheme::COLOR_BG_BASE, 0.70f));
    }

    void DrawCutCornerRect(Rectangle rect, float chamfer, Color fill, Color border, float borderWidth)
    {
        const float x = rect.x;
        const float y = rect.y;
        const float w = rect.width;
        const float h = rect.height;
        const float c = (chamfer < w * 0.5f && chamfer < h * 0.5f) ? chamfer : 4.0f;

        const Vector2 v[8] = {
            { x + c, y },             // 0
            { x + w - c, y },         // 1
            { x + w, y + c },         // 2
            { x + w, y + h - c },     // 3
            { x + w - c, y + h },     // 4
            { x + c, y + h },         // 5
            { x, y + h - c },         // 6
            { x, y + c }              // 7
        };

        // Relleno de las 3 zonas internas
        DrawRectangleRec({ x + c, y, w - 2.0f * c, h }, fill);
        DrawRectangleRec({ x, y + c, c, h - 2.0f * c }, fill);
        DrawRectangleRec({ x + w - c, y + c, c, h - 2.0f * c }, fill);

        // Triangulos de esquina
        DrawTriangle(v[7], { x + c, y + c }, v[0], fill);
        DrawTriangle(v[1], { x + w - c, y + c }, v[2], fill);
        DrawTriangle(v[3], { x + w - c, y + h - c }, v[4], fill);
        DrawTriangle(v[5], { x + c, y + h - c }, v[6], fill);

        // Bordes biselados
        if (borderWidth > 0.0f && border.a > 0)
        {
            for (int i = 0; i < 8; ++i)
            {
                DrawLineEx(v[i], v[(i + 1) % 8], borderWidth, border);
            }
        }
    }

    void DrawButtonSurface(Rectangle rect, ButtonState state)
    {
        const bool focused = state == ButtonState::Focused || state == ButtonState::Pressed;
        const bool disabled = state == ButtonState::Disabled;
        const Color edge = focused ? UITheme::COLOR_ACCENT_MAIN : UITheme::EDGE;
        const Color fill = disabled ? UITheme::PANEL : (focused ? UITheme::FOCUS : UITheme::SURFACE);
        Rectangle shadow = rect;
        shadow.y += 3;
        DrawCutCornerRect(shadow, 6, Color{9, 6, 19, 210}, BLANK, 0);
        DrawCutCornerRect(rect, 6, fill, edge, focused ? 2 : 1);
        if (focused)
        {
            DrawRectangleRec({rect.x + 5, rect.y + 7, 3, rect.height - 14}, UITheme::COLOR_ACCENT_CYAN);
            if (state == ButtonState::Pressed)
                DrawRectangleRec({rect.x + 10, rect.y + rect.height - 3, rect.width - 20, 2}, UITheme::COLOR_HIGHLIGHT);
        }
    }

    struct TextFace { Font font; float tracking; };

    static TextFace Face(float size, bool display)
    {
        auto& fm = UITheme::FontManager::Instance();
        if (display) return {size >= UITheme::FONT_SIZE_LOGO ? fm.GetLogoFont() : fm.GetTitleFont(), 0.8f};
        if (size >= UITheme::FONT_SIZE_BUTTON) return {fm.GetButtonFont(), 0.7f};
        if (size >= UITheme::FONT_SIZE_BODY) return {fm.GetBodyFont(), 0.25f};
        return {fm.GetSmallFont(), 0.3f};
    }

    Vector2 MeasureTextModern(const char* text, float size, bool display)
    {
        const auto face = Face(size, display);
        return MeasureTextEx(face.font, text, size, face.tracking);
    }

    void DrawTextModern(const char* text, float x, float y, float size, Color color, bool centered, bool display)
    {
        const auto face = Face(size, display);
        if (centered) x -= MeasureTextEx(face.font, text, size, face.tracking).x * 0.5f;
        const float scale = UITheme::s_viewportScale;
        x = std::round(x * scale) / scale;
        y = std::round(y * scale) / scale;
        const auto ink = [&](float dx, float dy, Color tint) {
            DrawTextEx(face.font, text, {x + dx, y + dy}, size, face.tracking, tint);
        };
        const Color shadow = {8, 5, 18, color.a};
        if (display)
        {
            ink(-1, 0, shadow);
            ink(1, 0, shadow);
            ink(0, -1, shadow);
        }
        ink(0, display ? 2 : 1, shadow);
        ink(0, 0, color);
    }

    void DrawKeycap(const char* key, float x, float y)
    {
        const float size = UITheme::FONT_SIZE_SMALL;
        const float width = std::max(32.0f, MeasureTextModern(key, size).x + 16);
        DrawCutCornerRect({x, y, width, UITheme::KEY_HEIGHT}, 3, UITheme::SURFACE, UITheme::EDGE, 1);
        DrawRectangleRec({x + 4, y + UITheme::KEY_HEIGHT - 2, width - 8, 1}, UITheme::COLOR_ACCENT_CYAN);
        DrawTextModern(key, x + width / 2, y + 3, size, UITheme::COLOR_TEXT_MAIN, true, false);
    }

    void DrawLogoFighterXD(float centerX, float centerY)
    {
        const float size = UITheme::FONT_SIZE_LOGO;
        const float left = MeasureTextModern("FIGHTER", size, true).x;
        const float right = MeasureTextModern("XD", size, true).x;
        const float x = centerX - (left + right + 2) / 2;
        const float y = centerY - size / 2;
        DrawTextModern("FIGHTER", x, y, size, UITheme::COLOR_TEXT_MAIN, false, true);
        DrawTextModern("XD", x + left + 2, y, size, UITheme::COLOR_HIGHLIGHT, false, true);
        DrawRectangleRec({x, y + size, left, 2}, UITheme::COLOR_ACCENT_CYAN);
        DrawRectangleRec({x + left + 2, y + size, right, 2}, UITheme::COLOR_ACCENT_MAIN);
    }

    int DrawSlidingButtonColumn(const char* const labels[], int count, float centerX, float startY,
                                float width, float height, float spacing, int& selectedIndex,
                                float dt, SlidingMenuState& state)
    {
        const Vector2 vMouse = UITheme::GetVirtualMousePosition();
        const Vector2 screenMouse = GetMousePosition();

        // 1. Coherencia raton/teclado: el hover solo cambia la seleccion si el raton se mueve
        const bool mouseMoved = (std::abs(screenMouse.x - state.lastMousePos.x) > 1.5f ||
                                 std::abs(screenMouse.y - state.lastMousePos.y) > 1.5f);
        if (mouseMoved)
        {
            state.lastMousePos = screenMouse;
            for (int i = 0; i < count; ++i)
            {
                const float btnY = startY + static_cast<float>(i) * spacing;
                const Rectangle rec = { centerX - width * 0.5f, btnY - height * 0.5f, width, height };
                if (CheckCollisionPointRec(vMouse, rec))
                {
                    if (selectedIndex != i)
                    {
                        UITheme::AudioManager::Instance().PlayCursor();
                    }
                    selectedIndex = i;
                    break;
                }
            }
        }

        // 2. Cursor unico deslizante con interpolacion suave (ease-out, ~120 ms)
        const float targetY = startY + static_cast<float>(selectedIndex) * spacing;
        if (state.currentY < 0.0f)
        {
            state.currentY = targetY; // Inicializacion instantanea la primera vez
        }
        else
        {
            // Amortiguacion exponencial para un ease-out suave y natural
            state.currentY += (targetY - state.currentY) * (1.0f - std::exp(-22.0f * dt));
            if (std::abs(targetY - state.currentY) < 0.25f)
            {
                state.currentY = targetY;
            }
        }

        // 3. Temporizador de feedback al presionar (80 ms a escala 0.97x)
        if (state.pressTimer > 0.0f)
        {
            state.pressTimer -= dt;
            if (state.pressTimer <= 0.0f)
            {
                state.pressedIndex = -1;
            }
        }

        // 4. Dibujar fondo estandar de todos los botones (sin moverse)
        for (int i = 0; i < count; ++i)
        {
            const float btnY = startY + static_cast<float>(i) * spacing;
            const Rectangle btnRec = {
                std::floor(centerX - width * 0.5f),
                std::floor(btnY - height * 0.5f),
                width,
                height
            };

            DrawButtonSurface(btnRec, ButtonState::Normal);
        }

        // 5. Dibujar el unico cursor de seleccion deslizante
        const Rectangle highlightRec = {
            std::floor(centerX - width * 0.5f),
            std::floor(state.currentY - height * 0.5f),
            width,
            height
        };

        // Resplandor exterior (Glow)
        DrawCutCornerRect({ highlightRec.x - 3.0f, highlightRec.y - 3.0f, highlightRec.width + 6.0f, highlightRec.height + 6.0f },
                          UITheme::BUTTON_CHAMFER + 2.0f,
                          ColorAlpha(UITheme::COLOR_ACCENT_MAIN, 0.15f),
                          ColorAlpha(UITheme::COLOR_ACCENT_MAIN, 0.45f), 2.0f);

        DrawButtonSurface(highlightRec, state.pressedIndex == selectedIndex && state.pressTimer > 0
                          ? ButtonState::Pressed : ButtonState::Focused);

        // Flechas ▶ y ◀ a 16 px de los bordes del cursor con pulso suave
        const float pulse = std::sin(static_cast<float>(GetTime()) * 8.0f) * 2.5f;
        const float midY = std::floor(state.currentY);
        const Color arrowColor = UITheme::COLOR_HIGHLIGHT;

        // Flecha izquierda ▶
        const float leftX = std::floor(highlightRec.x + 16.0f + pulse);
        DrawTriangle({ leftX - 5.0f, midY - 6.0f },
                     { leftX - 5.0f, midY + 6.0f },
                     { leftX + 6.0f, midY }, arrowColor);

        // Flecha derecha ◀
        const float rightX = std::floor(highlightRec.x + width - 16.0f - pulse);
        DrawTriangle({ rightX + 5.0f, midY - 6.0f },
                     { rightX + 5.0f, midY + 6.0f },
                     { rightX - 6.0f, midY }, arrowColor);

        // 6. Dibujar las etiquetas de texto
        int clickedIndex = -1;
        for (int i = 0; i < count; ++i)
        {
            const float btnY = startY + static_cast<float>(i) * spacing;
            const Rectangle btnRec = { centerX - width * 0.5f, btnY - height * 0.5f, width, height };

            // Comprobar clic del raton
            if (CheckCollisionPointRec(vMouse, btnRec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            {
                clickedIndex = i;
                state.pressedIndex = i;
                state.pressTimer = 0.08f; // 80 ms a escala 0.97
                UITheme::AudioManager::Instance().PlayConfirm();
            }

            // Aplicar escala 0.97 al texto si fue presionado recientemente
            const bool isPressed = (state.pressedIndex == i && state.pressTimer > 0.0f);
            const float currentFontSize = isPressed ? (UITheme::FONT_SIZE_BUTTON * 0.97f) : UITheme::FONT_SIZE_BUTTON;

            const float textY = std::floor(btnY - currentFontSize * 0.5f);
            const Color textColor = isPressed ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MAIN;

            DrawTextModern(labels[i], centerX, textY, currentFontSize, textColor, true, false);
        }

        // Si se confirma por teclado (ENTER / SPACE / X)
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_X))
        {
            state.pressedIndex = selectedIndex;
            state.pressTimer = 0.08f;
        }

        return clickedIndex;
    }

    bool DrawSingleArcadeButton(const char* label, float centerX, float centerY,
                                float width, float height, bool selected, float dt)
    {
        (void)dt;
        const Vector2 vMouse = UITheme::GetVirtualMousePosition();
        const Rectangle btnRec = {
            std::floor(centerX - width * 0.5f),
            std::floor(centerY - height * 0.5f),
            width,
            height
        };
        const bool hovered = CheckCollisionPointRec(vMouse, btnRec);
        const bool active = selected;

        const bool pressed = hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        DrawButtonSurface(btnRec, pressed ? ButtonState::Pressed :
                          (active || hovered ? ButtonState::Focused : ButtonState::Normal));

        if (active)
        {
            const float pulse = std::sin(static_cast<float>(GetTime()) * 8.0f) * 2.0f;
            const float midY = std::floor(centerY);
            DrawTriangle({ btnRec.x + 12.0f + pulse, midY - 5.0f },
                         { btnRec.x + 12.0f + pulse, midY + 5.0f },
                         { btnRec.x + 22.0f + pulse, midY }, UITheme::COLOR_HIGHLIGHT);

            DrawTriangle({ btnRec.x + width - 12.0f - pulse, midY - 5.0f },
                         { btnRec.x + width - 12.0f - pulse, midY + 5.0f },
                         { btnRec.x + width - 22.0f - pulse, midY }, UITheme::COLOR_HIGHLIGHT);
        }

        const float textY = std::floor(centerY - UITheme::FONT_SIZE_BUTTON * 0.5f);
        DrawTextModern(label, centerX, textY, UITheme::FONT_SIZE_BUTTON, UITheme::COLOR_TEXT_MAIN, true, false);

        const bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (clicked)
        {
            UITheme::AudioManager::Instance().PlayConfirm();
        }
        return clicked;
    }

    void DrawHelpBar(const char* helpText, int screenWidth, int screenHeight)
    {
        const float barH = UITheme::HELPBAR_HEIGHT;
        const float barY = static_cast<float>(screenHeight) - barH;

        DrawRectangle(0, static_cast<int>(barY), screenWidth, static_cast<int>(barH), ColorAlpha(UITheme::COLOR_BG_BASE, 0.94f));
        DrawLine(0, static_cast<int>(barY), screenWidth, static_cast<int>(barY), ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.45f));

        struct Shortcut { std::string key, action; float keyWidth, width; };
        static std::string hint;
        static std::vector<Shortcut> shortcuts;
        static float total = 0;
        if (hint != helpText)
        {
            hint = helpText;
            shortcuts.clear();
            total = 0;
            size_t pos = 0;
            while ((pos = hint.find('[', pos)) != std::string::npos)
            {
                const auto close = hint.find(']', pos);
                if (close == std::string::npos) break;
                const auto next = hint.find('[', close + 1);
                const std::string key = hint.substr(pos + 1, close - pos - 1);
                std::string action = hint.substr(close + 1, next - close - 1);
                action.erase(0, action.find_first_not_of(' '));
                action.erase(action.find_last_not_of(' ') + 1);
                const float keyWidth = std::max(32.0f, MeasureTextModern(key.c_str(), UITheme::FONT_SIZE_SMALL).x + 16);
                const float width = keyWidth + 6 + MeasureTextModern(action.c_str(), UITheme::FONT_SIZE_SMALL).x;
                shortcuts.push_back({key, action, keyWidth, width});
                total += width + 24;
                pos = close + 1;
            }
            total -= 24;
        }
        if (shortcuts.empty() || total > screenWidth - 24)
        {
            DrawTextModern(helpText, screenWidth / 2.0f, barY + 7, UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, true, false);
            return;
        }
        float x = (screenWidth - total) / 2;
        for (const auto& shortcut : shortcuts)
        {
            DrawKeycap(shortcut.key.c_str(), x, barY + 4);
            DrawTextModern(shortcut.action.c_str(), x + shortcut.keyWidth + 6, barY + 8,
                           UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, false, false);
            x += shortcut.width + 24;
        }
    }
}
