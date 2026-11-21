#if __has_include("lb/combat.hpp")
#include "lb/combat.hpp"
#else
#include "combat.hpp"
#endif

#include <algorithm>
#include <cmath>

#if __has_include("ft/fighter.hpp")
#include "ft/fighter.hpp"
#else
#include "../ft/fighter.hpp"
#endif

namespace
{
constexpr float kPi = 3.14159265358979323846F;

const std::array<AttackDefinition, static_cast<std::size_t>(AttackKind::Count)> kAttacks = {{
    AttackDefinition{ "None", {}, 0, 1, 1, 1, false, false },
    AttackDefinition{ "Jab", {{{ 0, 1, 25.0F, { 34.0F, -55.0F }, 3.0F, 35.0F, 22.0F, 100.0F, HitEffect::Normal },
                               { 1, 0, 19.0F, { 52.0F, -57.0F }, 3.0F, 35.0F, 22.0F, 100.0F, HitEffect::Normal }}}, 2, 3, 3, 11, false, false },
    AttackDefinition{ "Forward Tilt", {{{ 0, 1, 28.0F, { 40.0F, -55.0F }, 7.0F, 32.0F, 25.0F, 92.0F, HitEffect::Normal },
                                        { 1, 0, 23.0F, { 67.0F, -52.0F }, 7.0F, 32.0F, 25.0F, 92.0F, HitEffect::Normal }}}, 2, 5, 3, 15, false, false },
    AttackDefinition{ "Up Tilt", {{{ 0, 1, 27.0F, { 0.0F, -91.0F }, 8.0F, 90.0F, 24.0F, 96.0F, HitEffect::Normal },
                                   { 1, 0, 24.0F, { 20.0F, -80.0F }, 8.0F, 90.0F, 24.0F, 96.0F, HitEffect::Normal }}}, 2, 6, 4, 16, false, false },
    AttackDefinition{ "Down Tilt", {{{ 0, 1, 26.0F, { 38.0F, -20.0F }, 6.0F, 25.0F, 20.0F, 85.0F, HitEffect::Normal },
                                     { 1, 0, 22.0F, { 61.0F, -22.0F }, 6.0F, 25.0F, 20.0F, 85.0F, HitEffect::Normal }}}, 2, 5, 3, 13, false, false },
    AttackDefinition{ "Forward Smash", {{{ 0, 1, 34.0F, { 53.0F, -56.0F }, 16.0F, 38.0F, 24.0F, 112.0F, HitEffect::Slash },
                                         { 1, 0, 30.0F, { 85.0F, -53.0F }, 16.0F, 38.0F, 24.0F, 112.0F, HitEffect::Slash }}}, 2, 12, 4, 23, false, true },
    AttackDefinition{ "Neutral Air", {{{ 0, 1, 34.0F, { 0.0F, -50.0F }, 8.0F, 50.0F, 20.0F, 88.0F, HitEffect::Normal },
                                       { 1, 0, 31.0F, { 4.0F, -82.0F }, 8.0F, 50.0F, 20.0F, 88.0F, HitEffect::Normal }}}, 2, 4, 5, 23, true, false },
    AttackDefinition{ "Forward Air", {{{ 0, 1, 32.0F, { 40.0F, -53.0F }, 10.0F, 38.0F, 25.0F, 100.0F, HitEffect::Slash },
                                       { 1, 0, 26.0F, { 68.0F, -52.0F }, 10.0F, 38.0F, 25.0F, 100.0F, HitEffect::Slash }}}, 2, 7, 4, 23, true, false },
    AttackDefinition{ "Back Air", {{{ 0, 1, 32.0F, { -38.0F, -53.0F }, 11.0F, 32.0F, 24.0F, 102.0F, HitEffect::Slash },
                                    { 1, 0, 27.0F, { -67.0F, -50.0F }, 11.0F, 32.0F, 24.0F, 102.0F, HitEffect::Slash }}}, 2, 5, 4, 21, true, false },
    AttackDefinition{ "Up Air", {{{ 0, 1, 32.0F, { 0.0F, -100.0F }, 9.0F, 88.0F, 22.0F, 94.0F, HitEffect::Normal },
                                  { 1, 0, 26.0F, { 16.0F, -85.0F }, 9.0F, 88.0F, 22.0F, 94.0F, HitEffect::Normal }}}, 2, 6, 4, 22, true, false },
    AttackDefinition{ "Down Air", {{{ 0, 1, 33.0F, { 0.0F, -4.0F }, 11.0F, 270.0F, 28.0F, 98.0F, HitEffect::Electric },
                                    { 1, 0, 26.0F, { 12.0F, -28.0F }, 11.0F, 270.0F, 28.0F, 98.0F, HitEffect::Electric }}}, 2, 8, 5, 24, true, false },
    AttackDefinition{ "Up Special", {{{ 0, 1, 36.0F, { 0.0F, -55.0F }, 5.0F, 84.0F, 40.0F, 70.0F, HitEffect::Electric },
                                      { 1, 0, 29.0F, { 0.0F, -89.0F }, 5.0F, 84.0F, 40.0F, 70.0F, HitEffect::Electric }}}, 2, 5, 5, 27, true, false },
}};

Vector2 Center(const Fighter& fighter, const HitboxSpec& spec)
{
    const float x = spec.offset.x * static_cast<float>(fighter.facingDir);
    // position = pies; el centro del cuerpo queda 55 px por encima
    return Vector2{ fighter.position.x + x, fighter.position.y - 55.0F + spec.offset.y };
}

bool SegmentHitsRect(Vector2 from, Vector2 to, Rectangle rect, float radius)
{
    rect.x -= radius;
    rect.y -= radius;
    rect.width += radius * 2.0F;
    rect.height += radius * 2.0F;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    float t0 = 0.0F;
    float t1 = 1.0F;
    const auto clip = [&t0, &t1](float p, float q) {
        if (std::abs(p) < 0.00001F)
        {
            return q >= 0.0F;
        }
        const float ratio = q / p;
        if (p < 0.0F)
        {
            if (ratio > t1) return false;
            t0 = std::max(t0, ratio);
        }
        else
        {
            if (ratio < t0) return false;
            t1 = std::min(t1, ratio);
        }
        return true;
    };
    return clip(-dx, from.x - rect.x) && clip(dx, rect.x + rect.width - from.x) &&
           clip(-dy, from.y - rect.y) && clip(dy, rect.y + rect.height - from.y);
}

bool HitboxTouchesShield(const Fighter& defender, const HitboxSpec& spec, const Fighter& attacker)
{
    const Vector2 center = Center(attacker, spec);
    const Vector2 shieldCenter{ defender.position.x, defender.position.y - 53.0F };
    const float ratio = std::clamp(defender.shieldHealth / 100.0F, 0.28F, 1.0F);
    const float radius = defender.attributes.initialShieldSize * ratio;
    const float dx = center.x - shieldCenter.x;
    const float dy = center.y - shieldCenter.y;
    const float combined = radius + spec.radius;
    return dx * dx + dy * dy <= combined * combined;
}

void ApplyHit(Fighter& attacker, Fighter& defender, const HitboxSpec& hit)
{
    const bool hangingInvulnerable = (defender.state == ActionState::CliffWait || defender.state == ActionState::CliffCatch) &&
                                     defender.ledgeInvulnerability > 0;
    if (defender.invulnerabilityFrames > 0 || hangingInvulnerable)
    {
        return;
    }
    if (defender.shielding)
    {
        if (!HitboxTouchesShield(defender, hit, attacker)) return;
        const float chargedDamage = hit.damage * (1.0F + std::min(attacker.attack.chargeFrames, 60) * 0.0125F);
        defender.shieldHealth = std::max(0.0F, defender.shieldHealth - chargedDamage * 0.82F);
        defender.shieldStunFrames = std::max(2, static_cast<int>(std::ceil(chargedDamage * 0.85F)));
        SetActionState(defender, ActionState::ShieldStun);
        defender.hitlagFrames = std::max(defender.hitlagFrames, CalculateHitlag(chargedDamage));
        attacker.hitlagFrames = std::max(attacker.hitlagFrames, CalculateHitlag(chargedDamage));
        if (defender.shieldHealth <= 0.0F)
        {
            defender.shieldStunFrames = 48;
            defender.shielding = false;
        }
        return;
    }

    const float damage = hit.damage * (1.0F + std::min(attacker.attack.chargeFrames, 60) * 0.0125F);
    defender.percent += damage;
    const float knockback = CalculateMeleeKnockback(defender.percent, damage,
                                                    defender.attributes.weight, hit.knockbackGrowth,
                                                    hit.baseKnockback);
    const int hitlag = CalculateHitlag(damage);
    const int hitstun = CalculateHitstun(knockback);
    ApplyDamageLaunch(defender, hit.damage, knockback, hit.angleDegrees, hitlag, hitstun, attacker.facingDir);
    attacker.hitlagFrames = std::max(attacker.hitlagFrames, hitlag);
    attacker.damageShake = 0.12F;
}
}

const AttackDefinition& GetAttackDefinition(AttackKind kind)
{
    const std::size_t index = static_cast<std::size_t>(kind);
    return kAttacks[index < kAttacks.size() ? index : 0];
}

void StartAttack(Fighter& fighter, AttackKind kind)
{
    if (kind == AttackKind::None || kind >= AttackKind::Count) return;
    fighter.attack = {};
    fighter.attack.kind = kind;
    fighter.attack.frame = 0;
    fighter.attack.charging = GetAttackDefinition(kind).chargeable;
    SetActionState(fighter,
                   kind == AttackKind::Jab ? ActionState::AttackJab :
                   kind == AttackKind::ForwardTilt ? ActionState::AttackForwardTilt :
                   kind == AttackKind::UpTilt ? ActionState::AttackUpTilt :
                   kind == AttackKind::DownTilt ? ActionState::AttackDownTilt :
                   kind == AttackKind::ForwardSmash ? ActionState::AttackForwardSmash :
                   kind == AttackKind::Nair ? ActionState::AttackNair :
                   kind == AttackKind::Fair ? ActionState::AttackFair :
                   kind == AttackKind::Bair ? ActionState::AttackBair :
                   kind == AttackKind::UpAir ? ActionState::AttackUair :
                   kind == AttackKind::DownAir ? ActionState::AttackDair : ActionState::SpecialUp);
}

void TickAttack(Fighter& fighter)
{
    if (fighter.attack.kind != AttackKind::None)
    {
        if (fighter.attack.charging)
        {
            fighter.attack.chargeFrames = std::min(60, fighter.attack.chargeFrames + 1);
            return;
        }
        ++fighter.attack.frame;
        const AttackDefinition& definition = GetAttackDefinition(fighter.attack.kind);
        if (fighter.attack.frame >= definition.startupFrames + definition.activeFrames + definition.recoveryFrames)
        {
            fighter.attack = {};
        }
    }
}

float CalculateMeleeKnockback(float percent, float damage, float weight, float knockbackGrowth, float baseKnockback)
{
    const float safeWeight = std::max(0.0F, weight);
    const float weightTerm = 200.0F / std::max(safeWeight + 100.0F, 1.0F);
    const float inner = (percent / 10.0F + (percent * damage) / 20.0F) * weightTerm * 1.4F + 18.0F;
    const float result = inner * (knockbackGrowth / 100.0F) + baseKnockback;
    return std::isfinite(result) ? std::max(0.0F, result) : 0.0F;
}

int CalculateHitlag(float damage)
{
    return std::clamp(static_cast<int>(std::floor(std::max(0.0F, damage) * 0.33F + 3.0F)), 0, 60);
}

int CalculateHitstun(float knockback)
{
    return std::clamp(static_cast<int>(std::floor(std::max(0.0F, knockback) * 0.4F)), 0, 600);
}

void ApplyDirectionalInfluence(Fighter& fighter)
{
    if (fighter.launchKnockback <= 0.0F) return;
    const float launchRadians = fighter.launchAngleDegrees * kPi / 180.0F;
    const float nx = std::cos(launchRadians) * static_cast<float>(fighter.launchDirection);
    const float ny = -std::sin(launchRadians);
    const float perpendicularInput = fighter.diStickX * (-ny) + fighter.diStickY * nx;
    const float adjustment = std::clamp(perpendicularInput * 18.0F, -18.0F, 18.0F);
    fighter.launchAngleDegrees = std::clamp(fighter.launchAngleDegrees + adjustment, 0.0F, 360.0F);
    const float angle = fighter.launchAngleDegrees * kPi / 180.0F;
    fighter.velocity = Vector2{ std::cos(angle) * fighter.launchKnockback * static_cast<float>(fighter.launchDirection),
                                -std::sin(angle) * fighter.launchKnockback };
}

bool IsAttackActive(const Fighter& fighter)
{
    if (fighter.attack.kind == AttackKind::None || fighter.attack.hasConnected) return false;
    const AttackDefinition& definition = GetAttackDefinition(fighter.attack.kind);
    return fighter.attack.frame >= definition.startupFrames &&
           fighter.attack.frame < definition.startupFrames + definition.activeFrames;
}

void ResolveCombat(Fighter& first, Fighter& second)
{
    Fighter* attackers[] = { &first, &second };
    Fighter* defenders[] = { &second, &first };
    for (std::size_t player = 0; player < 2; ++player)
    {
        Fighter& attacker = *attackers[player];
        Fighter& defender = *defenders[player];
        if (!IsAttackActive(attacker)) continue;
        const AttackDefinition& attack = GetAttackDefinition(attacker.attack.kind);

        std::array<std::size_t, 2> order{ 0, 1 };
        if (attack.hitboxCount == 2 && attack.hitboxes[1].priority > attack.hitboxes[0].priority)
        {
            std::swap(order[0], order[1]);
        }
        for (std::size_t slot = 0; slot < static_cast<std::size_t>(attack.hitboxCount); ++slot)
        {
            const std::size_t index = order[slot];
            const HitboxSpec& hit = attack.hitboxes[index];
            const Vector2 current = Center(attacker, hit);
            if (defender.cpu && defender.cpuDiShield && IsAttackActive(attacker))
            {
                // Defensive dummy deliberately exposes shield/DI behavior when toggled.
                defender.diStickX = defender.position.x < attacker.position.x ? -1.0F : 1.0F;
                defender.shielding = true;
            }
            if (defender.shielding && HitboxTouchesShield(defender, hit, attacker))
            {
                ApplyHit(attacker, defender, hit);
                attacker.attack.previousCenters[index] = current;
                attacker.attack.hasPrevious[index] = true;
                attacker.attack.hasConnected = true;
                break;
            }
            if (SegmentHitsRect(attacker.attack.hasPrevious[index] ? attacker.attack.previousCenters[index] : current,
                                current, GetHurtbox(defender), hit.radius))
            {
                ApplyHit(attacker, defender, hit);
                attacker.attack.hasConnected = true;
                attacker.attack.previousCenters[index] = current;
                attacker.attack.hasPrevious[index] = true;
                break;
            }
            attacker.attack.previousCenters[index] = current;
            attacker.attack.hasPrevious[index] = true;
        }
    }
}

void DrawAttackHitboxes(const Fighter& fighter)
{
    if (!IsAttackActive(fighter)) return;
    const AttackDefinition& attack = GetAttackDefinition(fighter.attack.kind);
    for (int i = 0; i < attack.hitboxCount; ++i)
    {
        const HitboxSpec& hit = attack.hitboxes[static_cast<std::size_t>(i)];
        const Vector2 center = Center(fighter, hit);
        DrawCircleV(center, hit.radius, ColorAlpha(RED, 0.18F));
        DrawCircleLinesV(center, hit.radius, RED);
        DrawText(TextFormat("HB%d", hit.id), static_cast<int>(center.x), static_cast<int>(center.y), 12, RED);
    }
}
