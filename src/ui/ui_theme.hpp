#pragma once

#include <raylib.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace UITheme
{
    // Resolucion de diseno (Safe Area 16:9)
    constexpr int VIRTUAL_WIDTH = 960;
    constexpr int VIRTUAL_HEIGHT = 540;

    // Paleta de colores oficial FighterXD
    constexpr Color COLOR_BG_BASE      = { 0x0A, 0x06, 0x12, 0xFF }; // #0A0612 Fondo base profundo y letterbox
    constexpr Color COLOR_BG_GRAD_MID  = { 0x1B, 0x0B, 0x3A, 0xFF }; // #1B0B3A Morado neon intermedio
    constexpr Color COLOR_BG_GRAD_TOP  = { 0x3A, 0x0C, 0xA3, 0xFF }; // #3A0CA3 Azul electrico superior
    constexpr Color COLOR_ACCENT_MAIN  = { 0xFF, 0x2E, 0x63, 0xFF }; // #FF2E63 Neon Crimson (Seleccion, P2)
    constexpr Color COLOR_ACCENT_CYAN  = { 0x00, 0xE5, 0xFF, 0xFF }; // #00E5FF Cian electrico (Bordes, P1)
    constexpr Color COLOR_HIGHLIGHT    = { 0xFF, 0xC4, 0x00, 0xFF }; // #FFC400 Amarillo dorado (Titulos)
    constexpr Color COLOR_ORANGE       = { 0xFF, 0x57, 0x22, 0xFF }; // #FF5722 Naranja para degradado de logo
    constexpr Color COLOR_TEXT_MAIN    = { 0xF5, 0xF3, 0xFF, 0xFF }; // #F5F3FF Texto principal
    constexpr Color COLOR_TEXT_MUTED   = { 0xB8, 0xB4, 0xD4, 0xFF }; // #B8B4D4 Texto secundario claro y de alto contraste

    constexpr Color PANEL = {20, 16, 39, 250};
    constexpr Color SURFACE = {32, 28, 54, 255};
    constexpr Color FOCUS = {77, 28, 65, 255};
    constexpr Color EDGE = {83, 77, 110, 255};
    constexpr float INSET = 12.0f;
    constexpr float KEY_HEIGHT = 24.0f;
    constexpr float DAMAGE_SIZE = 34.0f;

    // Identidades oficiales de jugadores
    constexpr Color P1_COLOR      = COLOR_ACCENT_CYAN;
    constexpr Color P2_COLOR      = COLOR_ACCENT_MAIN;

    // Subtitulo oficial del motor en ingles y espanol
    constexpr const char* SUBTITLE_TEXT = "PIXEL FIGHTING ENGINE";

    // Idioma del juego (Espanol por defecto, conmutable a Ingles)
    enum class Language
    {
        Spanish,
        English
    };
    inline Language s_language = Language::Spanish;

    inline const char* GetSubtitleText()
    {
        return (s_language == Language::Spanish) ? "MOTOR DE PELEA PIXEL" : "PIXEL FIGHTING ENGINE";
    }

    // --- METRICAS DE INTERFAZ ---
    constexpr float BUTTON_WIDTH        = 240.0f; // Ancho fijo de botones
    constexpr float BUTTON_HEIGHT       = 44.0f;  // Alto fijo de botones
    constexpr float BUTTON_SPACING      = 12.0f;  // Separacion entre botones
    constexpr float BUTTON_CHAMFER      = 8.0f;   // Biselado de esquinas


    constexpr float HELPBAR_HEIGHT      = 32.0f;  // Alto de la barra inferior

    // --- TAMANOS LOGICOS DE FUENTE (Optimizados 1:1 para nitidez absoluta en 960x540) ---
    constexpr float FONT_SIZE_SMALL     = 16.0f; // Descripciones, ayudas, datos secundarios
    constexpr float FONT_SIZE_BODY      = 20.0f; // Opciones, nombres de personaje, subtitulos
    constexpr float FONT_SIZE_BUTTON    = 24.0f; // Botones principales y texto de accion
    constexpr float FONT_SIZE_TITLE     = 36.0f; // Titulos de pantalla (OPTIONS, CONTROLS, PAUSED)
    constexpr float FONT_SIZE_LOGO      = 64.0f; // Logo principal

    // Modalidad de escalado
    enum class ScalingMode
    {
        FitSmooth,    // Fraccional con sharp bilinear (llena la pantalla sin barras excesivas)
        PixelPerfect  // Escala entera estricta (1x, 2x, 3x...)
    };
    inline ScalingMode s_scalingMode = ScalingMode::FitSmooth;

    // Estado del viewport y proyeccion
    inline float s_viewportScale = 1.0f;
    inline Vector2 s_viewportOffset = { 0.0f, 0.0f };
    inline int s_windowWidth = VIRTUAL_WIDTH;
    inline int s_windowHeight = VIRTUAL_HEIGHT;

    inline void SetViewportMetrics(float scale, Vector2 offset, int winW, int winH)
    {
        s_viewportScale = (scale > 0.0f) ? scale : 1.0f;
        s_viewportOffset = offset;
        s_windowWidth = winW;
        s_windowHeight = winH;
    }

    // UNA sola funcion ScreenToUI() para convertir el raton de coordenadas de ventana a espacio 960x540
    inline Vector2 ScreenToUI(Vector2 screenPos)
    {
        Vector2 uiPos;
        uiPos.x = (screenPos.x - s_viewportOffset.x) / s_viewportScale;
        uiPos.y = (screenPos.y - s_viewportOffset.y) / s_viewportScale;
        return uiPos;
    }

    inline Vector2 GetVirtualMousePosition()
    {
        return ScreenToUI(::GetMousePosition());
    }

    // Gestion de Fuentes Tipograficas Modernas (Bungee y Chakra Petch)
    class FontManager
    {
    public:
        static FontManager& Instance()
        {
            static FontManager s_instance;
            return s_instance;
        }

        void Load()
        {
            if (loaded_) return;
            const auto fonts = std::filesystem::path(GetApplicationDirectory()) / "assets/fonts";
            std::vector<int> letters;
            for (int c = 32; c <= 255; ++c) letters.push_back(c);
            auto atlas = [&](const char* name, float size) {
                auto path = fonts / name;
                if (!std::filesystem::exists(path)) path = std::filesystem::path("assets/fonts") / name;
                if (!std::filesystem::exists(path)) return GetFontDefault();
                Font f = LoadFontEx(path.string().c_str(), int(size * 3), letters.data(), int(letters.size()));
                if (!IsFontValid(f) || f.texture.id == GetFontDefault().texture.id) return GetFontDefault();
                GenTextureMipmaps(&f.texture);
                SetTextureFilter(f.texture, TEXTURE_FILTER_TRILINEAR);
                return f;
            };
            fontLogo_ = atlas("Bungee-Regular.ttf", FONT_SIZE_LOGO);
            fontTitle_ = atlas("Bungee-Regular.ttf", FONT_SIZE_TITLE);
            fontButton_ = atlas("ChakraPetch-SemiBold.ttf", FONT_SIZE_BUTTON);
            fontBody_ = atlas("ChakraPetch-SemiBold.ttf", FONT_SIZE_BODY);
            fontSmall_ = atlas("ChakraPetch-Regular.ttf", FONT_SIZE_SMALL);
            loaded_ = true;
        }

        void Unload()
        {
            if (!loaded_) return;
            for (Font f : {fontLogo_, fontTitle_, fontButton_, fontBody_, fontSmall_})
                if (f.texture.id != GetFontDefault().texture.id) UnloadFont(f);
            loaded_ = false;
        }

        Font GetLogoFont() const { return fontLogo_; }
        Font GetTitleFont() const { return fontTitle_; }
        Font GetButtonFont() const { return fontButton_; }
        Font GetBodyFont() const { return fontBody_; }
        Font GetSmallFont() const { return fontSmall_; }
        bool IsLoaded() const { return loaded_; }

    private:
        FontManager() = default;
        Font fontLogo_{};
        Font fontTitle_{};
        Font fontButton_{};
        Font fontBody_{};
        Font fontSmall_{};
        bool loaded_ = false;
    };

    // Gestor de audio sintetizado por codigo (sin archivos externos, 100% Raylib)
    class AudioManager
    {
    public:
        static AudioManager& Instance()
        {
            static AudioManager s_instance;
            return s_instance;
        }

        void Init()
        {
            if (initialized_) return;
            InitAudioDevice();
            if (!IsAudioDeviceReady())
            {
                std::cout << "[Audio] Audio device not available. Running in silent mode.\n";
                return;
            }

            cursorSound_ = CreateProceduralBeep(440.0f, 660.0f, 0.045f, 0.40f, 1);    // 8-bit chip blip
            confirmSound_ = CreateProceduralBeep(587.0f, 880.0f, 0.10f, 0.50f, 0);    // Chime suave
            cancelSound_ = CreateProceduralBeep(440.0f, 220.0f, 0.075f, 0.40f, 0);    // Blip descendente
            playStartSound_ = CreateProceduralBeep(330.0f, 988.0f, 0.22f, 0.60f, 1);  // Fanfarria arcade punchy

            initialized_ = true;
            std::cout << "[Audio] Synthesized UI audio effects loaded successfully.\n";
        }

        void PlayCursor()
        {
            if (initialized_ && IsSoundValid(cursorSound_)) PlaySound(cursorSound_);
        }

        void PlayConfirm()
        {
            if (initialized_ && IsSoundValid(confirmSound_)) PlaySound(confirmSound_);
        }

        void PlayCancel()
        {
            if (initialized_ && IsSoundValid(cancelSound_)) PlaySound(cancelSound_);
        }

        void PlayStart()
        {
            if (initialized_ && IsSoundValid(playStartSound_)) PlaySound(playStartSound_);
        }

        void Unload()
        {
            if (initialized_)
            {
                if (IsSoundValid(cursorSound_)) UnloadSound(cursorSound_);
                if (IsSoundValid(confirmSound_)) UnloadSound(confirmSound_);
                if (IsSoundValid(cancelSound_)) UnloadSound(cancelSound_);
                if (IsSoundValid(playStartSound_)) UnloadSound(playStartSound_);
                CloseAudioDevice();
                initialized_ = false;
            }
        }

    private:
        AudioManager() = default;

        static Sound CreateProceduralBeep(float startFreq, float endFreq, float duration, float volume, int waveformType)
        {
            const unsigned int sampleRate = 44100;
            const unsigned int frameCount = static_cast<unsigned int>(duration * sampleRate);
            short* samples = static_cast<short*>(MemAlloc(frameCount * sizeof(short)));
            if (!samples) return Sound{};

            for (unsigned int i = 0; i < frameCount; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
                const float progress = static_cast<float>(i) / static_cast<float>(frameCount);
                const float freq = startFreq + (endFreq - startFreq) * progress;
                const float envelope = (progress < 0.08f) ? (progress / 0.08f) : (1.0f - progress);

                const float phase = t * freq * 2.0f * 3.14159265f;
                float sampleVal = 0.0f;
                if (waveformType == 1) // Square
                {
                    sampleVal = (std::sin(phase) >= 0.0f) ? 0.65f : -0.65f;
                }
                else // Sine
                {
                    sampleVal = std::sin(phase);
                }

                samples[i] = static_cast<short>(sampleVal * envelope * volume * 32767.0f);
            }

            Wave wave{};
            wave.frameCount = frameCount;
            wave.sampleRate = sampleRate;
            wave.sampleSize = 16;
            wave.channels = 1;
            wave.data = samples;

            Sound sound = LoadSoundFromWave(wave);
            UnloadWave(wave);
            return sound;
        }

        Sound cursorSound_{};
        Sound confirmSound_{};
        Sound cancelSound_{};
        Sound playStartSound_{};
        bool initialized_ = false;
    };
}
