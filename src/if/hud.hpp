#pragma once

#include <array>

#include "ft/fighter.hpp"

enum class OpponentMode
{
    DummyQuiet,
    CpuBasic,
    DummyDiShield
};

void DrawHud(const std::array<Fighter, 2>& fighters, OpponentMode mode, bool debugMode, float fps);
