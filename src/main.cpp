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
    UITheme::FontManager::Instance().Load(1.0f);
    UITheme::AudioManager::Instance().Init();

    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // Evita que ESC cierre la ventana abruptamente

    // Búfer virtual de diseno (960x540)
    RenderTexture2D virtualTarget = LoadRenderTexture(UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT);
    SetTextureFilter(virtualTarget.texture, TEXTURE_FILTER_POINT);

    // Búfer intermedio para tecnica Sharp Bilinear (1920x1080)
    RenderTexture2D intermediateTarget = LoadRenderTexture(UITheme::VIRTUAL_WIDTH * 2, UITheme::VIRTUAL_HEIGHT * 2);
    SetTextureFilter(intermediateTarget.texture, TEXTURE_FILTER_BILINEAR);

    GameContext ctx{};
    InitGame(ctx);

    while (!WindowShouldClose() && !ctx.shouldQuit)
    {
        const float dt = GetFrameTime();

        // 1. Alternar pantalla completa con F11
        if (IsKeyPressed(KEY_F11))
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
            scale = (maxIntScale >= 1) ? static_cast<float>(maxIntScale) : 1.0f;
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
        UITheme::FontManager::Instance().Load(scale);

        // 4. Actualizacion de transicion suave entre pantallas
        UIComponents::g_transition.Update(dt, ctx.scene);

        // 5. Actualizacion logica fija a 60 FPS
        UpdateGame(ctx);

        // 6. Renderizado de todo el juego en el búfer virtual de 960x540
        BeginTextureMode(virtualTarget);
        ClearBackground(BLANK);
        RenderGame(ctx);
        EndTextureMode();

        // 7. Presentacion en ventana con fondo "full-bleed" y tecnica Sharp Bilinear
        BeginDrawing();
        ClearBackground(UITheme::COLOR_BG_BASE);

        const bool isMenuScreen = (ctx.scene == SceneState::MainMenu ||
                                   ctx.scene == SceneState::Controls ||
                                   ctx.scene == SceneState::Options);

        if (isMenuScreen)
        {
            // En menus, el fondo cubre TODA la ventana (sin barras)
            UIComponents::DrawFullBleedBackground(winW, winH);
        }

        if (UITheme::s_scalingMode == UITheme::ScalingMode::FitSmooth)
        {
            // Paso Sharp Bilinear: 960x540 -> 1920x1080 (Point) -> tamano destino (Bilinear)
            BeginTextureMode(intermediateTarget);
            ClearBackground(BLANK);
            rlDisableColorBlend();
            const Rectangle srcVirt = { 0.0f, 0.0f, static_cast<float>(virtualTarget.texture.width), -static_cast<float>(virtualTarget.texture.height) };
            const Rectangle dstInter = { 0.0f, 0.0f, static_cast<float>(intermediateTarget.texture.width), static_cast<float>(intermediateTarget.texture.height) };
            DrawTexturePro(virtualTarget.texture, srcVirt, dstInter, { 0.0f, 0.0f }, 0.0f, WHITE);
            rlEnableColorBlend();
            EndTextureMode();

            const Vector2 shake = UIComponents::g_transition.GetShakeOffset();
            const Rectangle srcInter = { 0.0f, 0.0f, static_cast<float>(intermediateTarget.texture.width), -static_cast<float>(intermediateTarget.texture.height) };
            const Rectangle dstFinal = { offset.x + shake.x, offset.y + shake.y, safeW, safeH };
            DrawTexturePro(intermediateTarget.texture, srcInter, dstFinal, { 0.0f, 0.0f }, 0.0f, WHITE);
        }
        else
        {
            // Mapeo entero pixel-perfect directo
            const Vector2 shake = UIComponents::g_transition.GetShakeOffset();
            const Rectangle srcVirt = { 0.0f, 0.0f, static_cast<float>(virtualTarget.texture.width), -static_cast<float>(virtualTarget.texture.height) };
            const Rectangle dstFinal = { offset.x + shake.x, offset.y + shake.y, safeW, safeH };
            DrawTexturePro(virtualTarget.texture, srcVirt, dstFinal, { 0.0f, 0.0f }, 0.0f, WHITE);
        }

        // Overlay de transicion con fundido y flash
        UIComponents::g_transition.DrawOverlay(winW, winH);

        EndDrawing();
    }

    ShutdownGame(ctx);
    UnloadRenderTexture(virtualTarget);
    UnloadRenderTexture(intermediateTarget);
    UITheme::AudioManager::Instance().Unload();
    UITheme::FontManager::Instance().Unload();
    CloseWindow();
    return 0;
}
