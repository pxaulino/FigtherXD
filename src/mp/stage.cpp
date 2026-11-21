#if __has_include("mp/stage.hpp")
#include "mp/stage.hpp"
#else
#include "stage.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>

Stage CreateTrainingStage()
{
    Stage stage{};
    stage.platforms = {
        // Tarima superior continua (sin huecos vacíos para que el salto y movilidad sean fluidos y pro)
        StagePlatform{ 270.0F, 690.0F, 350.0F, true },
        // Suelo principal sólido del escenario
        StagePlatform{ 170.0F, 790.0F, stage.groundY, false },
    };
    return stage;
}

CollisionResult ResolveStageCollision(Stage& stage, Fighter& fighter, Vector2 previousPosition)
{
    if (!fighter.alive)
    {
        return CollisionResult::None;
    }

    // position = PIES del luchador (vertice inferior del ECB)
    const bool wasGrounded = fighter.grounded;
    const float previousFeet = previousPosition.y;
    const float currentFeet = fighter.position.y;
    const bool onLedge = fighter.state == ActionState::CliffCatch || fighter.state == ActionState::CliffWait ||
                         fighter.state == ActionState::CliffClimb;
    CollisionResult result = CollisionResult::None;
    fighter.grounded = false;
    fighter.onPassThrough = false;

    if (!onLedge && fighter.velocity.y >= 0.0F)
    {
        for (const StagePlatform& platform : stage.platforms)
        {
            // Solo las plataformas semiatravesables se ignoran durante el drop-through
            if (platform.passThrough && fighter.dropThroughFrames > 0)
            {
                continue;
            }
            if (fighter.position.x < platform.left || fighter.position.x > platform.right)
            {
                continue;
            }

            const bool standing = wasGrounded && std::abs(currentFeet - platform.top) <= 8.0F;
            const bool crossed = !wasGrounded && previousFeet <= platform.top + 6.0F && currentFeet >= platform.top;
            if (!standing && !crossed)
            {
                continue;
            }

            fighter.position.y = platform.top;
            fighter.velocity.y = 0.0F;
            fighter.grounded = true;
            fighter.onPassThrough = platform.passThrough;
            fighter.ledgeGrabCount = 0;

            if (!wasGrounded)
            {
                fighter.airJumps = 1;
                fighter.fastFalling = false;
                if (IsDamageState(fighter.state))
                {
                    fighter.hitstunFrames = 0;
                    fighter.launchKnockback = 0.0F;
                }

                if (fighter.state == ActionState::EscapeAir)
                {
                    // Wavedash / Waveland: conserva la inercia horizontal y desliza
                    fighter.landingLagFrames = 10;
                    SetActionState(fighter, ActionState::Landing);
                }
                else if (fighter.state == ActionState::FallSpecial || fighter.state == ActionState::SpecialUp)
                {
                    // Respeta el lag largo de la caida indefensa (28 frames)
                    if (fighter.landingLagFrames <= 0)
                    {
                        fighter.landingLagFrames = 28;
                    }
                    SetActionState(fighter, ActionState::LandingFallSpecial);
                }
                else
                {
                    fighter.landingLagFrames = 3;
                    SetActionState(fighter, ActionState::Landing);
                }
                result = CollisionResult::Landed;
            }
            break;
        }
    }

    // Only walking off a surface grants grace; jumping and dropping already clear grounded.
    if (wasGrounded && !fighter.grounded &&
        (fighter.state == ActionState::Wait || (fighter.state >= ActionState::WalkSlow && fighter.state <= ActionState::Turn)))
        fighter.coyoteFrames = 5;
    if (fighter.grounded) fighter.coyoteFrames = 0;

    if (!fighter.grounded && !onLedge && fighter.hitstunFrames <= 0 && fighter.ledgeRegrabTimer <= 0 &&
        fighter.velocity.y >= -160.0F)
    {
        const float feetBelowLedge = fighter.position.y - stage.groundY;
        const bool atLedgeHeight = feetBelowLedge >= 40.0F && feetBelowLedge <= 140.0F;
        const bool nearLeftLedge = fighter.position.x <= stage.groundLeft + 22.0F &&
                                   fighter.position.x >= stage.groundLeft - 38.0F;
        const bool nearRightLedge = fighter.position.x >= stage.groundRight - 22.0F &&
                                    fighter.position.x <= stage.groundRight + 38.0F;

        const auto grab = [&](std::size_t side) {
            const bool left = side == 0;
            const float cornerX = left ? stage.groundLeft : stage.groundRight;
            const int facing = left ? 1 : -1; // siempre mirando hacia adentro del escenario
            stage.ledgeOccupants[side] = fighter.playerIndex;
            fighter.ledgeId = static_cast<int>(side);
            fighter.facingDir = facing;
            fighter.ledgeCorner = Vector2{ cornerX, stage.groundY };
            fighter.position = Vector2{ cornerX - static_cast<float>(facing) * kLedgeHangOffsetX,
                                        stage.groundY + kLedgeHangFeetBelow };
            fighter.velocity = Vector2{ 0.0F, 0.0F };
            fighter.grounded = false;
            fighter.fastFalling = false;
            fighter.shielding = false;
            fighter.airJumps = 1;
            // La invulnerabilidad disminuye en cada reagarre sin tocar el suelo
            const int invulnerability = std::max(0, 45 - 20 * fighter.ledgeGrabCount);
            ++fighter.ledgeGrabCount;
            fighter.ledgeInvulnerability = invulnerability;
            fighter.invulnerabilityFrames = std::max(fighter.invulnerabilityFrames, invulnerability);
            SetActionState(fighter, ActionState::CliffCatch);
            result = CollisionResult::GrabbedLedge;
        };

        if (atLedgeHeight && nearLeftLedge && stage.ledgeOccupants[0] == -1)
        {
            grab(0);
        }
        else if (atLedgeHeight && nearRightLedge && stage.ledgeOccupants[1] == -1)
        {
            grab(1);
        }
    }

    if (!fighter.grounded && fighter.dropThroughFrames > 0)
    {
        --fighter.dropThroughFrames;
    }

    if (fighter.position.x < stage.blastLeft || fighter.position.x > stage.blastRight ||
        fighter.position.y < stage.blastTop || fighter.position.y > stage.blastBottom)
    {
        ReleaseLedge(stage, fighter);
        result = CollisionResult::KnockedOut;
    }
    return result;
}

void ReleaseLedge(Stage& stage, Fighter& fighter)
{
    if (fighter.ledgeId >= 0 && fighter.ledgeId < static_cast<int>(stage.ledgeOccupants.size()) &&
        stage.ledgeOccupants[static_cast<std::size_t>(fighter.ledgeId)] == fighter.playerIndex)
    {
        stage.ledgeOccupants[static_cast<std::size_t>(fighter.ledgeId)] = -1;
    }
    fighter.ledgeId = -1;
}

void DrawStage(const Stage& stage)
{
    // Fondo de arena con degradado y líneas de atmósfera
    DrawRectangle(0, 0, 1280, 650, Color{ 14, 18, 28, 255 });
    for (int x = 0; x <= 1280; x += 64)
    {
        DrawLine(x, 0, x, 650, Color{ 22, 30, 48, 60 });
    }
    for (int y = 0; y <= 650; y += 48)
    {
        DrawLine(0, y, 1280, y, Color{ 22, 30, 48, 60 });
    }

    // --- ESTRUCTURA INFERIOR SÓLIDA DEL ESCENARIO (Isla flotante / Base tech) ---
    const float gL = stage.groundLeft;
    const float gR = stage.groundRight;
    const float gY = stage.groundY;

    // Cuerpo principal bajo el suelo
    DrawRectangle(static_cast<int>(gL), static_cast<int>(gY),
                  static_cast<int>(gR - gL), 44, Color{ 42, 54, 72, 255 });

    // Bisel trapezoidal intermedio
    const Vector2 trap1_v1{ gL, gY + 44.0F };
    const Vector2 trap1_v2{ gR, gY + 44.0F };
    const Vector2 trap1_v3{ gR - 75.0F, gY + 110.0F };
    const Vector2 trap1_v4{ gL + 75.0F, gY + 110.0F };
    DrawTriangle(trap1_v1, trap1_v4, trap1_v2, Color{ 30, 40, 56, 255 });
    DrawTriangle(trap1_v2, trap1_v4, trap1_v3, Color{ 30, 40, 56, 255 });

    // Quilla inferior piramidal
    const Vector2 keel_v1{ gL + 75.0F, gY + 110.0F };
    const Vector2 keel_v2{ gR - 75.0F, gY + 110.0F };
    const Vector2 keel_v3{ 480.0F, gY + 175.0F };
    DrawTriangle(keel_v1, keel_v3, keel_v2, Color{ 20, 28, 42, 255 });

    // Líneas de detalle y runas/costuras metálicas en la base
    DrawLineV(trap1_v1, trap1_v4, Color{ 55, 75, 100, 255 });
    DrawLineV(trap1_v2, trap1_v3, Color{ 55, 75, 100, 255 });
    DrawLineV(keel_v1, keel_v3, Color{ 45, 65, 90, 255 });
    DrawLineV(keel_v2, keel_v3, Color{ 45, 65, 90, 255 });
    DrawLine(static_cast<int>(gL + 75.0F), static_cast<int>(gY + 110.0F),
             static_cast<int>(gR - 75.0F), static_cast<int>(gY + 110.0F), Color{ 50, 70, 95, 255 });
    DrawCircle(480, static_cast<int>(gY + 160.0F), 9.0F, Color{ 35, 95, 160, 200 });
    DrawCircleLines(480, static_cast<int>(gY + 160.0F), 9.0F, SKYBLUE);

    // --- RENDERIZADO DE PLATAFORMAS Y TARIMAS ---
    for (const StagePlatform& platform : stage.platforms)
    {
        const int pLeft = static_cast<int>(platform.left);
        const int pTop = static_cast<int>(platform.top);
        const int pWidth = static_cast<int>(platform.right - platform.left);

        if (platform.passThrough)
        {
            // Tarima superior semiatravesable con estilo luminoso
            DrawRectangle(pLeft, pTop, pWidth, 14, Color{ 68, 92, 122, 240 });
            DrawRectangle(pLeft, pTop + 14, pWidth, 3, Color{ 38, 55, 75, 180 });
            // Borde superior iluminado
            DrawLine(pLeft, pTop, pLeft + pWidth, pTop, Color{ 160, 205, 245, 255 });
            DrawLine(pLeft, pTop + 1, pLeft + pWidth, pTop + 1, Color{ 100, 160, 220, 255 });
            // Marcadores de extremo de tarima
            DrawRectangle(pLeft - 2, pTop - 1, 4, 16, Color{ 120, 185, 240, 255 });
            DrawRectangle(pLeft + pWidth - 2, pTop - 1, 4, 16, Color{ 120, 185, 240, 255 });
        }
        else
        {
            // Superficie superior del suelo principal
            DrawLine(pLeft, pTop, pLeft + pWidth, pTop, Color{ 210, 235, 255, 255 });
            DrawLine(pLeft, pTop + 1, pLeft + pWidth, pTop + 1, Color{ 130, 175, 220, 255 });
        }
    }

    // --- BORDES DEL ESCENARIO (LEDGES / ESQUINAS) ---
    // Indicador visual de esquina izquierda
    const bool leftOccupied = stage.ledgeOccupants[0] != -1;
    const Color leftLedgeColor = leftOccupied ? YELLOW : Color{ 100, 200, 255, 255 };
    DrawRectangle(static_cast<int>(gL) - 3, static_cast<int>(gY) - 2, 7, 7, leftLedgeColor);
    DrawRectangleLines(static_cast<int>(gL) - 4, static_cast<int>(gY) - 3, 9, 9, leftOccupied ? GOLD : SKYBLUE);
    if (leftOccupied)
    {
        DrawCircleLines(static_cast<int>(gL), static_cast<int>(gY), 12.0F, ColorAlpha(YELLOW, 0.7F));
    }

    // Indicador visual de esquina derecha
    const bool rightOccupied = stage.ledgeOccupants[1] != -1;
    const Color rightLedgeColor = rightOccupied ? YELLOW : Color{ 100, 200, 255, 255 };
    DrawRectangle(static_cast<int>(gR) - 4, static_cast<int>(gY) - 2, 7, 7, rightLedgeColor);
    DrawRectangleLines(static_cast<int>(gR) - 5, static_cast<int>(gY) - 3, 9, 9, rightOccupied ? GOLD : SKYBLUE);
    if (rightOccupied)
    {
        DrawCircleLines(static_cast<int>(gR), static_cast<int>(gY), 12.0F, ColorAlpha(YELLOW, 0.7F));
    }
}
