#if __has_include("if/sprite_renderer.hpp")
#include "if/sprite_renderer.hpp"
#else
#include "sprite_renderer.hpp"
#endif

#include "ft/character.hpp"
#include <algorithm>
#include <filesystem>

namespace
{
constexpr int kCellSize = 200;
constexpr float kAlphaThreshold = 0.1F;

constexpr const char* kFiles[2][static_cast<std::size_t>(SpriteAnimation::Count)] = {
    { "Idle.png", "Run.png", "Jump.png", "Fall.png", "Attack1.png", "Attack2.png", "Take Hit.png" },
    { "Idle.png", "Run.png", "Jump.png", "Fall.png", "Attack1.png", "Attack2.png", "Take hit.png" },
};
}

bool SpriteRenderer::Load()
{
    bool anyLoaded = false;
    for (int fighter = 0; fighter < static_cast<int>(fighters_.size()); ++fighter)
    {
        for (std::size_t animation = 0; animation < static_cast<std::size_t>(SpriteAnimation::Count); ++animation)
        {
            anyLoaded = LoadStrip(fighter,
                                 static_cast<SpriteAnimation>(animation),
                                 std::string(Roster[fighter].spriteDirectory) + "/" + kFiles[fighter][animation]) || anyLoaded;
        }
    }
    return anyLoaded;
}

bool SpriteRenderer::LoadStrip(int fighterIndex, SpriteAnimation animation, const std::string& relativePath)
{
    if (fighterIndex < 0 || fighterIndex >= static_cast<int>(fighters_.size()))
    {
        return false;
    }

    const std::filesystem::path fullPath = std::filesystem::path(GetApplicationDirectory()) / relativePath;
    const std::string path = fullPath.string();
    Image image = LoadImage(path.c_str());
    if (!IsImageValid(image))
    {
        TraceLog(LOG_WARNING, "No se pudo cargar el sprite: %s", path.c_str());
        return false;
    }

    Strip& strip = fighters_[fighterIndex][static_cast<std::size_t>(animation)];
    if (image.height < kCellSize || image.width < kCellSize)
    {
        TraceLog(LOG_WARNING, "La hoja no contiene una celda de 200x200: %s", path.c_str());
        UnloadImage(image);
        return false;
    }

    strip.texture = LoadTextureFromImage(image);
    if (!IsTextureValid(strip.texture))
    {
        TraceLog(LOG_WARNING, "No se pudo crear la textura: %s", path.c_str());
        strip.texture = {};
        UnloadImage(image);
        return false;
    }

    // Derive the frame count from the loaded texture instead of asset-name assumptions.
    strip.frameCount = strip.texture.width / kCellSize;
    if (strip.texture.width % kCellSize != 0)
    {
        TraceLog(LOG_WARNING, "El ancho de %s no es múltiplo de 200; se ignora el sobrante", path.c_str());
    }
    strip.alphaBounds.reserve(static_cast<std::size_t>(strip.frameCount));

    for (int frameIndex = 0; frameIndex < strip.frameCount; ++frameIndex)
    {
        const Rectangle cell{ static_cast<float>(frameIndex * kCellSize), 0.0F,
                              static_cast<float>(kCellSize), static_cast<float>(kCellSize) };
        Image frame = ImageFromImage(image, cell);
        Rectangle alphaBounds{ 0.0F, 0.0F, static_cast<float>(kCellSize), static_cast<float>(kCellSize) };
        if (IsImageValid(frame))
        {
            const Rectangle measured = GetImageAlphaBorder(frame, kAlphaThreshold);
            if (measured.width > 0.0F && measured.height > 0.0F)
            {
                alphaBounds = measured;
            }
            UnloadImage(frame);
        }
        strip.alphaBounds.push_back(alphaBounds);
    }

    UnloadImage(image);
    TraceLog(LOG_INFO, "Sprite cargado: %s (%d cuadros)", path.c_str(), strip.frameCount);
    return strip.frameCount > 0;
}

void SpriteRenderer::Unload()
{
    for (auto& fighter : fighters_)
    {
        for (auto& strip : fighter)
        {
            if (IsTextureValid(strip.texture))
            {
                UnloadTexture(strip.texture);
            }
            strip = Strip{};
        }
    }
}

int SpriteRenderer::FrameCount(int fighterIndex, SpriteAnimation animation) const
{
    if (fighterIndex < 0 || fighterIndex >= static_cast<int>(fighters_.size()))
    {
        return 0;
    }
    return fighters_[fighterIndex][static_cast<std::size_t>(animation)].frameCount;
}

bool SpriteRenderer::Draw(int fighterIndex,
                          SpriteAnimation animation,
                          int stateFrame,
                          bool loop,
                          int facingDir,
                          Rectangle hurtbox,
                          float feetY,
                          Color tint) const
{
    if (fighterIndex < 0 || fighterIndex >= static_cast<int>(fighters_.size()))
    {
        return false;
    }

    const Strip& strip = fighters_[fighterIndex][static_cast<std::size_t>(animation)];
    if (!IsTextureValid(strip.texture) || strip.frameCount <= 0 || strip.alphaBounds.empty())
    {
        return false;
    }

    const int lastFrame = strip.frameCount - 1;
    const int requestedFrame = std::max(0, stateFrame / 5);
    const int frameIndex = loop ? requestedFrame % strip.frameCount : std::min(requestedFrame, lastFrame);
    const Rectangle alpha = strip.alphaBounds[static_cast<std::size_t>(frameIndex)];
    if (alpha.width <= 0.0F || alpha.height <= 0.0F)
    {
        return false;
    }

    const float scaleX = hurtbox.width / alpha.width;
    const float scaleY = hurtbox.height / alpha.height;
    const float scale = std::min(scaleX, scaleY);
    if (scale <= 0.0F)
    {
        return false;
    }

    const bool flipped = facingDir < 0;
    const float alphaCenterX = alpha.x + alpha.width * 0.5F;
    const float visibleCenterX = flipped ? static_cast<float>(kCellSize) - alphaCenterX : alphaCenterX;
    const float fighterCenterX = hurtbox.x + hurtbox.width * 0.5F;
    const float frameX = fighterCenterX - visibleCenterX * scale;
    const float frameY = feetY - (alpha.y + alpha.height) * scale;
    const Rectangle source{
        static_cast<float>(frameIndex * kCellSize),
        0.0F,
        flipped ? -static_cast<float>(kCellSize) : static_cast<float>(kCellSize),
        static_cast<float>(kCellSize),
    };
    const Rectangle destination{
        frameX,
        frameY,
        static_cast<float>(kCellSize) * scale,
        static_cast<float>(kCellSize) * scale,
    };
    DrawTexturePro(strip.texture, source, destination, Vector2{ 0.0F, 0.0F }, 0.0F, tint);
    return true;
}

void SpriteRenderer::DrawFallback(Rectangle hurtbox, float feetY, Color tint) const
{
    DrawRectangleRec(hurtbox, tint);
    DrawCircle(static_cast<int>(hurtbox.x + hurtbox.width * 0.5F),
               static_cast<int>(feetY - hurtbox.height - hurtbox.width * 0.35F),
               hurtbox.width * 0.35F,
               tint);
}
