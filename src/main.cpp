#include <raylib.h>
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "gm/game.hpp"
#include "ui/ui_components.hpp"
#include "ui/ui_theme.hpp"
#include "win32_icon.hpp"

static void SetupWindowIcon()
{
    const std::filesystem::path appDir = GetApplicationDirectory();
    const std::string candidatePaths[] = {
        (appDir / "icono.png").string(),
        "icono.png",
        (appDir / "icono.ico").string(),
        "icono.ico"
    };

    for (const auto& path : candidatePaths)
    {
        if (FileExists(path.c_str()))
        {
            Image icon = LoadImage(path.c_str());
            if (icon.data != nullptr)
            {
                // RGFW en raylib requiere estrictamente formato R8G8B8A8
                ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                SetWindowIcon(icon);
                UnloadImage(icon);
                break;
            }
        }
    }

    const std::wstring fallbackIco = (appDir / "icono.ico").wstring();
    SetupPlatformTaskbarIcon(GetWindowHandle(), fallbackIco.c_str());
}

int main()
{
    SetupPlatformAppId();

    // Ventana redimensionable, MSAA 4x y VSync
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT, "FighterXD");
    if (!IsWindowReady()) return 1;

    SetupWindowIcon();
    UITheme::FontManager::Instance().Load();
    constexpr bool audioEnabled = false;
    if constexpr (audioEnabled) UITheme::AudioManager::Instance().Init();

    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Evita que ESC cierre la ventana abruptamente

    // Búfer virtual de diseno (960x540)
    RenderTexture2D virtualTarget = LoadRenderTexture(UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT - HudBandHeight);
    SetTextureFilter(virtualTarget.texture, TEXTURE_FILTER_POINT);

    // Búfer intermedio para tecnica Sharp Bilinear (1920x1080)
    RenderTexture2D intermediateTarget = LoadRenderTexture(UITheme::VIRTUAL_WIDTH * 2, (UITheme::VIRTUAL_HEIGHT - HudBandHeight) * 2);
    SetTextureFilter(intermediateTarget.texture, TEXTURE_FILTER_BILINEAR);

    GameContext ctx{};
    InitGame(ctx);

    while (!WindowShouldClose() && !ctx.shouldQuit)
    {
        const float dt = GetFrameTime();

        // 1. Alternar pantalla completa con F11
        if (IsKeyPressed(KEY_F11) && !ctx.controlsCapturing &&
            (ctx.scene != SceneState::InGame || !HasControlBinding(KEY_F11)))
        {
            ToggleFullscreen();
            SetWindowFocused();
            ForceWindowActive(GetWindowHandle());
        }

        // 2. Gestion de visibilidad del cursor (oculto en combate, visible en menus)
        if (ctx.scene == SceneState::InGame)
        {
            if (!IsCursorHidden()) HideCursor();
        }
        else
        {
            if (IsCursorHidden()) ShowCursor();
        }

        // 3. Calculo de dimensiones de ventana y escalado (FitSmooth por defecto, PixelPerfect opcional)
        const int winW = GetScreenWidth();
        const int winH = GetScreenHeight();

        float scale = 1.0f;
        if (UITheme::s_scalingMode == UITheme::ScalingMode::PixelPerfect)
        {
            const int maxIntScale = std::min(winW / UITheme::VIRTUAL_WIDTH, winH / UITheme::VIRTUAL_HEIGHT);
            scale = (maxIntScale >= 1) ? static_cast<float>(maxIntScale) : std::max(0.001f, std::min(winW / float(UITheme::VIRTUAL_WIDTH), winH / float(UITheme::VIRTUAL_HEIGHT)));
        }
        else
        {
            // Escala fraccional uniforme para llenar la ventana al maximizar o en F11
            const float scaleX = static_cast<float>(winW) / static_cast<float>(UITheme::VIRTUAL_WIDTH);
            const float scaleY = static_cast<float>(winH) / static_cast<float>(UITheme::VIRTUAL_HEIGHT);
            scale = std::min(scaleX, scaleY);
            if (scale <= 0.0f) scale = 1.0f;
        }

        const float safeW = static_cast<float>(UITheme::VIRTUAL_WIDTH) * scale;
        const float safeH = static_cast<float>(UITheme::VIRTUAL_HEIGHT) * scale;
        const Vector2 offset = {
            std::floor((static_cast<float>(winW) - safeW) * 0.5f),
            std::floor((static_cast<float>(winH) - safeH) * 0.5f)
        };

        UITheme::SetViewportMetrics(scale, offset, winW, winH);


        // 4. Actualizacion de transicion suave entre pantallas
        UIComponents::g_transition.Update(dt, ctx.scene);

        // 5. Actualizacion logica fija a 60 FPS
        UpdateGame(ctx);

        const bool combat = ctx.scene == SceneState::InGame || ctx.scene == SceneState::Paused;
        const bool smooth = UITheme::s_scalingMode == UITheme::ScalingMode::FitSmooth;
        Texture2D world = virtualTarget.texture;
        const auto flipped = [](Texture2D image) {
            return Rectangle{0, 0, float(image.width), -float(image.height)};
        };
        if (combat)
        {
            BeginTextureMode(virtualTarget);
            RenderWorld(ctx);
            EndTextureMode();
            if (smooth)
            {
                BeginTextureMode(intermediateTarget);
                DrawTexturePro(world, flipped(world), {0, 0, float(intermediateTarget.texture.width),
                               float(intermediateTarget.texture.height)}, {}, 0, WHITE);
                EndTextureMode();
                world = intermediateTarget.texture;
            }
        }

        BeginDrawing();
        ClearBackground(UITheme::COLOR_BG_BASE);
        const Vector2 shake = UIComponents::g_transition.GetShakeOffset();
        const Vector2 origin{offset.x + shake.x, offset.y + shake.y};
        if (combat)
            DrawTexturePro(world, flipped(world), {origin.x, origin.y + HudBandHeight * scale, safeW, safeH - HudBandHeight * scale}, {}, 0, WHITE);
        else
            UIComponents::DrawFullBleedBackground(winW, winH);

        BeginMode2D(Camera2D{origin, {}, 0, scale});
        RenderInterface(ctx);
        EndMode2D();

        // Overlay de transicion con fundido y flash
        UIComponents::g_transition.DrawOverlay(winW, winH);

        EndDrawing();
    }

    ShutdownGame(ctx);
    UnloadRenderTexture(virtualTarget);
    UnloadRenderTexture(intermediateTarget);
    if constexpr (audioEnabled) UITheme::AudioManager::Instance().Unload();
    UITheme::FontManager::Instance().Unload();
    CloseWindow();
    return 0;
}
