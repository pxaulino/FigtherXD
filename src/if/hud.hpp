#pragma once

#include <array>

#include "ft/fighter.hpp"

enum class OpponentMode
{
    DummyQuiet,
    CpuBasic,
    DummyDiShield
};

#include "if/sprite_renderer.hpp"

inline constexpr int HudBandHeight = 72;

void DrawHud(const SpriteRenderer& sprites, const std::array<float,2>& stockFlash, const std::array<Fighter, 2>& fighters, OpponentMode mode, bool debugMode, float fps);
