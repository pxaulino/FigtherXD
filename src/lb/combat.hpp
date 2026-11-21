#pragma once

#include <array>
#include <cstdint>

#include <raylib.h>

enum class AttackKind : std::uint8_t
{
    None,
    Jab,
    ForwardTilt,
    UpTilt,
    DownTilt,
    ForwardSmash,
    Nair,
    Fair,
    Bair,
    UpAir,
    DownAir,
    UpSpecial,
    Count
};

enum class HitEffect : std::uint8_t
{
    Normal,
    Fire,
    Electric,
    Slash
};

struct HitboxSpec
{
    int id = 0;
    int priority = 0;
    float radius = 0.0F;
    Vector2 offset{};
    float damage = 0.0F;
    float angleDegrees = 0.0F;
    float baseKnockback = 0.0F;
    float knockbackGrowth = 0.0F;
    HitEffect effect = HitEffect::Normal;
};

struct AttackDefinition
{
    const char* name = "None";
    std::array<HitboxSpec, 2> hitboxes{};
    int hitboxCount = 0;
    int startupFrames = 1;
    int activeFrames = 1;
    int recoveryFrames = 1;
    bool aerial = false;
    bool chargeable = false;
};

struct AttackInstance
{
    AttackKind kind = AttackKind::None;
    int frame = 0;
    int chargeFrames = 0;
    std::array<Vector2, 2> previousCenters{};
    std::array<bool, 2> hasPrevious{};
    bool charging = false;
    bool hasConnected = false;
};

struct Fighter;

const AttackDefinition& GetAttackDefinition(AttackKind kind);
void StartAttack(Fighter& fighter, AttackKind kind);
void TickAttack(Fighter& fighter);
void ResolveCombat(Fighter& first, Fighter& second);
float CalculateMeleeKnockback(float percent, float damage, float weight, float knockbackGrowth, float baseKnockback);
int CalculateHitlag(float damage);
int CalculateHitstun(float knockback);
void ApplyDirectionalInfluence(Fighter& fighter);
bool IsAttackActive(const Fighter& fighter);
void DrawAttackHitboxes(const Fighter& fighter);
