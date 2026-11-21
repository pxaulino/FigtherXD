#pragma once

#include <array>
#include <vector>

#if __has_include("ft/fighter.hpp")
#include "ft/fighter.hpp"
#else
#include "../ft/fighter.hpp"
#endif

struct StagePlatform
{
    float left = 0.0F;
    float right = 0.0F;
    float top = 0.0F;
    bool passThrough = false;
};

struct Stage
{
    float groundY = 455.0F;
    float groundLeft = 170.0F;
    float groundRight = 790.0F;
    float blastLeft = -250.0F;
    float blastRight = 1210.0F;
    float blastTop = -340.0F;
    float blastBottom = 820.0F;
    std::vector<StagePlatform> platforms{};
    std::array<int, 2> ledgeOccupants{ -1, -1 };
};

enum class CollisionResult
{
    None,
    Landed,
    GrabbedLedge,
    KnockedOut
};

Stage CreateTrainingStage();
CollisionResult ResolveStageCollision(Stage& stage, Fighter& fighter, Vector2 previousPosition);
void ReleaseLedge(Stage& stage, Fighter& fighter);
void DrawStage(const Stage& stage);
