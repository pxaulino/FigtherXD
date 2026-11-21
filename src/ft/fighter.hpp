#pragma once

#include <cstdint>

#include <raylib.h>

#if __has_include("lb/combat.hpp")
#include "lb/combat.hpp"
#else
#include "../lb/combat.hpp"
#endif

// Posicion del luchador colgado del borde respecto a la esquina del escenario
inline constexpr float kLedgeHangOffsetX = 12.0F;   // separacion horizontal hacia afuera
inline constexpr float kLedgeHangFeetBelow = 60.0F; // los pies cuelgan por debajo de la esquina

enum class ActionState : std::uint8_t
{
    Wait,
    WalkSlow,
    WalkMid,
    WalkFast,
    Dash,
    Run,
    RunBrake,
    Turn,
    Squat,
    JumpSquat,
    JumpF,
    JumpB,
    JumpAerialF,
    JumpAerialB,
    Fall,
    FallAerial,
    Landing,
    LandingFallSpecial,
    AttackJab,
    AttackForwardTilt,
    AttackUpTilt,
    AttackDownTilt,
    AttackForwardSmash,
    AttackNair,
    AttackFair,
    AttackBair,
    AttackUair,
    AttackDair,
    SpecialUp,
    GuardOn,
    Guard,
    GuardOff,
    ShieldStun,
    EscapeAir,
    CliffCatch,
    CliffWait,
    CliffClimb,
    CliffJump,
    DamageHi,
    DamageLw,
    DamageFly,
    Respawn,
    FallSpecial
};

struct FighterAttributes
{
    float groundFriction = 1200.0F;
    float walkAccel = 880.0F;
    float walkMaxVelocity = 220.0F;
    float dashInitialVelocity = 310.0F;
    float dashRunAccel = 1400.0F;
    float runMaxVelocity = 440.0F;
    float airDriftAccel = 650.0F;
    float airMaxVelocity = 340.0F;
    float airFriction = 0.988F;
    float gravity = 1100.0F;
    float terminalVelocity = 660.0F;
    float fastFallVelocity = 840.0F;
    float weight = 100.0F;
    float shortHopVelocity = 450.0F;
    float fullHopVelocity = 580.0F;
    int jumpSquatFrames = 3;
    float initialShieldSize = 58.0F;
};

struct EnvironmentCollisionBox
{
    Vector2 top{};
    Vector2 bottom{};
    Vector2 left{};
    Vector2 right{};
};

struct FighterInput
{
    float stickX = 0.0F;
    float stickY = 0.0F;
    bool jumpDown = false;
    bool jumpPressed = false;
    bool jumpReleased = false;
    bool attackPressed = false;
    bool attackDown = false;
    bool specialPressed = false;
    bool shieldDown = false;
    bool airDodgePressed = false;
    bool dropThroughPressed = false;
    bool grabPressed = false;
    AttackKind attackKind = AttackKind::Jab;
};

struct Fighter
{
    FighterAttributes attributes{};
    Vector2 position{}; // Punto central inferior (PIES / vertice bottom del ECB) en coordenadas de mundo.
    Vector2 previousPosition{};
    Vector2 velocity{};
    FighterInput input{};
    EnvironmentCollisionBox ecb{};
    ActionState state = ActionState::Wait;
    AttackInstance attack{};
    int stateFrame = 0;
    int facingDir = 1;
    int playerIndex = 0;
    int stocks = 4;
    int hitlagFrames = 0;
    int hitstunFrames = 0;
    int invulnerabilityFrames = 0;
    int jumpSquatFrame = 0;
    int jumpBufferFrames = 0;
    int airJumps = 1;
    int ledgeId = -1;
    int ledgeInvulnerability = 0;
    int dropThroughFrames = 0;
    int ledgeRegrabTimer = 0;
    int ledgeGrabCount = 0;
    Vector2 ledgeCorner{};
    int landingLagFrames = 0;
    int shieldStunFrames = 0;
    int launchDirection = 1;
    float percent = 0.0F;
    float shieldHealth = 100.0F;
    float damageShake = 0.0F;
    float launchKnockback = 0.0F;
    float launchAngleDegrees = 0.0F;
    float diStickX = 0.0F;
    float diStickY = 0.0F;
    bool grounded = false;
    bool onPassThrough = false;
    bool fastFalling = false;
    bool shortHopRequested = false;
    bool shielding = false;
    bool cpu = false;
    bool cpuDiShield = false;
    bool alive = true;
};

const char* ActionStateName(ActionState state);
bool IsAerialState(ActionState state);
bool IsAttackingState(ActionState state);
bool IsDamageState(ActionState state);
EnvironmentCollisionBox BuildECB(const Fighter& fighter);
Rectangle GetHurtbox(const Fighter& fighter);
void SetActionState(Fighter& fighter, ActionState state);
void ResetFighter(Fighter& fighter, float x, float groundY, int facingDir, int playerIndex);
void UpdateFighter(Fighter& fighter, const FighterInput& input, float dt);
void IntegrateFighter(Fighter& fighter, float dt);
void ApplyDamageLaunch(Fighter& fighter, float damage, float knockback, float angleDegrees, int hitlagFrames, int hitstunFrames, int attackerFacing);
void DrawECB(const Fighter& fighter, Color color = YELLOW);
