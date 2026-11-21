#pragma once

#include <array>

#include "ft/fighter.hpp"
#include "raylib.h"

struct CompetitiveCamera
{
    Camera2D camera{};
    Vector2 target{};
    float zoom = 1.0F;
};

void InitializeCamera(CompetitiveCamera& camera);
void UpdateCamera(CompetitiveCamera& camera, const std::array<Fighter, 2>& fighters, float dt);
