#if __has_include("if/menu_renderer.hpp")
#include "if/menu_renderer.hpp"
#else
#include "menu_renderer.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace
{
struct LargeButtonPath
{
    const char* normal;
    const char* hover;
    const char* defaultText;
};

const LargeButtonPath kLargePaths[static_cast<std::size_t>(LargeButtonKind::Count)] = {
    { "Menu Buttons/Large Buttons/Large Buttons/Play Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Play col_Button.png",
      "PLAY" },
    { "Menu Buttons/Large Buttons/Large Buttons/Controls Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Controls  col_Button.png",
      "CONTROLS" },
    { "Menu Buttons/Large Buttons/Large Buttons/Options Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Options  col_Button.png",
      "OPTIONS" },
    { "Menu Buttons/Large Buttons/Large Buttons/Quit Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Quit  col_Button.png",
      "QUIT" },
    { "Menu Buttons/Large Buttons/Large Buttons/Resume Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Resume  col_Button.png",
      "RESUME" },
    { "Menu Buttons/Large Buttons/Large Buttons/New game Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/New Game  col_Button.png",
      "NEW GAME" },
    { "Menu Buttons/Large Buttons/Large Buttons/Menu Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Menu  col_Button.png",
      "MAIN MENU" },
    { "Menu Buttons/Large Buttons/Large Buttons/Back Button.png",
      "Menu Buttons/Large Buttons/Colored Large Buttons/Back  col_Button.png",
      "BACK" },
};

const char* kSquarePaths[static_cast<std::size_t>(SquareButtonKind::Count)] = {
    "Menu Buttons/Square Buttons/Colored Square Buttons/Up col_Square Button.png",
    "Menu Buttons/Square Buttons/Colored Square Buttons/Down col_Square Button.png",
    "Menu Buttons/Square Buttons/Colored Square Buttons/Back col_Square Button.png",
    "Menu Buttons/Square Buttons/Colored Square Buttons/Next col_Square Button.png",
    "Menu Buttons/Square Buttons/Colored Square Buttons/V col_Square Button.png",
    "Menu Buttons/Square Buttons/Colored Square Buttons/X col_Square Button.png",
    "Menu Buttons/Square Buttons/Square Buttons/Pause Square Button.png",
};
}

Texture2D MenuRenderer::LoadTextureRelative(const std::string& relativePath)
{
    // Intento 1: relativo al directorio del ejecutable (GetApplicationDirectory)
    const std::filesystem::path fullPath = std::filesystem::path(GetApplicationDirectory()) / relativePath;
    const std::string fullStr = fullPath.string();
    Image img = LoadImage(fullStr.c_str());

    // Intento 2: relativo al directorio actual de trabajo
    if (!IsImageValid(img))
    {
        img = LoadImage(relativePath.c_str());
    }

    if (!IsImageValid(img))
    {
        TraceLog(LOG_WARNING, "No se pudo cargar textura UI: %s", relativePath.c_str());
        return Texture2D{};
    }

    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (!IsTextureValid(tex))
    {
        TraceLog(LOG_WARNING, "Error al crear textura GPU para UI: %s", relativePath.c_str());
        return Texture2D{};
    }

    // Filtrado bilineal suave para escalas y redimensionado
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

bool MenuRenderer::Load()
{
    bool allLoaded = true;

    // Cargar botones grandes (Normal y Hover)
    for (std::size_t i = 0; i < static_cast<std::size_t>(LargeButtonKind::Count); ++i)
    {
        largeButtons_[i].normal = LoadTextureRelative(kLargePaths[i].normal);
        largeButtons_[i].normalLoaded = IsTextureValid(largeButtons_[i].normal);

        largeButtons_[i].hover = LoadTextureRelative(kLargePaths[i].hover);
        largeButtons_[i].hoverLoaded = IsTextureValid(largeButtons_[i].hover);

        buttonScales_[i] = 1.0F;

        if (!largeButtons_[i].normalLoaded || !largeButtons_[i].hoverLoaded)
        {
            allLoaded = false;
        }
    }

    // Cargar botones cuadrados
    for (std::size_t i = 0; i < static_cast<std::size_t>(SquareButtonKind::Count); ++i)
    {
        squareButtons_[i] = LoadTextureRelative(kSquarePaths[i]);
        squareLoaded_[i] = IsTextureValid(squareButtons_[i]);

        if (!squareLoaded_[i])
        {
            allLoaded = false;
        }
    }

    return allLoaded;
}

void MenuRenderer::Unload()
{
    for (auto& btn : largeButtons_)
    {
        if (btn.normalLoaded)
        {
            UnloadTexture(btn.normal);
            btn.normalLoaded = false;
        }
        if (btn.hoverLoaded)
        {
            UnloadTexture(btn.hover);
            btn.hoverLoaded = false;
        }
        btn = MenuButtonTexture{};
    }

    for (std::size_t i = 0; i < static_cast<std::size_t>(SquareButtonKind::Count); ++i)
    {
        if (squareLoaded_[i])
        {
            UnloadTexture(squareButtons_[i]);
            squareLoaded_[i] = false;
        }
        squareButtons_[i] = Texture2D{};
    }
}

bool MenuRenderer::IsMouseOverLargeButton(float centerX, float centerY, float width, float height) const
{
    const Rectangle hitBox{ centerX - width * 0.5F, centerY - height * 0.5F, width, height };
    return CheckCollisionPointRec(GetMousePosition(), hitBox);
}

bool MenuRenderer::DrawLargeButton(LargeButtonKind kind, float centerX, float centerY, float width,
                                   bool selected, float dt, const char* fallbackText)
{
    const std::size_t idx = static_cast<std::size_t>(kind);
    // Aspect ratio 3:1 de los botones grandes (600x200)
    const float baseHeight = width / 3.0F;

    // Comprobacion de colision con el cursor del raton
    const Rectangle hitBox{ centerX - width * 0.5F, centerY - baseHeight * 0.5F, width, baseHeight };
    const bool mouseHover = CheckCollisionPointRec(GetMousePosition(), hitBox);
    const bool isHighlighted = selected || mouseHover;

    // Animacion suave de escala: 1.06x lerp al seleccionarse o hacer hover
    const float targetScale = isHighlighted ? 1.06F : 1.0F;
    if (buttonScales_[idx] <= 0.01F) buttonScales_[idx] = 1.0F;
    buttonScales_[idx] += (targetScale - buttonScales_[idx]) * std::clamp(14.0F * dt, 0.0F, 1.0F);
    const float currentScale = buttonScales_[idx];

    const float drawWidth = width * currentScale;
    const float drawHeight = baseHeight * currentScale;
    const Rectangle destRect{ centerX - drawWidth * 0.5F, centerY - drawHeight * 0.5F, drawWidth, drawHeight };

    const MenuButtonTexture& btn = largeButtons_[idx];
    const bool useHoverTex = isHighlighted && btn.hoverLoaded;
    const Texture2D tex = useHoverTex ? btn.hover : btn.normal;
    const bool hasValidTexture = useHoverTex ? btn.hoverLoaded : btn.normalLoaded;

    if (hasValidTexture)
    {
        const Rectangle srcRect{ 0.0F, 0.0F, static_cast<float>(tex.width), static_cast<float>(tex.height) };
        DrawTexturePro(tex, srcRect, destRect, Vector2{ 0.0F, 0.0F }, 0.0F, WHITE);
    }
    else
    {
        // Respaldo visual en caso de que la textura no este disponible
        const Color bg = isHighlighted ? Color{ 175, 42, 48, 240 } : Color{ 36, 48, 68, 230 };
        const Color border = isHighlighted ? YELLOW : SKYBLUE;
        DrawRectangleRounded(destRect, 0.22F, 4, bg);
        DrawRectangleRoundedLinesEx(destRect, 0.22F, 4, isHighlighted ? 3.0F : 2.0F, border);

        const char* text = fallbackText ? fallbackText : kLargePaths[idx].defaultText;
        const int fontSize = static_cast<int>(drawHeight * 0.38F);
        const int textW = MeasureText(text, fontSize);
        DrawText(text, static_cast<int>(centerX - textW * 0.5F),
                 static_cast<int>(centerY - fontSize * 0.5F), fontSize, isHighlighted ? YELLOW : RAYWHITE);
    }

    // Clic con raton
    if (mouseHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return true;
    }

    return false;
}

bool MenuRenderer::DrawSquareButton(SquareButtonKind kind, Rectangle dest, Color tint, bool clickable)
{
    const std::size_t idx = static_cast<std::size_t>(kind);
    const bool mouseHover = CheckCollisionPointRec(GetMousePosition(), dest);
    const Color drawTint = (clickable && mouseHover) ? YELLOW : tint;

    if (squareLoaded_[idx])
    {
        const Texture2D tex = squareButtons_[idx];
        const Rectangle srcRect{ 0.0F, 0.0F, static_cast<float>(tex.width), static_cast<float>(tex.height) };
        DrawTexturePro(tex, srcRect, dest, Vector2{ 0.0F, 0.0F }, 0.0F, drawTint);
    }
    else
    {
        // Respaldo visual cuadrado
        DrawRectangleRounded(dest, 0.25F, 4, Color{ 35, 48, 68, 230 });
        DrawRectangleRoundedLinesEx(dest, 0.25F, 4, 2.0F, drawTint);
    }

    if (clickable && mouseHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        return true;
    }

    return false;
}
