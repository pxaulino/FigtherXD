#pragma once

#include <raylib.h>
#include <string>

enum class LargeButtonKind
{
    Play,
    Controls,
    Options,
    Quit,
    Resume,
    NewGame,
    Menu,
    Back,
    Count
};

enum class SquareButtonKind
{
    Up,
    Down,
    Left,
    Right,
    Check,
    Cross,
    Pause,
    Count
};

struct MenuButtonTexture
{
    Texture2D normal{};
    Texture2D hover{};
    bool normalLoaded = false;
    bool hoverLoaded = false;
};

class MenuRenderer
{
public:
    bool Load();
    void Unload();

    // Dibuja un boton grande centrado en (centerX, centerY).
    // Aplica interpolacion suave de escala (1.06x lerp al estar seleccionado o hovered).
    // Retorna true si el usuario hace clic izquierdo sobre el boton.
    bool DrawLargeButton(LargeButtonKind kind, float centerX, float centerY, float width,
                         bool selected, float dt, const char* fallbackText = nullptr);

    // Dibuja un boton cuadrado en el rectangulo destino.
    // Retorna true si es clickable y se hace clic izquierdo sobre el.
    bool DrawSquareButton(SquareButtonKind kind, Rectangle dest, Color tint = WHITE, bool clickable = false);

    // Obtiene el estado de hover del raton sobre el area de un boton grande
    bool IsMouseOverLargeButton(float centerX, float centerY, float width, float height) const;

private:
    MenuButtonTexture largeButtons_[static_cast<std::size_t>(LargeButtonKind::Count)]{};
    Texture2D squareButtons_[static_cast<std::size_t>(SquareButtonKind::Count)]{};
    bool squareLoaded_[static_cast<std::size_t>(SquareButtonKind::Count)]{};
    float buttonScales_[static_cast<std::size_t>(LargeButtonKind::Count)]{};

    Texture2D LoadTextureRelative(const std::string& relativePath);
};
