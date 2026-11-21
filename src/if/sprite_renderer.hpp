#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <raylib.h>

enum class SpriteAnimation : std::size_t
{
    Idle,
    Run,
    Jump,
    Fall,
    AttackJab,
    AttackAerial,
    DamageFly,
    Count
};

class SpriteRenderer
{
public:
    bool Load();
    void Unload();

    int FrameCount(int fighterIndex, SpriteAnimation animation) const;
    bool Draw(int fighterIndex,
              SpriteAnimation animation,
              int stateFrame,
              bool loop,
              int facingDir,
              Rectangle hurtbox,
              float feetY,
              Color tint = WHITE) const;
    void DrawFallback(Rectangle hurtbox, float feetY, Color tint) const;

private:
    struct Strip
    {
        Texture2D texture{};
        int frameCount = 0;
        std::vector<Rectangle> alphaBounds;
    };

    using FighterStrips = std::array<Strip, static_cast<std::size_t>(SpriteAnimation::Count)>;
    std::array<FighterStrips, 2> fighters_{};

    bool LoadStrip(int fighterIndex, SpriteAnimation animation, const std::string& relativePath);
};
