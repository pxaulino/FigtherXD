#if __has_include("ft/fighter.hpp")
#include "ft/fighter.hpp"
#else
#include "fighter.hpp"
#endif

#include <algorithm>
#include <cmath>

namespace
{
constexpr float kFighterWidth = 58.0F;
constexpr float kStandingHeight = 110.0F;
constexpr float kLedgeJumpX = 140.0F;
constexpr float kLedgeJumpY = -520.0F;
constexpr float kLedgeClimbInset = 28.0F;
constexpr int kLedgeRegrabFrames = 30;
constexpr int kLedgeMinHangFrames = 10;
constexpr int kClimbFrames = 14;
constexpr int kFallSpecialLandingLag = 28;
constexpr float kAirDodgeDecay = 0.92F;

float ClampAxis(float axis)
{
    return std::clamp(axis, -1.0F, 1.0F);
}

bool IsAttackState(ActionState state)
{
    return state >= ActionState::AttackJab && state <= ActionState::SpecialUp;
}

bool IsLedgeState(ActionState state)
{
    return state == ActionState::CliffCatch || state == ActionState::CliffWait || state == ActionState::CliffClimb;
}

bool IsWalkableState(ActionState state)
{
    return state == ActionState::Wait || state == ActionState::WalkSlow || state == ActionState::WalkMid ||
           state == ActionState::WalkFast;
}

void ApplyGroundFriction(Fighter& fighter, float dt)
{
    const float step = fighter.attributes.groundFriction * dt;
    if (std::abs(fighter.velocity.x) <= step)
    {
        fighter.velocity.x = 0.0F;
    }
    else
    {
        fighter.velocity.x -= std::copysign(step, fighter.velocity.x);
    }
}

void EnterJumpSquat(Fighter& fighter)
{
    fighter.jumpBufferFrames = 0;
    fighter.jumpSquatFrame = 1;
    fighter.shortHopRequested = false;
    SetActionState(fighter, ActionState::JumpSquat);
    ++fighter.stateFrame;
}

void LeaveLedge(Fighter& fighter)
{
    fighter.ledgeId = -1;
    fighter.ledgeRegrabTimer = kLedgeRegrabFrames;
}

void StartJump(Fighter& fighter, bool shortHop)
{
    fighter.grounded = false;
    // El salto desde el suelo no consume el salto aereo (airJumps se conserva en 1)
    fighter.velocity.y = shortHop ? -fighter.attributes.shortHopVelocity : -fighter.attributes.fullHopVelocity;
    const float axis = ClampAxis(fighter.input.stickX);
    if (std::abs(axis) > 0.15F)
    {
        fighter.velocity.x = std::clamp(fighter.velocity.x + axis * 140.0F,
                                        -fighter.attributes.airMaxVelocity,
                                        fighter.attributes.airMaxVelocity);
    }
    SetActionState(fighter, fighter.velocity.x * static_cast<float>(fighter.facingDir) >= 0.0F ? ActionState::JumpF : ActionState::JumpB);
}
}

const char* ActionStateName(ActionState state)
{
    switch (state)
    {
    case ActionState::Wait: return "Wait";
    case ActionState::WalkSlow: return "WalkSlow";
    case ActionState::WalkMid: return "WalkMid";
    case ActionState::WalkFast: return "WalkFast";
    case ActionState::Dash: return "Dash";
    case ActionState::Run: return "Run";
    case ActionState::RunBrake: return "RunBrake";
    case ActionState::Turn: return "Turn";
    case ActionState::Squat: return "Squat";
    case ActionState::JumpSquat: return "JumpSquat";
    case ActionState::JumpF: return "JumpF";
    case ActionState::JumpB: return "JumpB";
    case ActionState::JumpAerialF: return "JumpAerialF";
    case ActionState::JumpAerialB: return "JumpAerialB";
    case ActionState::Fall: return "Fall";
    case ActionState::FallAerial: return "FallAerial";
    case ActionState::FallSpecial: return "FallSpecial";
    case ActionState::Landing: return "Landing";
    case ActionState::LandingFallSpecial: return "LandingFallSpecial";
    case ActionState::AttackJab: return "AttackJab";
    case ActionState::AttackForwardTilt: return "AttackForwardTilt";
    case ActionState::AttackUpTilt: return "AttackUpTilt";
    case ActionState::AttackDownTilt: return "AttackDownTilt";
    case ActionState::AttackForwardSmash: return "AttackForwardSmash";
    case ActionState::AttackNair: return "Nair";
    case ActionState::AttackFair: return "Fair";
    case ActionState::AttackBair: return "Bair";
    case ActionState::AttackUair: return "Uair";
    case ActionState::AttackDair: return "Dair";
    case ActionState::SpecialUp: return "SpecialUp";
    case ActionState::GuardOn: return "GuardOn";
    case ActionState::Guard: return "Guard";
    case ActionState::GuardOff: return "GuardOff";
    case ActionState::ShieldStun: return "ShieldStun";
    case ActionState::EscapeAir: return "EscapeAir";
    case ActionState::CliffCatch: return "CliffCatch";
    case ActionState::CliffWait: return "CliffWait";
    case ActionState::CliffClimb: return "CliffClimb";
    case ActionState::CliffJump: return "CliffJump";
    case ActionState::DamageHi: return "DamageHi";
    case ActionState::DamageLw: return "DamageLw";
    case ActionState::DamageFly: return "DamageFly";
    case ActionState::Respawn: return "Respawn";
    }
    return "Unknown";
}

bool IsAerialState(ActionState state)
{
    switch (state)
    {
    case ActionState::JumpF:
    case ActionState::JumpB:
    case ActionState::JumpAerialF:
    case ActionState::JumpAerialB:
    case ActionState::Fall:
    case ActionState::FallAerial:
    case ActionState::FallSpecial:
    case ActionState::EscapeAir:
    case ActionState::CliffJump:
    case ActionState::DamageFly:
    case ActionState::DamageHi:
    case ActionState::DamageLw:
    case ActionState::SpecialUp:
    case ActionState::AttackNair:
    case ActionState::AttackFair:
    case ActionState::AttackBair:
    case ActionState::AttackUair:
    case ActionState::AttackDair: return true;
    default: return false;
    }
}

bool IsAttackingState(ActionState state)
{
    return IsAttackState(state);
}

bool IsDamageState(ActionState state)
{
    return state == ActionState::DamageHi || state == ActionState::DamageLw || state == ActionState::DamageFly;
}

// Coordenadas del ECB relativas a position (los PIES): el vertice inferior es siempre (0, 0).
EnvironmentCollisionBox BuildECB(const Fighter& fighter)
{
    float height = kStandingHeight;
    float halfWidth = kFighterWidth * 0.5F;
    float sideFromTop = 0.55F;
    if (fighter.state == ActionState::Squat)
    {
        height *= 0.62F;
        halfWidth *= 1.08F;
    }
    else if (IsAerialState(fighter.state) || IsDamageState(fighter.state))
    {
        height *= 0.96F;
        sideFromTop = 0.48F;
    }
    else if (fighter.state == ActionState::CliffWait || fighter.state == ActionState::CliffCatch)
    {
        height *= 0.70F;
        halfWidth *= 0.82F;
    }
    const float sideY = -height * (1.0F - sideFromTop);
    return EnvironmentCollisionBox{
        Vector2{ 0.0F, -height },
        Vector2{ 0.0F, 0.0F },
        Vector2{ -halfWidth, sideY },
        Vector2{ halfWidth, sideY },
    };
}

Rectangle GetHurtbox(const Fighter& fighter)
{
    const EnvironmentCollisionBox ecb = BuildECB(fighter);
    const float height = std::max(20.0F, -ecb.top.y - 8.0F);
    return Rectangle{ fighter.position.x - 24.0F,
                      fighter.position.y - height - 2.0F,
                      48.0F,
                      height };
}

void SetActionState(Fighter& fighter, ActionState state)
{
    if (fighter.state != state)
    {
        if (IsAttackState(fighter.state) && !IsAttackState(state))
        {
            fighter.attack = {};
        }
        fighter.state = state;
        fighter.stateFrame = 0;
    }
}

void ResetFighter(Fighter& fighter, float x, float groundY, int facingDir, int playerIndex)
{
    const FighterAttributes attributes = fighter.attributes;
    fighter = Fighter{};
    fighter.attributes = attributes;
    fighter.position = Vector2{ x, groundY }; // position = pies
    fighter.facingDir = facingDir;
    fighter.playerIndex = playerIndex;
    fighter.stocks = 4;
    fighter.shieldHealth = 100.0F;
    fighter.grounded = true;
    fighter.airJumps = 1;
    fighter.state = ActionState::Wait;
    fighter.alive = true;
}

void UpdateFighter(Fighter& fighter, const FighterInput& input, float dt)
{
    if (!fighter.alive) return;
    fighter.input = input;
    fighter.ecb = BuildECB(fighter);
    fighter.invulnerabilityFrames = std::max(0, fighter.invulnerabilityFrames - 1);
    fighter.ledgeInvulnerability = std::max(0, fighter.ledgeInvulnerability - 1);
    fighter.ledgeRegrabTimer = std::max(0, fighter.ledgeRegrabTimer - 1);
    fighter.damageShake = std::max(0.0F, fighter.damageShake - dt);
    fighter.diStickX = ClampAxis(input.stickX);
    fighter.diStickY = ClampAxis(input.stickY);
    if (fighter.jumpBufferFrames > 0) --fighter.jumpBufferFrames;
    if (input.jumpPressed) fighter.jumpBufferFrames = 5;

    if (fighter.hitlagFrames > 0)
    {
        --fighter.hitlagFrames;
        if (fighter.hitlagFrames == 0 && IsDamageState(fighter.state))
        {
            ApplyDirectionalInfluence(fighter);
        }
        return;
    }

    const float axis = ClampAxis(input.stickX);

    // ------------------------------------------------------------------ Borde
    if (fighter.state == ActionState::CliffCatch)
    {
        fighter.velocity = Vector2{ 0.0F, 0.0F };
        if (fighter.stateFrame >= 4)
        {
            SetActionState(fighter, ActionState::CliffWait);
        }
        ++fighter.stateFrame;
        return;
    }
    if (fighter.state == ActionState::CliffWait)
    {
        fighter.velocity = Vector2{ 0.0F, 0.0F };
        const float facing = static_cast<float>(fighter.facingDir);
        const float toward = input.stickX * facing;
        if (fighter.stateFrame >= kLedgeMinHangFrames)
        {
            if (input.jumpPressed)
            {
                LeaveLedge(fighter);
                fighter.grounded = false;
                fighter.airJumps = 1;
                fighter.velocity = Vector2{ facing * kLedgeJumpX, kLedgeJumpY };
                SetActionState(fighter, ActionState::CliffJump);
            }
            else if (input.stickY < -0.45F || toward > 0.45F || input.grabPressed || input.attackPressed)
            {
                // Subir al escenario
                SetActionState(fighter, ActionState::CliffClimb);
                fighter.invulnerabilityFrames = std::max(fighter.invulnerabilityFrames, 18);
            }
            else if (input.stickY > 0.55F || toward < -0.45F)
            {
                // Soltarse del borde (abajo o hacia atras)
                LeaveLedge(fighter);
                fighter.grounded = false;
                fighter.ledgeInvulnerability = 0;
                fighter.velocity = Vector2{ -facing * 30.0F, 60.0F };
                SetActionState(fighter, ActionState::Fall);
            }
        }
        ++fighter.stateFrame;
        return;
    }
    if (fighter.state == ActionState::CliffClimb)
    {
        const float facing = static_cast<float>(fighter.facingDir);
        const float startX = fighter.ledgeCorner.x - facing * kLedgeHangOffsetX;
        const float startY = fighter.ledgeCorner.y + kLedgeHangFeetBelow;
        const float targetX = fighter.ledgeCorner.x + facing * kLedgeClimbInset;
        const float targetY = fighter.ledgeCorner.y;
        const float t = std::clamp(static_cast<float>(fighter.stateFrame) / static_cast<float>(kClimbFrames), 0.0F, 1.0F);
        if (t < 0.6F)
        {
            // Primero sube en vertical hasta la superficie...
            fighter.position.x = startX;
            fighter.position.y = startY + (targetY - startY) * (t / 0.6F);
        }
        else
        {
            // ...y luego avanza hacia adentro del escenario
            fighter.position.y = targetY;
            fighter.position.x = startX + (targetX - startX) * ((t - 0.6F) / 0.4F);
        }
        fighter.velocity = Vector2{ 0.0F, 0.0F };
        if (fighter.stateFrame >= kClimbFrames)
        {
            LeaveLedge(fighter);
            fighter.position = Vector2{ targetX, targetY };
            fighter.grounded = true;
            fighter.landingLagFrames = 0;
            SetActionState(fighter, ActionState::Wait);
        }
        ++fighter.stateFrame;
        return;
    }
    if (fighter.state == ActionState::CliffJump && fighter.stateFrame > 4)
    {
        SetActionState(fighter, ActionState::JumpAerialF);
    }

    // ---------------------------------------------------------------- Respawn
    if (fighter.state == ActionState::Respawn)
    {
        if (fighter.invulnerabilityFrames <= 0) SetActionState(fighter, ActionState::Wait);
        return;
    }

    if (fighter.shieldStunFrames > 0)
    {
        --fighter.shieldStunFrames;
        SetActionState(fighter, ActionState::ShieldStun);
        if (fighter.shieldStunFrames == 0) SetActionState(fighter, ActionState::Guard);
        ++fighter.stateFrame;
        return;
    }

    if (IsDamageState(fighter.state))
    {
        if (fighter.hitstunFrames > 0) --fighter.hitstunFrames;
        else
        {
            fighter.launchKnockback = 0.0F;
            SetActionState(fighter, fighter.grounded ? ActionState::Wait : ActionState::Fall);
        }
        ++fighter.stateFrame;
        return;
    }

    // ----------------------------------------------- Air dodge (movimiento X e Y)
    if (fighter.state == ActionState::EscapeAir)
    {
        if (fighter.stateFrame >= 18)
        {
            SetActionState(fighter, ActionState::Fall);
        }
        else
        {
            fighter.velocity.x *= kAirDodgeDecay;
            fighter.velocity.y *= kAirDodgeDecay;
            ++fighter.stateFrame;
            return;
        }
    }

    if (fighter.state == ActionState::LandingFallSpecial || fighter.state == ActionState::Landing)
    {
        // Friccion de suelo: aqui se desliza el Wavedash / Waveland
        ApplyGroundFriction(fighter, dt);
        if (fighter.landingLagFrames > 0) --fighter.landingLagFrames;
        if (fighter.landingLagFrames <= 0)
        {
            if (fighter.jumpBufferFrames > 0)
            {
                EnterJumpSquat(fighter);
            }
            else
            {
                SetActionState(fighter, ActionState::Wait);
            }
        }
        ++fighter.stateFrame;
        return;
    }

    // JumpSquat se evalua ANTES que el escudo: pulsar escudo no cancela el salto
    if (fighter.state == ActionState::JumpSquat)
    {
        ++fighter.jumpSquatFrame;
        if (input.jumpReleased || !input.jumpDown)
        {
            fighter.shortHopRequested = true;
        }
        if (fighter.jumpSquatFrame >= fighter.attributes.jumpSquatFrames)
        {
            StartJump(fighter, fighter.shortHopRequested);
        }
        ++fighter.stateFrame;
        return; // Impide que la logica de suelo anule el salto
    }

    // -------------------------------------------- Salto desde escudo (Jump OOS)
    if (fighter.grounded && (fighter.state == ActionState::Guard || fighter.state == ActionState::GuardOn) &&
        (input.jumpPressed || fighter.jumpBufferFrames > 0))
    {
        fighter.shielding = false;
        EnterJumpSquat(fighter);
        return;
    }

    if (input.shieldDown && fighter.grounded && !IsAttackingState(fighter.state))
    {
        fighter.shielding = true;
        ApplyGroundFriction(fighter, dt);
        if (fighter.state != ActionState::Guard && fighter.state != ActionState::GuardOn)
        {
            SetActionState(fighter, ActionState::GuardOn);
        }
        else if (fighter.state == ActionState::GuardOn && fighter.stateFrame >= 3)
        {
            SetActionState(fighter, ActionState::Guard);
        }
        fighter.shieldHealth = std::min(100.0F, fighter.shieldHealth + 0.08F);
        ++fighter.stateFrame;
        return;
    }
    if (fighter.shielding)
    {
        fighter.shielding = false;
        SetActionState(fighter, ActionState::GuardOff);
        fighter.stateFrame = 0;
        ++fighter.stateFrame;
        return;
    }
    if (fighter.state == ActionState::GuardOff)
    {
        ApplyGroundFriction(fighter, dt);
        if (fighter.stateFrame >= 5) SetActionState(fighter, ActionState::Wait);
        ++fighter.stateFrame;
        return;
    }

    if (fighter.shieldHealth < 100.0F)
    {
        fighter.shieldHealth = std::min(100.0F, fighter.shieldHealth + 0.025F);
    }

    const bool helpless = fighter.state == ActionState::FallSpecial;

    if (input.airDodgePressed && !fighter.grounded && !helpless && !IsAttackingState(fighter.state) &&
        fighter.state != ActionState::EscapeAir)
    {
        Vector2 direction{ input.stickX, input.stickY };
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0.15F)
        {
            direction.x /= length;
            direction.y /= length;
        }
        else
        {
            direction = Vector2{ 0.0F, 0.0F };
        }
        fighter.velocity = Vector2{ direction.x * 480.0F, direction.y * 480.0F };
        fighter.landingLagFrames = 10;
        fighter.invulnerabilityFrames = 10;
        fighter.fastFalling = false;
        SetActionState(fighter, ActionState::EscapeAir);
        ++fighter.stateFrame;
        return;
    }

    // Drop-through: solo sobre plataformas semiatravesables, nunca en el suelo solido
    if (input.dropThroughPressed && fighter.grounded && fighter.onPassThrough)
    {
        fighter.dropThroughFrames = 12;
        fighter.grounded = false;
        fighter.onPassThrough = false;
        fighter.velocity.y = 75.0F;
        SetActionState(fighter, ActionState::Fall);
    }

    // ---------------------------------------------------------------- Ataques
    if (input.specialPressed && !fighter.grounded && fighter.state != ActionState::SpecialUp && !helpless)
    {
        fighter.velocity.y = -720.0F;
        fighter.grounded = false;
        fighter.landingLagFrames = kFallSpecialLandingLag;
        StartAttack(fighter, AttackKind::UpSpecial);
    }
    else if (input.attackPressed && !IsAttackingState(fighter.state) && !helpless)
    {
        if (fighter.grounded && std::abs(axis) > 0.3F &&
            (input.attackKind == AttackKind::ForwardTilt || input.attackKind == AttackKind::ForwardSmash))
        {
            fighter.facingDir = axis > 0.0F ? 1 : -1;
        }
        StartAttack(fighter, input.attackKind);
        fighter.attack.chargeFrames = 0;
    }

    if (IsAttackingState(fighter.state))
    {
        const bool wasSpecialUp = fighter.state == ActionState::SpecialUp;
        if (fighter.attack.charging && (!input.attackDown || fighter.attack.chargeFrames >= 60))
        {
            fighter.attack.charging = false;
            fighter.attack.frame = 0;
        }
        TickAttack(fighter);
        if (fighter.attack.kind == AttackKind::None)
        {
            if (fighter.grounded)
            {
                SetActionState(fighter, ActionState::Wait);
            }
            else if (wasSpecialUp)
            {
                // Caida indefensa tras Up-B en el aire
                fighter.landingLagFrames = kFallSpecialLandingLag;
                SetActionState(fighter, ActionState::FallSpecial);
            }
            else
            {
                SetActionState(fighter, ActionState::Fall);
            }
        }
        else
        {
            if (fighter.grounded)
            {
                ApplyGroundFriction(fighter, dt);
            }
            else if (std::abs(axis) > 0.12F)
            {
                fighter.velocity.x = std::clamp(fighter.velocity.x + axis * fighter.attributes.airDriftAccel * dt,
                                                -fighter.attributes.airMaxVelocity,
                                                fighter.attributes.airMaxVelocity);
            }
            ++fighter.stateFrame;
            return;
        }
    }

    // ------------------------------------------------------------------ Saltos
    if (input.jumpPressed || (fighter.grounded && fighter.jumpBufferFrames > 0))
    {
        if (fighter.grounded && fighter.state != ActionState::JumpSquat)
        {
            EnterJumpSquat(fighter);
            return;
        }
        else if (!fighter.grounded && fighter.airJumps > 0 && input.jumpPressed && !helpless)
        {
            --fighter.airJumps;
            fighter.jumpBufferFrames = 0;
            fighter.fastFalling = false;
            fighter.velocity.y = -fighter.attributes.fullHopVelocity * 0.95F;
            const float jumpAxis = ClampAxis(input.stickX);
            if (std::abs(jumpAxis) > 0.15F)
            {
                fighter.velocity.x = std::clamp(fighter.velocity.x + jumpAxis * 120.0F,
                                                -fighter.attributes.airMaxVelocity,
                                                fighter.attributes.airMaxVelocity);
            }
            SetActionState(fighter, input.stickX * static_cast<float>(fighter.facingDir) < 0.0F ? ActionState::JumpAerialB : ActionState::JumpAerialF);
        }
    }

    // ------------------------------------------------------------- Movimiento
    if (fighter.grounded)
    {
        // Dash Dancing: cambio instantaneo de direccion durante Dash
        if (fighter.state == ActionState::Dash && axis * static_cast<float>(fighter.facingDir) < -0.3F)
        {
            fighter.facingDir = axis < 0.0F ? -1 : 1;
            fighter.velocity.x = static_cast<float>(fighter.facingDir) * fighter.attributes.dashInitialVelocity;
            fighter.stateFrame = 0;
            SetActionState(fighter, ActionState::Dash);
        }
        else if (std::abs(axis) > 0.72F && IsWalkableState(fighter.state))
        {
            fighter.facingDir = axis > 0.0F ? 1 : -1;
            fighter.velocity.x = static_cast<float>(fighter.facingDir) * fighter.attributes.dashInitialVelocity;
            fighter.stateFrame = 0;
            SetActionState(fighter, ActionState::Dash);
        }

        if (std::abs(axis) > 0.12F)
        {
            if (fighter.state == ActionState::Dash)
            {
                if (fighter.stateFrame >= 14)
                {
                    SetActionState(fighter, ActionState::Run);
                }
                else
                {
                    fighter.velocity.x = static_cast<float>(fighter.facingDir) *
                        std::max(fighter.attributes.dashInitialVelocity, std::abs(fighter.velocity.x));
                }
            }
            else if (fighter.state == ActionState::Run)
            {
                if (axis * static_cast<float>(fighter.facingDir) < -0.35F)
                {
                    SetActionState(fighter, ActionState::Turn);
                }
                else
                {
                    fighter.velocity.x += axis * fighter.attributes.dashRunAccel * dt;
                    fighter.velocity.x = std::clamp(fighter.velocity.x, -fighter.attributes.runMaxVelocity, fighter.attributes.runMaxVelocity);
                }
            }
            else if (fighter.state != ActionState::Turn)
            {
                fighter.velocity.x += axis * fighter.attributes.walkAccel * dt;
                const float maxWalk = fighter.attributes.walkMaxVelocity;
                fighter.velocity.x = std::clamp(fighter.velocity.x, -maxWalk, maxWalk);
                fighter.facingDir = axis > 0.0F ? 1 : -1;
                const float absVel = std::abs(fighter.velocity.x);
                if (absVel > maxWalk * 0.65F)
                {
                    SetActionState(fighter, ActionState::WalkFast);
                }
                else if (absVel > maxWalk * 0.3F)
                {
                    SetActionState(fighter, ActionState::WalkMid);
                }
                else
                {
                    SetActionState(fighter, ActionState::WalkSlow);
                }
            }
        }
        else
        {
            const float frictionStep = fighter.attributes.groundFriction * dt;
            if (std::abs(fighter.velocity.x) <= frictionStep)
            {
                fighter.velocity.x = 0.0F;
                if (fighter.state != ActionState::LandingFallSpecial && fighter.state != ActionState::Landing &&
                    fighter.state != ActionState::Squat)
                {
                    SetActionState(fighter, ActionState::Wait);
                }
            }
            else
            {
                fighter.velocity.x -= std::copysign(frictionStep, fighter.velocity.x);
                if (fighter.state == ActionState::Run || fighter.state == ActionState::Dash)
                {
                    SetActionState(fighter, ActionState::RunBrake);
                }
            }
        }

        if (input.stickY > 0.55F && IsWalkableState(fighter.state))
        {
            SetActionState(fighter, ActionState::Squat);
        }
        else if (fighter.state == ActionState::Squat && input.stickY <= 0.55F)
        {
            SetActionState(fighter, ActionState::Wait);
        }
    }
    else
    {
        const float driftAccel = fighter.attributes.airDriftAccel * (helpless ? 0.35F : 1.0F);
        const float maxAir = fighter.attributes.airMaxVelocity * (helpless ? 0.55F : 1.0F);
        if (std::abs(axis) > 0.12F)
        {
            fighter.velocity.x += axis * driftAccel * dt;
        }
        else
        {
            fighter.velocity.x *= std::pow(fighter.attributes.airFriction, dt * 60.0F);
        }
        fighter.velocity.x = std::clamp(fighter.velocity.x, -maxAir, maxAir);
        if (!helpless && fighter.velocity.y >= 0.0F && input.stickY > 0.55F && !fighter.fastFalling)
        {
            fighter.velocity.y = fighter.attributes.fastFallVelocity;
            fighter.fastFalling = true;
        }
        if (fighter.state == ActionState::JumpF || fighter.state == ActionState::JumpB ||
            fighter.state == ActionState::JumpAerialF || fighter.state == ActionState::JumpAerialB ||
            fighter.state == ActionState::CliffJump)
        {
            if (fighter.velocity.y >= 0.0F) SetActionState(fighter, ActionState::Fall);
        }
        else if (!IsAttackingState(fighter.state) && fighter.state != ActionState::EscapeAir && !helpless)
        {
            SetActionState(fighter, ActionState::FallAerial);
        }
    }

    if (fighter.state == ActionState::Turn && fighter.stateFrame >= 4)
    {
        fighter.facingDir = -fighter.facingDir;
        if (std::abs(axis) > 0.12F)
        {
            SetActionState(fighter, ActionState::Dash);
        }
        else
        {
            SetActionState(fighter, ActionState::Wait);
        }
    }

    ++fighter.stateFrame;
}

void IntegrateFighter(Fighter& fighter, float dt)
{
    // El hitlag congela por completo al luchador; en el borde la posicion la controla la maquina de estados
    if (!fighter.alive || fighter.hitlagFrames > 0 || IsLedgeState(fighter.state))
    {
        return;
    }
    if (fighter.state == ActionState::EscapeAir)
    {
        // Air dodge: movimiento libre en X e Y, sin gravedad
        fighter.position.x += fighter.velocity.x * dt;
        fighter.position.y += fighter.velocity.y * dt;
        return;
    }
    if (fighter.grounded)
    {
        fighter.position.x += fighter.velocity.x * dt;
        return;
    }

    fighter.velocity.y = std::min(fighter.velocity.y + fighter.attributes.gravity * dt,
                                  fighter.attributes.terminalVelocity);
    fighter.position.x += fighter.velocity.x * dt;
    fighter.position.y += fighter.velocity.y * dt;
}

void ApplyDamageLaunch(Fighter& fighter, float, float knockback, float angleDegrees,
                       int hitlagFrames, int hitstunFrames, int attackerFacing)
{
    fighter.launchKnockback = std::max(knockback, 0.0F);
    fighter.launchAngleDegrees = angleDegrees;
    fighter.launchDirection = attackerFacing >= 0 ? 1 : -1;
    fighter.hitlagFrames = std::max(fighter.hitlagFrames, hitlagFrames);
    fighter.hitstunFrames = std::max(fighter.hitstunFrames, hitstunFrames);
    fighter.damageShake = 0.18F;
    fighter.grounded = false;
    fighter.fastFalling = false;
    const float radians = angleDegrees * 3.14159265358979323846F / 180.0F;
    fighter.velocity = Vector2{ std::cos(radians) * knockback * static_cast<float>(fighter.launchDirection),
                                -std::sin(radians) * knockback };
    SetActionState(fighter, angleDegrees > 65.0F && angleDegrees < 115.0F ? ActionState::DamageHi :
                   angleDegrees > 245.0F && angleDegrees < 295.0F ? ActionState::DamageLw : ActionState::DamageFly);
}

void DrawECB(const Fighter& fighter, Color color)
{
    const EnvironmentCollisionBox ecb = BuildECB(fighter);
    const Vector2 top{ fighter.position.x + ecb.top.x, fighter.position.y + ecb.top.y };
    const Vector2 bottom{ fighter.position.x + ecb.bottom.x, fighter.position.y + ecb.bottom.y };
    const Vector2 left{ fighter.position.x + ecb.left.x, fighter.position.y + ecb.left.y };
    const Vector2 right{ fighter.position.x + ecb.right.x, fighter.position.y + ecb.right.y };
    DrawLineV(top, right, color);
    DrawLineV(right, bottom, color);
    DrawLineV(bottom, left, color);
    DrawLineV(left, top, color);
    DrawCircleV(bottom, 3.0F, ORANGE);
}
