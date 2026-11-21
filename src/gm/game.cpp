#if __has_include("gm/game.hpp")
#include "gm/game.hpp"
#else
#include "game.hpp"
#endif

#include "win32_icon.hpp"
#include "if/character_select.hpp"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kFixedDt = 1.0F / 60.0F;

enum class PauseOption { Resume, Menu, Options, ChangeFighter, Count };
constexpr int kPauseOptionCount = static_cast<int>(PauseOption::Count);

bool PadPressed(int gamepad, int button)
{
    return IsGamepadAvailable(gamepad) && IsGamepadButtonPressed(gamepad, button);
}

bool PadDown(int gamepad, int button)
{
    return IsGamepadAvailable(gamepad) && IsGamepadButtonDown(gamepad, button);
}

bool PadReleased(int gamepad, int button)
{
    return IsGamepadAvailable(gamepad) && IsGamepadButtonReleased(gamepad, button);
}

void ResumeGame(GameContext& ctx)
{
    ctx.scene = SceneState::InGame;
    ctx.p1Input = PlayerInputState{};
    ctx.accumulator = 0.0;
}

void ActivatePauseOption(GameContext& ctx)
{
    switch (static_cast<PauseOption>(ctx.pauseMenuSelection))
    {
    case PauseOption::Resume:
        ResumeGame(ctx);
        break;
    case PauseOption::Menu:
        UIComponents::g_transition.Start(static_cast<int>(SceneState::MainMenu));
        break;
    case PauseOption::Options:
        ctx.optionsReturnScene = SceneState::Paused;
        ctx.optionsSelection = 5;
        UIComponents::g_transition.Start(static_cast<int>(SceneState::Options));
        break;
    case PauseOption::ChangeFighter:
        OpenCharacterSelect(ctx);
        break;
    case PauseOption::Count:
        break;
    }
}

void CycleOpponentMode(GameContext& ctx)
{
    ctx.opponentMode = static_cast<OpponentMode>((static_cast<int>(ctx.opponentMode) + 1) % 3);
    ctx.fighters[1].cpu = true;
    ctx.fighters[1].cpuDiShield = (ctx.opponentMode == OpponentMode::DummyDiShield);
}

void OpenControls(GameContext& ctx)
{
    ctx.controlsOriginal = GetControlSettings();
    ctx.controlsSelection = 0;
    ctx.controlsScroll = 0;
    ctx.controlsCapturing = false;
    ctx.controlsCaptureAction = -1;
    ctx.controlsConflictAction = -1;
    ctx.controlsMessage.clear();
    ctx.scene = SceneState::ControlsConfig;
    ctx.p1Input = PlayerInputState{};
    ctx.accumulator = 0.0;
}

void LeaveControls(GameContext& ctx)
{
    GetControlSettings() = ctx.controlsOriginal;
    ctx.p1Input = PlayerInputState{};
    ctx.accumulator = 0.0;
    UIComponents::g_transition.Start(static_cast<int>(SceneState::Options));
}

// Se llama UNA vez por frame de render: acumula los eventos hasta que un tick fisico los consuma.
void PollPlayer1Events(PlayerInputState& s)
{
    s.leftPressed = s.leftPressed || IsControlPressed(ControlAction::MoveLeft);
    s.rightPressed = s.rightPressed || IsControlPressed(ControlAction::MoveRight);
    s.jumpPressed = s.jumpPressed || IsControlPressed(ControlAction::Jump) || PadPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    s.jumpReleased = s.jumpReleased || IsControlReleased(ControlAction::Jump) || PadReleased(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    s.attackPressed = s.attackPressed || IsControlPressed(ControlAction::LightAttack) || IsControlPressed(ControlAction::HeavyAttack) || PadPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
                      PadPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    s.heavyPressed = s.heavyPressed || IsControlPressed(ControlAction::HeavyAttack);
    s.specialPressed = s.specialPressed || IsControlPressed(ControlAction::Special) || PadPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    s.shieldPressed = s.shieldPressed || IsControlPressed(ControlAction::Dodge) ||
                      PadPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
    s.downReleased = s.downReleased || IsControlReleased(ControlAction::AimDown);
}

// Se llama UNA vez por tick fisico (60 Hz): consume los eventos acumulados y avanza los contadores.
//  - Flechas / WASD: direccion (UP / W solo apunta: Up-Tilt / Up-Air / Up-B / subir borde).
//  - Z / W / Flecha Arriba: salto, X / J: ataque, C / K: especial, SHIFT / SPACE / L: escudo / air dodge.
//  - Mantener flecha / A / D = caminar; doble tap = Dash / Run (y Dash Dance cambiando de lado).
FighterInput BuildPlayer1Input(PlayerInputState& s, const Fighter& fighter)
{
    FighterInput input{};

    if (s.ticksSinceTap < 999) ++s.ticksSinceTap;

    const auto registerPress = [&s](int dir) {
        if (s.lastTapDir == dir && s.ticksSinceTap <= 14)
        {
            s.dashLatch = true; // doble tap intencional
        }
        s.lastTapDir = dir;
        s.ticksSinceTap = 0;
        s.lastHorizDir = dir;
    };

    int pressedDir = 0;
    if (s.leftPressed)
    {
        registerPress(-1);
        pressedDir = -1;
    }
    if (s.rightPressed)
    {
        registerPress(1);
        pressedDir = 1;
    }

    // Resolucion de pulsacion simultanea: gana la ultima direccion presionada
    const bool leftDown = IsControlDown(ControlAction::MoveLeft);
    const bool rightDown = IsControlDown(ControlAction::MoveRight);
    int dir = 0;
    if (leftDown && rightDown) dir = s.lastHorizDir;
    else if (rightDown) dir = 1;
    else if (leftDown) dir = -1;
    else dir = pressedDir; // tap mas corto que un tick

    if (dir == 0)
    {
        s.heldDir = 0;
        s.horizHoldFrames = 0;
        s.dashLatch = false;
    }
    else
    {
        if (dir != s.heldDir)
        {
            s.heldDir = dir;
            s.horizHoldFrames = 0;
            if (dir != s.lastTapDir) s.dashLatch = false;
        }
        ++s.horizHoldFrames;
    }

    const bool downHeld = IsControlDown(ControlAction::AimDown);
    const bool upHeld = IsControlDown(ControlAction::AimUp);
    const ActionState state = fighter.state;
    const bool groundedStance = fighter.grounded &&
                                (state == ActionState::Wait || state == ActionState::WalkSlow ||
                                 state == ActionState::WalkMid || state == ActionState::WalkFast ||
                                 state == ActionState::Squat);

    // --- Eje horizontal: caminar (0.6) vs Dash/Run (1.0, solo tras doble tap) ---
    float axis = 0.0F;
    if (dir != 0)
    {
        const bool running = state == ActionState::Dash || state == ActionState::Run;
        const float dirF = static_cast<float>(dir);
        axis = (!fighter.grounded || running || s.dashLatch || IsControlDown(ControlAction::Run)) ? dirF : dirF * 0.6F;
        if (downHeld && groundedStance) axis = 0.0F; // agachado no camina
    }
    input.stickX = axis;

    // --- Eje vertical: Down = +1, Up = -1 ---
    float vert = 0.0F;
    if (downHeld) vert += 1.0F;
    if (upHeld) vert -= 1.0F;
    input.stickY = vert;

    // Compatibilidad opcional con Gamepad 0 (Left Stick)
    if (IsGamepadAvailable(0) && GetGamepadAxisCount(0) > GAMEPAD_AXIS_LEFT_Y)
    {
        const float padX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        const float padY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (std::abs(padX) > 0.16F) input.stickX = std::clamp(padX, -1.0F, 1.0F);
        if (std::abs(padY) > 0.16F) input.stickY = std::clamp(padY, -1.0F, 1.0F);
    }

    // --- Bajar plataforma: tap rapido hacia abajo (soltar en <= 8 ticks) ---
    if (downHeld) ++s.downHoldFrames;
    if (s.downReleased && !downHeld)
    {
        input.dropThroughPressed = s.downHoldFrames <= 8;
    }
    if (!downHeld) s.downHoldFrames = 0;

    // --- Botones ---
    input.jumpPressed = s.jumpPressed;
    input.jumpReleased = s.jumpReleased;
    input.jumpDown = IsControlDown(ControlAction::Jump) || PadDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    input.attackPressed = s.attackPressed;
    input.attackDown = IsControlDown(ControlAction::LightAttack) || IsControlDown(ControlAction::HeavyAttack) || PadDown(0, GAMEPAD_BUTTON_RIGHT_FACE_LEFT) ||
                       PadDown(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    input.specialPressed = s.specialPressed;
    input.shieldDown = IsControlDown(ControlAction::Dodge) || PadDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1) || PadDown(0, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
    input.airDodgePressed = s.shieldPressed;

    // --- Seleccion contextual de ataque ---
    if (fighter.grounded && s.heavyPressed) input.attackKind = AttackKind::ForwardSmash;
    else if (IsAerialState(state) || !fighter.grounded)
    {
        if (input.stickY < -0.45F) input.attackKind = AttackKind::UpAir;
        else if (input.stickY > 0.45F) input.attackKind = AttackKind::DownAir;
        else if (input.stickX * static_cast<float>(fighter.facingDir) < -0.35F) input.attackKind = AttackKind::Bair;
        else if (std::abs(input.stickX) > 0.35F) input.attackKind = AttackKind::Fair;
        else input.attackKind = AttackKind::Nair;
    }
    else if (input.stickY < -0.45F)
    {
        input.attackKind = AttackKind::UpTilt;
    }
    else if (input.stickY > 0.45F)
    {
        input.attackKind = AttackKind::DownTilt;
    }
    else if (dir != 0)
    {
        // F-Smash: flecha + X casi simultaneos (flecha pulsada hace <= 4 ticks). Si ya caminabas -> F-Tilt.
        const bool running = state == ActionState::Dash || state == ActionState::Run;
        const bool smash = !running && s.horizHoldFrames <= 4;
        input.attackKind = smash ? AttackKind::ForwardSmash : AttackKind::ForwardTilt;
    }
    else
    {
        input.attackKind = AttackKind::Jab;
    }

    // Eventos consumidos en este tick fisico
    s.leftPressed = false;
    s.rightPressed = false;
    s.jumpPressed = false;
    s.jumpReleased = false;
    s.attackPressed = false;
    s.heavyPressed = false;
    s.specialPressed = false;
    s.shieldPressed = false;
    s.downReleased = false;

    return input;
}

FighterInput BuildDummyInput(OpponentMode mode, Fighter& dummy, const Fighter& opponent, const Stage& stage)
{
    FighterInput input{};
    if (mode == OpponentMode::DummyQuiet) return input;

    const float dx = opponent.position.x - dummy.position.x;
    const float dist = std::abs(dx);

    if (mode == OpponentMode::DummyDiShield)
    {
        // Reacciona cubriendose cuando el oponente ataca cerca
        if (IsAttackActive(opponent) && dist < 150.0F && dummy.grounded)
        {
            input.shieldDown = true;
        }
        // Aplicar DI hacia el escenario cuando sale disparado
        if (dummy.hitlagFrames > 0)
        {
            input.stickX = dummy.position.x < 480.0F ? 1.0F : -1.0F;
            input.stickY = -0.5F;
        }
        return input;
    }

    // Modo CpuBasic: Inteligencia artificial activa
    if (mode == OpponentMode::CpuBasic)
    {
        // 1. Recuperacion si esta fuera del escenario
        const bool offStage = dummy.position.x < stage.groundLeft || dummy.position.x > stage.groundRight;
        if (offStage || (!dummy.grounded && dummy.position.y > stage.groundY + 70.0F))
        {
            // Moverse hacia el centro del escenario
            input.stickX = dummy.position.x < 480.0F ? 1.0F : -1.0F;
            if (dummy.velocity.y > 50.0F && dummy.airJumps > 0)
            {
                input.jumpPressed = true;
            }
            else if (dummy.position.y > stage.groundY + 120.0F && dummy.state != ActionState::SpecialUp &&
                     dummy.state != ActionState::FallSpecial)
            {
                input.specialPressed = true;
                input.stickY = -1.0F;
            }
            return input;
        }

        // 2. Comportamiento en combate sobre el escenario
        if (dummy.grounded)
        {
            if (dist > 90.0F)
            {
                // Acercarse al jugador
                input.stickX = dx > 0.0F ? 1.0F : -1.0F;
                // Salto ocasional de aproximacion
                if (dist > 180.0F && dummy.stateFrame % 110 == 0)
                {
                    input.jumpPressed = true;
                }
            }
            else
            {
                // En rango cuerpo a cuerpo: encarar al rival
                dummy.facingDir = dx > 0.0F ? 1 : -1;

                // Ocasionalmente escudarse si el rival ataca
                if (IsAttackActive(opponent) && dummy.stateFrame % 2 == 0)
                {
                    input.shieldDown = true;
                }
                else if (dummy.stateFrame % 28 == 0 && !IsDamageState(dummy.state) && dummy.state != ActionState::ShieldStun)
                {
                    input.attackPressed = true;
                    input.attackDown = true;
                    const int attackChoice = (dummy.stateFrame / 28) % 3;
                    if (attackChoice == 0) input.attackKind = AttackKind::Jab;
                    else if (attackChoice == 1) input.attackKind = AttackKind::ForwardTilt;
                    else input.attackKind = AttackKind::DownTilt;
                }
            }
        }
        else
        {
            // En el aire: flotar hacia el rival y conectar aereos
            input.stickX = dx > 0.0F ? 0.75F : -0.75F;
            if (dist < 85.0F && dummy.stateFrame % 25 == 0)
            {
                input.attackPressed = true;
                input.attackKind = AttackKind::Nair;
            }
        }
    }

    return input;
}

SpriteAnimation AnimationFor(ActionState state)
{
    switch (state)
    {
    case ActionState::WalkSlow:
    case ActionState::WalkMid:
    case ActionState::WalkFast:
    case ActionState::Dash:
    case ActionState::Run:
    case ActionState::RunBrake:
    case ActionState::Turn:
    case ActionState::CliffClimb: return SpriteAnimation::Run;
    case ActionState::JumpSquat:
    case ActionState::JumpF:
    case ActionState::JumpB:
    case ActionState::JumpAerialF:
    case ActionState::JumpAerialB:
    case ActionState::CliffCatch:
    case ActionState::CliffWait:
    case ActionState::CliffJump:
    case ActionState::SpecialUp: return SpriteAnimation::Jump;
    case ActionState::Fall:
    case ActionState::FallAerial:
    case ActionState::EscapeAir:
    case ActionState::FallSpecial: return SpriteAnimation::Fall;
    case ActionState::AttackJab:
    case ActionState::AttackForwardTilt:
    case ActionState::AttackUpTilt:
    case ActionState::AttackDownTilt:
    case ActionState::AttackForwardSmash: return SpriteAnimation::AttackJab;
    case ActionState::AttackNair:
    case ActionState::AttackFair:
    case ActionState::AttackBair:
    case ActionState::AttackUair:
    case ActionState::AttackDair: return SpriteAnimation::AttackAerial;
    case ActionState::DamageHi:
    case ActionState::DamageLw:
    case ActionState::DamageFly: return SpriteAnimation::DamageFly;
    default: return SpriteAnimation::Idle;
    }
}

bool AnimationLoops(ActionState state)
{
    return !IsAttackingState(state) && !IsDamageState(state);
}

void DrawFighterSprite(const SpriteRenderer& sprites, const Fighter& fighter)
{
    if (!fighter.alive) return;
    const Rectangle hurtbox = GetHurtbox(fighter);
    const EnvironmentCollisionBox ecb = BuildECB(fighter);
    const Color fallback = fighter.playerIndex == 0 ? Color{ 70, 155, 250, 255 } : Color{ 230, 90, 130, 255 };
    const bool blink = fighter.invulnerabilityFrames > 0 && (fighter.invulnerabilityFrames / 5) % 2 == 0;
    const Color tint = blink ? ColorAlpha(WHITE, 0.46f) : WHITE;
    if (!sprites.Draw(static_cast<int>(fighter.character), AnimationFor(fighter.state), fighter.stateFrame,
                      AnimationLoops(fighter.state), fighter.facingDir, hurtbox,
                      fighter.position.y + ecb.bottom.y, tint))
        sprites.DrawFallback(hurtbox, fighter.position.y + ecb.bottom.y, fallback);
    const float x = fighter.position.x;
    const float y = hurtbox.y - 6;
    DrawTriangle({x - 5, y - 5}, {x, y}, {x + 5, y - 5}, fighter.cpu ? UITheme::P2_COLOR : UITheme::P1_COLOR);

    if (fighter.shielding)
    {
        DrawCircleLines(static_cast<int>(fighter.position.x), static_cast<int>(fighter.position.y - 53.0F),
                        fighter.attributes.initialShieldSize * std::clamp(fighter.shieldHealth / 100.0F, 0.28F, 1.0F), SKYBLUE);
    }
}

void DrawDebugFighter(const Fighter& fighter)
{
    if (!fighter.alive) return;
    DrawRectangleRec(GetHurtbox(fighter), ColorAlpha(fighter.playerIndex == 0 ? SKYBLUE : PINK, 0.12F));
    DrawRectangleLinesEx(GetHurtbox(fighter), 2.0F, fighter.playerIndex == 0 ? SKYBLUE : PINK);
    DrawECB(fighter);
    DrawAttackHitboxes(fighter);
}

void HandleKnockout(Stage& stage, Fighter& fighter, float spawnX, int facing)
{
    const int nextStocks = fighter.stocks - 1;
    ReleaseLedge(stage, fighter);
    if (nextStocks <= 0)
    {
        fighter.stocks = 0;
        fighter.alive = false;
        return;
    }
    const int player = fighter.playerIndex;
    const FighterAttributes attributes = fighter.attributes;
    const auto character = fighter.character;
    const bool cpu = fighter.cpu, di = fighter.cpuDiShield;
    ResetFighter(fighter, spawnX, stage.groundY, facing, player);
    fighter.character = character;
    fighter.cpu = cpu;
    fighter.cpuDiShield = di;
    fighter.attributes = attributes;
    fighter.stocks = nextStocks;
    fighter.invulnerabilityFrames = 120;
    fighter.state = ActionState::Respawn;
    fighter.percent = 0.0F;
}

void ReleaseUnusedLedge(Stage& stage, Fighter& fighter, int previousLedge)
{
    if (previousLedge < 0 || previousLedge >= static_cast<int>(stage.ledgeOccupants.size())) return;
    const bool stillOnLedge = fighter.state == ActionState::CliffWait || fighter.state == ActionState::CliffCatch ||
                              fighter.state == ActionState::CliffClimb;
    if (!stillOnLedge || fighter.ledgeId < 0)
    {
        if (stage.ledgeOccupants[static_cast<std::size_t>(previousLedge)] == fighter.playerIndex)
        {
            stage.ledgeOccupants[static_cast<std::size_t>(previousLedge)] = -1;
        }
        fighter.ledgeId = -1;
    }
}
} // namespace

void InitGame(GameContext& ctx)
{
    LoadControlSettings();
    ctx.sprites.Load();
    ctx.menuRenderer.Load();
    ctx.stage = CreateTrainingStage();
    ResetMatch(ctx);
    InitializeCamera(ctx.camera);
    ctx.scene = SceneState::MainMenu;
    ctx.mainMenuSelection = 0;
    ctx.optionsSelection = 0;
    ctx.pauseMenuSelection = 0;
    ctx.opponentMode = OpponentMode::DummyQuiet;
    ctx.debugMode = false;
    ctx.shouldQuit = false;
    ctx.menuAnimFrame = 0;
}

void ResetMatch(GameContext& ctx)
{
    ResetFighter(ctx.fighters[0], 340.0F, ctx.stage.groundY, 1, 0);
    ResetFighter(ctx.fighters[1], 620.0F, ctx.stage.groundY, -1, 1);
    ctx.fighters[0].character = ctx.selected;
    ctx.fighters[1].character = Character::Shadow;
    ctx.stockFlash = {};
    InitializeCamera(ctx.camera);
    ctx.fighters[1].cpu = true;
    ctx.fighters[1].cpuDiShield = (ctx.opponentMode == OpponentMode::DummyDiShield);
    ctx.stage.ledgeOccupants = { -1, -1 };
    ctx.p1Input = PlayerInputState{};
    ctx.accumulator = 0.0;
}

void ShutdownGame(GameContext& ctx)
{
    ctx.menuRenderer.Unload();
    ctx.sprites.Unload();
}

void UpdateGame(GameContext& ctx)
{
    ++ctx.menuAnimFrame;
    if (ctx.scene == SceneState::CharacterSelect)
    {
        UpdateCharacterSelect(ctx);
        return;
    }
    if (UIComponents::g_transition.active && UIComponents::g_transition.fadingOut) return;

    // --- ESTADO: MENU PRINCIPAL ---
    if (ctx.scene == SceneState::MainMenu)
    {
        if (IsKeyPressed(KEY_UP) || PadPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP))
        {
            ctx.mainMenuSelection = (ctx.mainMenuSelection + 3) % 4;
            UITheme::AudioManager::Instance().PlayCursor();
        }
        if (IsKeyPressed(KEY_DOWN) || PadPressed(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        {
            ctx.mainMenuSelection = (ctx.mainMenuSelection + 1) % 4;
            UITheme::AudioManager::Instance().PlayCursor();
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            UITheme::AudioManager::Instance().PlayCancel();
            ctx.shouldQuit = true;
            return;
        }

        const bool confirmKey = PadPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_SPACE);
        if (confirmKey)
        {
            const int choice = ctx.mainMenuSelection;
            if (choice == 0) // PLAY
            {
                OpenCharacterSelect(ctx);
            }
            else if (choice == 1) // CONTROLS
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                UIComponents::g_transition.Start(static_cast<int>(SceneState::Controls));
            }
            else if (choice == 2) // OPTIONS
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                UIComponents::g_transition.Start(static_cast<int>(SceneState::Options));
                ctx.optionsSelection = 0;
                ctx.optionsReturnScene = SceneState::MainMenu;
            }
            else if (choice == 3) // QUIT
            {
                UITheme::AudioManager::Instance().PlayCancel();
                ctx.shouldQuit = true;
            }
        }
        return;
    }

    // --- ESTADO: PANTALLA DE CONTROLES ---
    if (ctx.scene == SceneState::Controls)
    {
        const bool backKey = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_ENTER);
        const float backBtnY = 460.0F;
        const float backBtnW = UITheme::BUTTON_WIDTH;
        const float backBtnH = UITheme::BUTTON_HEIGHT;
        const Rectangle backRec = { 480.0F - backBtnW * 0.5F, backBtnY - backBtnH * 0.5F, backBtnW, backBtnH };
        const bool mouseOverBack = CheckCollisionPointRec(UITheme::GetVirtualMousePosition(), backRec);
        const bool backClick = mouseOverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const bool confirmKey = PadPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_SPACE);

        if (backKey || backClick || (mouseOverBack && confirmKey))
        {
            UITheme::AudioManager::Instance().PlayCancel();
            UIComponents::g_transition.Start(static_cast<int>(SceneState::MainMenu));
        }
        return;
    }

    // --- ESTADO: PANTALLA DE OPCIONES ---
    if (ctx.scene == SceneState::Options)
    {
        const bool upKey = IsKeyPressed(KEY_UP);
        const bool downKey = IsKeyPressed(KEY_DOWN);
        if (upKey)
        {
            ctx.optionsSelection = (ctx.optionsSelection + 6) % 7;
            UITheme::AudioManager::Instance().PlayCursor();
        }
        if (downKey)
        {
            ctx.optionsSelection = (ctx.optionsSelection + 1) % 7;
            UITheme::AudioManager::Instance().PlayCursor();
        }

        // Teclado ESC / Z para volver
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Z))
        {
            UITheme::AudioManager::Instance().PlayCancel();
            UIComponents::g_transition.Start(static_cast<int>(ctx.optionsReturnScene));
            return;
        }

        const bool leftKey = IsKeyPressed(KEY_LEFT);
        const bool rightKey = IsKeyPressed(KEY_RIGHT);
        const bool enterKey = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_SPACE);

        if (ctx.optionsSelection == 0) // OPPONENT MODE
        {
            if (leftKey || rightKey || enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                CycleOpponentMode(ctx);
            }
        }
        else if (ctx.optionsSelection == 1) // DEBUG OVERLAY
        {
            if (leftKey || rightKey || enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                ctx.debugMode = !ctx.debugMode;
            }
        }
        else if (ctx.optionsSelection == 2) // SCALING MODE
        {
            if (leftKey || rightKey || enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                UITheme::s_scalingMode = (UITheme::s_scalingMode == UITheme::ScalingMode::FitSmooth)
                    ? UITheme::ScalingMode::PixelPerfect
                    : UITheme::ScalingMode::FitSmooth;
            }
        }
        else if (ctx.optionsSelection == 3) // DISPLAY MODE
        {
            if (leftKey || rightKey || enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                ToggleFullscreen();
                SetWindowFocused();
                ForceWindowActive(GetWindowHandle());
            }
        }
        else if (ctx.optionsSelection == 4) // LANGUAGE (IDIOMA)
        {
            if (leftKey || rightKey || enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                UITheme::s_language = (UITheme::s_language == UITheme::Language::Spanish)
                    ? UITheme::Language::English
                    : UITheme::Language::Spanish;
            }
        }
        else if (ctx.optionsSelection == 5) // CONTROLS
        {
            if (enterKey)
            {
                UITheme::AudioManager::Instance().PlayConfirm();
                OpenControls(ctx);
                return;
            }
        }
        else if (ctx.optionsSelection == 6) // BACK (VOLVER)
        {
            if (enterKey)
            {
                UITheme::AudioManager::Instance().PlayCancel();
                UIComponents::g_transition.Start(static_cast<int>(ctx.optionsReturnScene));
                return;
            }
        }
        return;
    }

    if (ctx.scene == SceneState::ControlsConfig)
    {
        const bool isEs = UITheme::s_language == UITheme::Language::Spanish;
        if (ctx.controlsMessageTimer > 0.0F) ctx.controlsMessageTimer -= GetFrameTime();
        if (ctx.controlsCapturing)
        {
            if (IsKeyPressed(KEY_ESCAPE))
            {
                ctx.controlsCapturing = false;
                ctx.controlsConflictAction = -1;
                ctx.controlsMessage.clear();
            }
            else if (ctx.controlsConflictAction >= 0)
            {
                if (IsKeyPressed(KEY_ENTER))
                {
                    if (!SwapControlBindings(static_cast<ControlAction>(ctx.controlsCaptureAction),
                                             static_cast<ControlAction>(ctx.controlsConflictAction)))
                    {
                        ctx.controlsConflictAction = -1;
                        ctx.controlsMessage = isEs ? "TECLA COMPARTIDA. ELIGE OTRA TECLA." : "SHARED KEY. CHOOSE ANOTHER KEY.";
                        return;
                    }
                    ctx.controlsMessage = isEs ? "ASIGNACIONES INTERCAMBIADAS" : "BINDINGS SWAPPED";
                    ctx.controlsMessageTimer = 2.0F;
                    ctx.controlsCapturing = false;
                    ctx.controlsConflictAction = -1;
                }
                else if (IsKeyPressed(KEY_R))
                {
                    ctx.controlsConflictAction = -1;
                    ctx.controlsMessage = isEs ? "PRESIONA OTRA TECLA..." : "PRESS ANOTHER KEY...";
                }
                else if (IsKeyPressed(KEY_C))
                {
                    ctx.controlsCapturing = false;
                    ctx.controlsConflictAction = -1;
                    ctx.controlsMessage.clear();
                }
            }
            else
            {
                int key = KEY_NULL;
                for (int candidate = GetKeyPressed(); candidate != KEY_NULL; candidate = GetKeyPressed())
                    if (IsValidRebindKey(candidate)) { key = candidate; break; }
                if (key != KEY_NULL)
                {
                    const ControlSettings& settings = GetControlSettings();
                    int conflict = -1;
                    for (int i = 0; i < static_cast<int>(ControlAction::Count); ++i)
                        if (i != ctx.controlsCaptureAction && settings.keys[static_cast<std::size_t>(i)] == key &&
                            !AreControlsCompatible(static_cast<ControlAction>(ctx.controlsCaptureAction), static_cast<ControlAction>(i))) conflict = i;
                    if (conflict >= 0)
                    {
                        ctx.controlsConflictKey = key;
                        ctx.controlsConflictAction = conflict;
                        ctx.controlsMessage = std::string(isEs ? "YA ASIGNADA A " : "ALREADY ASSIGNED TO ") + GetControlActionName(static_cast<ControlAction>(conflict), isEs);
                    }
                    else
                    {
                        GetControlSettings().keys[static_cast<std::size_t>(ctx.controlsCaptureAction)] = key;
                        GetControlSettings().preset = MovementPreset::Custom;
                        ctx.controlsMessage = isEs ? "TECLA ACTUALIZADA" : "KEY UPDATED";
                        ctx.controlsMessageTimer = 2.0F;
                        ctx.controlsCapturing = false;
                    }
                }
            }
            return;
        }

        const int actionCount = static_cast<int>(ControlAction::Count);
        const int saveIndex = actionCount + 1, resetIndex = actionCount + 2, backIndex = actionCount + 3;
        if (IsKeyPressed(KEY_UP)) ctx.controlsSelection = (ctx.controlsSelection + backIndex) % (backIndex + 1);
        if (IsKeyPressed(KEY_DOWN)) ctx.controlsSelection = (ctx.controlsSelection + 1) % (backIndex + 1);

        const bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_X);
        const Vector2 mouse = UITheme::GetVirtualMousePosition();
        const bool mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const Rectangle presetRects[3] = { { 266, 78, 130, 34 }, { 415, 78, 130, 34 }, { 564, 78, 130, 34 } };
        static Vector2 s_lastControlsMouse = { -999.0F, -999.0F };
        const bool controlsMouseMoved = std::abs(mouse.x - s_lastControlsMouse.x) > 1.5F ||
                                        std::abs(mouse.y - s_lastControlsMouse.y) > 1.5F;
        if (controlsMouseMoved) s_lastControlsMouse = mouse;
        for (int i = 0; i < 3; ++i)
        {
            if (mouseClick && CheckCollisionPointRec(mouse, presetRects[i]))
            {
                const bool changed = SetMovementPreset(static_cast<MovementPreset>(i));
                ctx.controlsMessage = changed ? (isEs ? "PRESET DE MOVIMIENTO ACTUALIZADO" : "MOVEMENT PRESET UPDATED")
                                              : (isEs ? "CONFLICTO: REVISA LAS TECLAS ASIGNADAS" : "CONFLICT: CHECK YOUR KEY BINDINGS");
                ctx.controlsMessageTimer = 2.0F;
                return;
            }
        }
        if (controlsMouseMoved)
        {
            for (int i = 0; i < 3; ++i)
                if (CheckCollisionPointRec(mouse, presetRects[i])) ctx.controlsSelection = 0;
            for (int row = 0; row < 7; ++row)
            {
                const Rectangle rowRect{ 118, 122.0F + row * 39.0F, 724, 35 };
                if (CheckCollisionPointRec(mouse, rowRect)) ctx.controlsSelection = 1 + ctx.controlsScroll + row;
            }
            if (CheckCollisionPointRec(mouse, Rectangle{ 215, 430, 160, 42 })) ctx.controlsSelection = saveIndex;
            if (CheckCollisionPointRec(mouse, Rectangle{ 400, 430, 190, 42 })) ctx.controlsSelection = resetIndex;
            if (CheckCollisionPointRec(mouse, Rectangle{ 615, 430, 130, 42 })) ctx.controlsSelection = backIndex;
        }
        bool mouseActivatesSelection = false;
        if (mouseClick)
        {
            for (int i = 0; i < 3; ++i)
            {
                if (CheckCollisionPointRec(mouse, presetRects[i]))
                {
                    ctx.controlsSelection = 0;
                    mouseActivatesSelection = true;
                }
            }
            for (int row = 0; row < 7; ++row)
            {
                const Rectangle rowRect{ 118, 122.0F + row * 39.0F, 724, 35 };
                if (CheckCollisionPointRec(mouse, rowRect))
                {
                    ctx.controlsSelection = 1 + ctx.controlsScroll + row;
                    mouseActivatesSelection = true;
                    break;
                }
            }
            if (CheckCollisionPointRec(mouse, Rectangle{ 215, 430, 160, 42 })) { ctx.controlsSelection = saveIndex; mouseActivatesSelection = true; }
            if (CheckCollisionPointRec(mouse, Rectangle{ 400, 430, 190, 42 })) { ctx.controlsSelection = resetIndex; mouseActivatesSelection = true; }
            if (CheckCollisionPointRec(mouse, Rectangle{ 615, 430, 130, 42 })) { ctx.controlsSelection = backIndex; mouseActivatesSelection = true; }
        }
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) ctx.controlsSelection = (ctx.controlsSelection + backIndex) % (backIndex + 1);
        if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) ctx.controlsSelection = (ctx.controlsSelection + 1) % (backIndex + 1);
        if (ctx.controlsSelection == 0 &&
            (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
             IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)))
        {
            int preset = static_cast<int>(GetControlSettings().preset);
            preset = (preset + ((IsKeyPressed(KEY_LEFT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) ? 2 : 1)) % 3;
            const bool changed = SetMovementPreset(static_cast<MovementPreset>(preset));
            ctx.controlsMessage = changed ? (isEs ? "PRESET DE MOVIMIENTO ACTUALIZADO" : "MOVEMENT PRESET UPDATED")
                                         : (isEs ? "CONFLICTO: REVISA LAS TECLAS ASIGNADAS" : "CONFLICT: CHECK YOUR KEY BINDINGS");
            ctx.controlsMessageTimer = 2.0F;
        }
        ctx.controlsScroll = std::clamp(ctx.controlsSelection - 7, 0, actionCount - 7);
        const bool accept = confirm || mouseActivatesSelection || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
        const bool back = IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Z);
        if (back)
        {
            LeaveControls(ctx);
            return;
        }
        if (ctx.controlsSelection >= 1 && ctx.controlsSelection <= actionCount && accept)
        {
            ctx.controlsCapturing = true;
            ctx.controlsCaptureAction = ctx.controlsSelection - 1;
            ctx.controlsMessage = isEs ? "PRESIONA UNA TECLA..." : "PRESS A KEY...";
            return;
        }
        if (ctx.controlsSelection == 0 && accept)
        {
            int preset = (static_cast<int>(GetControlSettings().preset) + 1) % 3;
            const bool changed = SetMovementPreset(static_cast<MovementPreset>(preset));
            ctx.controlsMessage = changed ? (isEs ? "PRESET DE MOVIMIENTO ACTUALIZADO" : "MOVEMENT PRESET UPDATED")
                                         : (isEs ? "CONFLICTO: REVISA LAS TECLAS ASIGNADAS" : "CONFLICT: CHECK YOUR KEY BINDINGS");
            ctx.controlsMessageTimer = 2.0F;
        }
        if (ctx.controlsSelection == saveIndex && accept)
        {
            const bool saved = SaveControlSettings();
            if (saved) ctx.controlsOriginal = GetControlSettings();
            ctx.controlsMessage = saved ? (isEs ? "CONTROLES GUARDADOS" : "CONTROLS SAVED")
                                       : (isEs ? "NO SE PUDO GUARDAR" : "SAVE FAILED");
            ctx.controlsMessageTimer = 2.5F;
        }
        else if (ctx.controlsSelection == resetIndex && accept)
        {
            ResetControlSettings();
            ctx.controlsMessage = isEs ? "VALORES PREDETERMINADOS RESTAURADOS" : "DEFAULTS RESTORED";
            ctx.controlsMessageTimer = 2.5F;
        }
        else if (ctx.controlsSelection == backIndex && accept)
        {
            LeaveControls(ctx);
        }
        return;
    }

    // --- ESTADO: MENU DE PAUSA ---
    if (ctx.scene == SceneState::Paused)
    {
        ctx.pauseEntranceTimer += GetFrameTime();

        // Reanudar rapido con ESC o P
        if (IsKeyPressed(KEY_ESCAPE) || IsControlPressed(ControlAction::Pause))
        {
            UITheme::AudioManager::Instance().PlayCancel();
            ResumeGame(ctx);
            return;
        }

        if (IsKeyPressed(KEY_UP) || PadPressed(0,GAMEPAD_BUTTON_LEFT_FACE_UP))
        {
            ctx.pauseMenuSelection = (ctx.pauseMenuSelection + kPauseOptionCount - 1) % kPauseOptionCount;
            UITheme::AudioManager::Instance().PlayCursor();
        }
        if (IsKeyPressed(KEY_DOWN) || PadPressed(0,GAMEPAD_BUTTON_LEFT_FACE_DOWN))
        {
            ctx.pauseMenuSelection = (ctx.pauseMenuSelection + 1) % kPauseOptionCount;
            UITheme::AudioManager::Instance().PlayCursor();
        }

        const bool confirmKey = PadPressed(0,GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_X) || IsKeyPressed(KEY_SPACE);
        if (confirmKey)
        {
            auto& audio = UITheme::AudioManager::Instance();
            if (ctx.pauseMenuSelection == static_cast<int>(PauseOption::Resume)) audio.PlayCancel();
            else if (ctx.pauseMenuSelection == static_cast<int>(PauseOption::Menu) ||
                     ctx.pauseMenuSelection == static_cast<int>(PauseOption::Options)) audio.PlayConfirm();
            ActivatePauseOption(ctx);
        }
        return;
    }

    // --- ESTADO: COMBATE EN VIVO (IN_GAME) ---
    // Pausar con tecla o haciendo clic en el icono de pausa en la esquina superior derecha
    const Rectangle pauseIconRect{ 914.0F, 12.0F, 34.0F, 34.0F };
    const bool clickPauseIcon = CheckCollisionPointRec(UITheme::GetVirtualMousePosition(), pauseIconRect) &&
                                IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    if (IsKeyPressed(KEY_ESCAPE) || IsControlPressed(ControlAction::Pause) || clickPauseIcon)
    {
        ctx.scene = SceneState::Paused;
        ctx.pauseMenuSelection = static_cast<int>(PauseOption::Resume);
        ctx.pauseEntranceTimer = 0.0F;
        ctx.pauseMenuSliding = {};
        ctx.pauseMenuSliding.lastMousePos = GetMousePosition();
        return;
    }

    if (IsKeyPressed(KEY_F1) && !HasControlBinding(KEY_F1)) ctx.debugMode = !ctx.debugMode;
    if (IsKeyPressed(KEY_F2) && !HasControlBinding(KEY_F2)) CycleOpponentMode(ctx);
    if (IsKeyPressed(KEY_R) && !HasControlBinding(KEY_R)) ResetMatch(ctx);

    // Acumula los eventos de teclado hasta que un tick fisico real de 60 Hz los consuma
    PollPlayer1Events(ctx.p1Input);

    const float frameTime = std::min(GetFrameTime(), 0.25F);
    for (float& flash : ctx.stockFlash) flash = std::max(0.0f, flash - frameTime);
    ctx.accumulator += static_cast<double>(frameTime);

    while (ctx.accumulator >= static_cast<double>(kFixedDt))
    {
        // Un tick fisico (FixedUpdate): aqui se consumen los eventos acumulados y avanzan los contadores
        FighterInput inputs[2]{
            BuildPlayer1Input(ctx.p1Input, ctx.fighters[0]),
            BuildDummyInput(ctx.opponentMode, ctx.fighters[1], ctx.fighters[0], ctx.stage)
        };

        for (int player = 0; player < 2; ++player)
        {
            Fighter& fighter = ctx.fighters[static_cast<std::size_t>(player)];
            if (!fighter.alive) continue;
            const int oldLedge = fighter.ledgeId;
            fighter.previousPosition = fighter.position;
            UpdateFighter(fighter, inputs[player], kFixedDt);
            IntegrateFighter(fighter, kFixedDt);
            ReleaseUnusedLedge(ctx.stage, fighter, oldLedge);
            const CollisionResult collision = ResolveStageCollision(ctx.stage, fighter, fighter.previousPosition);
            if (collision == CollisionResult::KnockedOut)
            {
                ctx.stockFlash[player] = 0.35f;
                HandleKnockout(ctx.stage, fighter, player == 0 ? 340.0F : 620.0F, player == 0 ? 1 : -1);
            }
        }

        ResolveCombat(ctx.fighters[0], ctx.fighters[1]);
        ctx.accumulator -= static_cast<double>(kFixedDt);
    }

    UpdateCamera(ctx.camera, ctx.fighters, frameTime);
    if (ctx.fighters[0].damageShake > 0.0F || ctx.fighters[1].damageShake > 0.0F)
    {
        const float shake = std::sin(static_cast<float>(GetTime()) * 90.0F) * 3.0F;
        ctx.camera.camera.target.y += shake;
    }
}

void RenderWorld(const GameContext& ctx)
{
    ClearBackground(Color{ 15, 19, 31, 255 });
    Camera2D view = ctx.camera.camera;
    constexpr float fieldHeight = UITheme::VIRTUAL_HEIGHT - HudBandHeight;
    view.offset.y = fieldHeight * 0.5f;
    view.zoom *= fieldHeight / UITheme::VIRTUAL_HEIGHT;
    BeginMode2D(view);
    DrawStage(ctx.stage);
    for (const Fighter& fighter : ctx.fighters)
    {
        DrawFighterSprite(ctx.sprites, fighter);
    }
    if (ctx.debugMode)
    {
        for (const Fighter& fighter : ctx.fighters)
        {
            DrawDebugFighter(fighter);
        }
    }
    EndMode2D();
}

void RenderInterface(const GameContext& ctx)
{
    const float dt = GetFrameTime();
    if (ctx.scene == SceneState::CharacterSelect) { DrawCharacterSelect(ctx); return; }
    if (ctx.scene == SceneState::InGame || ctx.scene == SceneState::Paused)
    {
        DrawHud(ctx.sprites, ctx.stockFlash, ctx.fighters, ctx.opponentMode, ctx.debugMode, static_cast<float>(GetFPS()));

        // Icono cuadrado discreto de PAUSE en la esquina superior derecha
        const Rectangle pauseIconRect{ 914.0F, 12.0F, 34.0F, 34.0F };
        const_cast<GameContext&>(ctx).menuRenderer.DrawSquareButton(SquareButtonKind::Pause, pauseIconRect, WHITE, true);
        const std::string pauseHint = "[" + GetKeyDisplayName(GetControlKey(ControlAction::Pause)) + "]";
        UIComponents::DrawTextModern(pauseHint.c_str(), 931.0F, 52.0F, UITheme::FONT_SIZE_SMALL, ColorAlpha(UITheme::COLOR_TEXT_MUTED, 0.85F), true, true);

        if (!ctx.fighters[0].alive || !ctx.fighters[1].alive)
        {
            DrawRectangle(0, 195, UITheme::VIRTUAL_WIDTH, 90, ColorAlpha(UITheme::COLOR_BG_BASE, 0.88F));
            UIComponents::DrawTextModern("K.O.", 480.0F, 220.0F, UITheme::FONT_SIZE_TITLE, UITheme::COLOR_ACCENT_MAIN, true, true);
            const char* const restartPrompt = (UITheme::s_language == UITheme::Language::Spanish)
                ? "PRESIONA [R] PARA REINICIAR COMBATE"
                : "PRESS [R] TO RESTART MATCH";
            UIComponents::DrawTextModern(restartPrompt, 480.0F, 258.0F, UITheme::FONT_SIZE_BODY, Color{ 255, 255, 255, 255 }, true, false);
        }
    }

    // 2. Overlay del Menu de Pausa (PAUSED)
    if (ctx.scene == SceneState::Paused)
    {
        // Overlay #0A0612 al 75% sobre el juego congelado + vineta suave
        DrawRectangle(0, 0, UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT, ColorAlpha(UITheme::COLOR_BG_BASE, 0.64F));
        DrawRectangleGradientH(0, 0, 140, UITheme::VIRTUAL_HEIGHT, ColorAlpha(UITheme::COLOR_BG_BASE, 0.85F), ColorAlpha(UITheme::COLOR_BG_BASE, 0.0F));
        DrawRectangleGradientH(UITheme::VIRTUAL_WIDTH - 140, 0, 140, UITheme::VIRTUAL_HEIGHT, ColorAlpha(UITheme::COLOR_BG_BASE, 0.0F), ColorAlpha(UITheme::COLOR_BG_BASE, 0.85F));

        // Animacion de entrada: escala 0.92 a 1.0 + fade en 120 ms
        const float progress = std::clamp(ctx.pauseEntranceTimer / 0.12F, 0.0F, 1.0F);
        const float modalScale = 0.92F + 0.08F * progress;
        const float modalAlpha = progress;

        constexpr float headerHeight = 80.0F;
        constexpr float buttonHeight = 38.0F;
        constexpr float buttonGap = 10.0F;
        constexpr float bottomPadding = 18.0F;
        const float boxW = 356.0F * modalScale;
        const float boxH = (headerHeight + kPauseOptionCount * buttonHeight +
                            (kPauseOptionCount - 1) * buttonGap + bottomPadding) * modalScale;
        const float boxX = 480.0F - boxW * 0.5F;
        const float boxY = 270.0F - boxH * 0.5F;

        // Panel centrado con borde cian y esquinas cortadas
        const Color panelFill = ColorAlpha(UITheme::COLOR_BG_BASE, 0.98F * modalAlpha);
        const Color panelBorder = ColorAlpha(UITheme::COLOR_ACCENT_CYAN, modalAlpha);
        UIComponents::DrawCutCornerRect({ boxX, boxY, boxW, boxH }, 10.0F, panelFill, panelBorder, 2.0F);

        DrawRectangleRec({boxX + 16, boxY, boxW - 32, 3}, UITheme::COLOR_ACCENT_MAIN);
        DrawRectangleRec({boxX + 20, boxY + 57 * modalScale, boxW - 40, 1}, UITheme::EDGE);
        // Titulo de pausa
        const char* const pausedTitle = (UITheme::s_language == UITheme::Language::Spanish) ? "PAUSA" : "PAUSED";
        UIComponents::DrawTextModern(pausedTitle, 480.0F, boxY + 16.0F * modalScale,
                                     UITheme::FONT_SIZE_TITLE * modalScale, UITheme::COLOR_ACCENT_CYAN, true, true);

        static constexpr const char* pauseLabelsEs[kPauseOptionCount] = { "REANUDAR", "MENÚ", "OPCIONES", "CAMBIAR LUCHADOR" };
        static constexpr const char* pauseLabelsEn[kPauseOptionCount] = { "RESUME", "MENU", "OPTIONS", "CHANGE FIGHTER" };
        const auto& pauseLabels = UITheme::s_language == UITheme::Language::Spanish ? pauseLabelsEs : pauseLabelsEn;
        GameContext& game = const_cast<GameContext&>(ctx);
        const int clicked = UIComponents::DrawSlidingButtonColumn(
            pauseLabels, kPauseOptionCount, 480.0F, boxY + (headerHeight + buttonHeight / 2) * modalScale,
            276.0F * modalScale, buttonHeight * modalScale, (buttonHeight + buttonGap) * modalScale,
            game.pauseMenuSelection, dt, game.pauseMenuSliding);

        if (clicked >= 0 && !(UIComponents::g_transition.active && UIComponents::g_transition.fadingOut))
        {
            game.pauseMenuSelection = clicked;
            ActivatePauseOption(game);
        }
    }

    // 3. Pantalla del Menu Principal (MAIN_MENU)
    if (ctx.scene == SceneState::MainMenu)
    {

        // Titulo del juego: Logo "FIGHTERXD" con contorno grueso, degradado y brillo
        UIComponents::DrawLogoFighterXD(480.0f, 86.0f);

        UIComponents::DrawTextModern(UITheme::GetSubtitleText(), 480.0f, 126.0f, UITheme::FONT_SIZE_BODY, UITheme::COLOR_TEXT_MAIN, true, false);

        // --- Columna con cursor unico deslizante (ease-out) y feedback de 80 ms ---
        const char* const menuLabelsEs[4] = { "JUGAR", "CONTROLES", "OPCIONES", "SALIR" };
        const char* const menuLabelsEn[4] = { "PLAY", "CONTROLS", "OPTIONS", "QUIT" };
        const char* const* const menuLabels = (UITheme::s_language == UITheme::Language::Spanish) ? menuLabelsEs : menuLabelsEn;

        const float startY = 220.0F;
        const float spacing = UITheme::BUTTON_HEIGHT + UITheme::BUTTON_SPACING; // 56.0F

        const int clicked = UIComponents::DrawSlidingButtonColumn(
            menuLabels, 4, 480.0F, startY, 274.0F, UITheme::BUTTON_HEIGHT,
            spacing, const_cast<GameContext&>(ctx).mainMenuSelection, dt, const_cast<GameContext&>(ctx).slidingMenu);

        if (clicked >= 0)
        {
            const_cast<GameContext&>(ctx).mainMenuSelection = clicked;
            if (clicked == 0) // JUGAR / PLAY
            {
                OpenCharacterSelect(const_cast<GameContext&>(ctx));
            }
            else if (clicked == 1) // CONTROLES / CONTROLS
            {
                UIComponents::g_transition.Start(static_cast<int>(SceneState::Controls));
            }
            else if (clicked == 2) // OPCIONES / OPTIONS
            {
                UIComponents::g_transition.Start(static_cast<int>(SceneState::Options));
                const_cast<GameContext&>(ctx).optionsSelection = 0;
                const_cast<GameContext&>(ctx).optionsReturnScene = SceneState::MainMenu;
            }
            else if (clicked == 3) // SALIR / QUIT
            {
                const_cast<GameContext&>(ctx).shouldQuit = true;
            }
        }

        // Barra inferior de atajos
        const char* const mainHelp = (UITheme::s_language == UITheme::Language::Spanish)
            ? "[ARRIBA/ABAJO] NAVEGAR    [ENTER] SELECCIONAR    [ESC] SALIR"
            : "[UP/DOWN] NAVIGATE    [ENTER] SELECT    [ESC] QUIT";
        UIComponents::DrawHelpBar(mainHelp, UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT);
    }

    // 4. Pantalla de Controles (CONTROLS_SCREEN)
    if (ctx.scene == SceneState::Controls)
    {

        // Titulo de pantalla
        const char* const ctrlTitle = (UITheme::s_language == UITheme::Language::Spanish)
            ? "CONTROLES - JUGADOR 1"
            : "CONTROLS - PLAYER 1";
        UIComponents::DrawTextModern(ctrlTitle, 480.0f, 22.0f, UITheme::FONT_SIZE_TITLE, UITheme::COLOR_ACCENT_CYAN, true, true);

        // Panel principal biselado estilo arcade
        const float cardX = 80.0f;
        const float cardY = 64.0f;
        const float cardW = 800.0f;
        const float cardH = 360.0f;
        UIComponents::DrawCutCornerRect({ cardX, cardY, cardW, cardH }, 10.0f,
                                        ColorAlpha(UITheme::COLOR_BG_BASE, 0.92f),
                                        UITheme::COLOR_ACCENT_CYAN, 2.0f);

        // Linea divisoria central entre columnas
        DrawLineEx({ 480.0f, cardY + 16.0f }, { 480.0f, cardY + cardH - 16.0f }, 1.0f, ColorAlpha(UITheme::COLOR_ACCENT_CYAN, 0.25f));

        const bool spanish = UITheme::s_language == UITheme::Language::Spanish;
        const char* const notesEs[] = {
            "Caminar o iniciar una carrera", "Caminar o iniciar una carrera", "Dirigir ataques hacia arriba",
            "Agacharse o bajar de plataforma", "Salto corto, alto o doble", "Golpes rápidos y aéreos",
            "Ataque Smash", "Recuperación aérea", "Escudo en tierra o esquive aéreo", "Correr con una dirección",
            "Pausar o reanudar el combate"
        };
        const char* const notesEn[] = {
            "Walk or start a dash", "Walk or start a dash", "Aim attacks upward", "Crouch or drop through",
            "Short, full or double jump", "Quick strikes and aerials", "Smash attack", "Aerial recovery",
            "Ground shield or air dodge", "Hold with a direction", "Pause or resume the match"
        };
        for (int column = 0; column < 2; ++column)
        {
            const float x = cardX + 22 + column * 400;
            const int start = column == 0 ? 0 : int(ControlAction::Jump);
            const int end = column == 0 ? int(ControlAction::Jump) : int(ControlAction::Count);
            const char* heading = column == 0 ? (spanish ? "MOVIMIENTO" : "MOVEMENT") : (spanish ? "ACCIONES" : "ACTIONS");
            DrawRectangleRec({x, cardY + 16, 4, 23}, column == 0 ? UITheme::P1_COLOR : UITheme::P2_COLOR);
            UIComponents::DrawTextModern(heading, x + 12, cardY + 16, 21, UITheme::COLOR_TEXT_MAIN, false, true);
            for (int i = start; i < end; ++i)
            {
                const float y = cardY + 48 + (i - start) * (column == 0 ? 66 : 43);
                const auto action = static_cast<ControlAction>(i);
                const auto key = GetKeyDisplayName(GetControlKey(action));
                const float keyWidth = std::max(32.0f, UIComponents::MeasureTextModern(key.c_str(), UITheme::FONT_SIZE_SMALL).x + 16);
                UIComponents::DrawKeycap(key.c_str(), x + 352 - keyWidth, y);
                UIComponents::DrawTextModern(GetControlActionName(action, spanish), x, y, 20, UITheme::COLOR_TEXT_MAIN, false, false);
                UIComponents::DrawTextModern(spanish ? notesEs[i] : notesEn[i], x, y + 21, 16, UITheme::COLOR_TEXT_MUTED, false, false);
            }
        }

        // Boton VOLVER / BACK estandarizado (240x44 px) centrado abajo
        const float backBtnY = 460.0f;
        const char* const backLabel = (UITheme::s_language == UITheme::Language::Spanish) ? "VOLVER" : "BACK";
        const bool clickedBack = UIComponents::DrawSingleArcadeButton(
            backLabel, 480.0f, backBtnY, UITheme::BUTTON_WIDTH, UITheme::BUTTON_HEIGHT,
            true, dt);

        if (clickedBack)
        {
            UIComponents::g_transition.Start(static_cast<int>(SceneState::MainMenu));
        }

        // Barra inferior de atajos
        const char* const ctrlHelp = (UITheme::s_language == UITheme::Language::Spanish)
            ? "[ESC / Z] VOLVER    [ENTER / CLIC] CONFIRMAR"
            : "[ESC / Z] BACK    [ENTER / CLICK] CONFIRM";
        UIComponents::DrawHelpBar(ctrlHelp, UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT);
    }

    if (ctx.scene == SceneState::ControlsConfig)
    {
        const bool isEs = UITheme::s_language == UITheme::Language::Spanish;
        UIComponents::DrawTextModern(isEs ? "CONFIGURAR CONTROLES" : "CONTROL CONFIGURATION", 480, 9,
                                     UITheme::FONT_SIZE_TITLE, UITheme::COLOR_ACCENT_CYAN, true, true);
        UIComponents::DrawTextModern(isEs ? "TECLADO DEL JUGADOR 1" : "PLAYER 1 KEYBOARD", 480, 48,
                                     UITheme::FONT_SIZE_SMALL, UITheme::COLOR_HIGHLIGHT, true, false);
        UIComponents::DrawCutCornerRect({ 104, 72, 752, 356 }, 10, Color{ 14, 10, 22, 244 }, UITheme::COLOR_ACCENT_CYAN, 2);

        const MovementPreset preset = GetControlSettings().preset;
        const char* presetLabels[3] = { "WASD", isEs ? "FLECHAS" : "ARROWS", "CUSTOM" };
        const Rectangle presetRects[3] = { { 266, 78, 130, 34 }, { 415, 78, 130, 34 }, { 564, 78, 130, 34 } };
        for (int i = 0; i < 3; ++i)
        {
            const bool active = static_cast<int>(preset) == i;
            UIComponents::DrawCutCornerRect(presetRects[i], 5, active ? Color{ 38, 20, 58, 255 } : Color{ 16, 10, 26, 220 },
                                            ctx.controlsSelection == 0 ? UITheme::COLOR_ACCENT_MAIN :
                                            (active ? UITheme::COLOR_ACCENT_CYAN : Color{ 68, 55, 88, 255 }),
                                            ctx.controlsSelection == 0 || active ? 2 : 1);
            UIComponents::DrawTextModern(presetLabels[i], presetRects[i].x + presetRects[i].width * 0.5F,
                                         presetRects[i].y + 7, UITheme::FONT_SIZE_SMALL,
                                         active ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MAIN, true, false);
        }

        const int scroll = ctx.controlsScroll;
        for (int row = 0; row < 7; ++row)
        {
            const int actionIndex = scroll + row;
            const Rectangle rowRect{ 118, 122.0F + row * 39.0F, 724, 35 };
            const bool selected = actionIndex + 1 == ctx.controlsSelection;
            const Color bg = selected ? UITheme::FOCUS : UITheme::SURFACE;
            UIComponents::DrawCutCornerRect(rowRect, 4, bg, selected ? UITheme::COLOR_ACCENT_MAIN : UITheme::EDGE, selected ? 2 : 1);
            const auto action = static_cast<ControlAction>(actionIndex);
            UIComponents::DrawTextModern(GetControlActionName(action, isEs), 136, rowRect.y + 8,
                                         UITheme::FONT_SIZE_SMALL, selected ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MAIN, false, false);
            const std::string keyName = GetKeyDisplayName(GetControlKey(action));
            UIComponents::DrawKeycap(keyName.c_str(), 544, rowRect.y + 5);
            UIComponents::DrawTextModern(isEs ? "CAMBIAR" : "CHANGE", 790, rowRect.y + 8,
                                         UITheme::FONT_SIZE_SMALL, selected ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MUTED, true, false);
        }

        const int actionCount = static_cast<int>(ControlAction::Count);
        const int indices[3] = { actionCount + 1, actionCount + 2, actionCount + 3 };
        const char* labels[3] = { isEs ? "GUARDAR" : "SAVE", isEs ? "REINICIAR" : "RESET DEFAULT", isEs ? "VOLVER" : "BACK" };
        const Rectangle buttons[3] = { { 215, 432, 160, 40 }, { 400, 432, 190, 40 }, { 615, 432, 130, 40 } };
        for (int i = 0; i < 3; ++i)
        {
            const bool selected = ctx.controlsSelection == indices[i];
            const auto state = ctx.controlsCapturing ? UIComponents::ButtonState::Disabled :
                               (selected ? UIComponents::ButtonState::Focused : UIComponents::ButtonState::Normal);
            UIComponents::DrawButtonSurface(buttons[i], state);
            UIComponents::DrawTextModern(labels[i], buttons[i].x + buttons[i].width * 0.5F, buttons[i].y + 9,
                                         UITheme::FONT_SIZE_SMALL, ctx.controlsCapturing ? UITheme::COLOR_TEXT_MUTED : selected ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MAIN, true, false);
        }

        if (!ctx.controlsMessage.empty() && (ctx.controlsMessageTimer > 0.0F || ctx.controlsCapturing))
        {
            UIComponents::DrawCutCornerRect({ 220, 478, 520, 30 }, 5, Color{ 30, 14, 38, 245 }, UITheme::COLOR_HIGHLIGHT, 1);
            UIComponents::DrawTextModern(ctx.controlsMessage.c_str(), 480, 484, UITheme::FONT_SIZE_SMALL,
                                         UITheme::COLOR_HIGHLIGHT, true, false);
            if (ctx.controlsCapturing && ctx.controlsConflictAction >= 0)
            {
                const std::string choices = isEs ? "[ENTER] INTERCAMBIAR   [R] OTRA TECLA   [C / ESC] CANCELAR"
                                                  : "[ENTER] SWAP   [R] CHOOSE ANOTHER   [C / ESC] CANCEL";
                UIComponents::DrawTextModern(choices.c_str(), 480, 508, UITheme::FONT_SIZE_SMALL, UITheme::COLOR_TEXT_MAIN, true, false);
            }
        }
        else
        {
            const char* hint = isEs ? "[ARRIBA/ABAJO] NAVEGAR   [ENTER] EDITAR   [ESC] VOLVER"
                                    : "[UP/DOWN] NAVIGATE   [ENTER] EDIT   [ESC] BACK";
            UIComponents::DrawHelpBar(hint, UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT);
        }
    }

    // 5. Pantalla de Opciones (OPTIONS_SCREEN)
    if (ctx.scene == SceneState::Options)
    {

        // Titulo principal y subtitulo claros y legibles
        const char* const optTitle = (UITheme::s_language == UITheme::Language::Spanish) ? "OPCIONES" : "OPTIONS";
        const char* const optSub = (UITheme::s_language == UITheme::Language::Spanish)
            ? "PREFERENCIAS DE JUEGO Y PANTALLA"
            : "GAMEPLAY & DISPLAY PREFERENCES";
        UIComponents::DrawTextModern(optTitle, 480.0F, 9.0F, UITheme::FONT_SIZE_TITLE, UITheme::COLOR_ACCENT_CYAN, true, true);
        UIComponents::DrawTextModern(optSub, 480.0F, 48.0F, UITheme::FONT_SIZE_SMALL, UITheme::COLOR_HIGHLIGHT, true, false);

        // Panel central con esquinas cortadas (780 x 340 px)
        const float cardW = 780.0F;
        const float cardH = 360.0F;
        const float cardX = 480.0F - cardW * 0.5F;
        const float cardY = 72.0F;
        UIComponents::DrawCutCornerRect(Rectangle{ cardX, cardY, cardW, cardH }, 12.0F,
                                        UITheme::PANEL, UITheme::COLOR_ACCENT_CYAN, 2.0F);

        const Vector2 mpos = UITheme::GetVirtualMousePosition();
        const bool mClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        struct OptionRowDef
        {
            const char* title;
            const char* desc;
            std::string value;
            Color valColor;
        };

        const bool isEs = (UITheme::s_language == UITheme::Language::Spanish);

        // Textos del modo de oponente
        std::string oppText;
        if (ctx.opponentMode == OpponentMode::DummyQuiet)
        {
            oppText = isEs ? "< INMOVIL >" : "< DUMMY (STAND) >";
        }
        else if (ctx.opponentMode == OpponentMode::CpuBasic)
        {
            oppText = isEs ? "< CPU BASICA >" : "< BASIC CPU >";
        }
        else
        {
            oppText = isEs ? "< DI + ESCUDO >" : "< DUMMY (DI + SHIELD) >";
        }
        const Color oppColor = (ctx.opponentMode == OpponentMode::CpuBasic) ? UITheme::COLOR_ACCENT_CYAN :
                               ((ctx.opponentMode == OpponentMode::DummyQuiet) ? Color{ 255, 255, 255, 255 } : UITheme::COLOR_HIGHLIGHT);

        // Textos de debug
        const std::string debugText = isEs
            ? (ctx.debugMode ? "[ ACTIVADO ]" : "[ DESACTIVADO ]")
            : (ctx.debugMode ? "[ ON ]" : "[ OFF ]");

        // Textos de escalado
        std::string scalingText;
        if (UITheme::s_scalingMode == UITheme::ScalingMode::FitSmooth)
        {
            scalingText = isEs ? "< AJUSTAR (SUAVE) >" : "< FIT (SMOOTH) >";
        }
        else
        {
            scalingText = isEs ? "< PIXEL-PERFECT (ENTERO) >" : "< PIXEL-PERFECT (INTEGER) >";
        }

        // Textos de pantalla completa
        const std::string displayVal = isEs
            ? (IsWindowFullscreen() ? "[ PANTALLA COMPLETA ]" : "[ VENTANA ]")
            : (IsWindowFullscreen() ? "[ FULLSCREEN ]" : "[ WINDOWED ]");

        // Fila 4: Idioma
        const std::string langVal = isEs ? "< ESPAÑOL >" : "< ENGLISH >";

        const OptionRowDef rows[6] = {
            {
                isEs ? "MODO OPONENTE" : "OPPONENT MODE",
                isEs ? "Elige cómo responde el rival." : "Choose how the opponent responds.",
                oppText,
                oppColor
            },
            {
                isEs ? "INFORMACION DEBUG" : "DEBUG OVERLAY",
                isEs ? "Muestra colisiones y datos de combate (F1)." : "Show collisions and combat data (F1).",
                debugText,
                ctx.debugMode ? UITheme::COLOR_ACCENT_CYAN : Color{ 250, 245, 255, 255 }
            },
            {
                isEs ? "MODO DE ESCALADO" : "SCALING MODE",
                isEs ? "Ajusta cómo se escala el escenario." : "Choose how the stage scales.",
                scalingText,
                UITheme::COLOR_ACCENT_CYAN
            },
            {
                isEs ? "MODO DE PANTALLA" : "DISPLAY MODE",
                isEs ? "Alternar Pantalla Completa / Ventana (F11)" : "Toggle Fullscreen / Windowed display mode (F11)",
                displayVal,
                IsWindowFullscreen() ? UITheme::COLOR_HIGHLIGHT : Color{ 250, 245, 255, 255 }
            },
            {
                isEs ? "IDIOMA / LANGUAGE" : "LANGUAGE / IDIOMA",
                isEs ? "Seleccionar idioma de la interfaz (Espanol / Ingles)" : "Select interface language (Spanish / English)",
                langVal,
                UITheme::COLOR_HIGHLIGHT
            },
            {
                isEs ? "CONTROLES" : "CONTROLS",
                isEs ? "Preset de movimiento y reasignacion de teclas" : "Movement presets and keyboard rebinding",
                isEs ? "[ CONFIGURAR ]" : "[ CONFIGURE ]",
                UITheme::COLOR_ACCENT_CYAN
            }
        };

        static Vector2 s_lastOptMousePos = { -999.0F, -999.0F };
        const Vector2 curScreenMouse = GetMousePosition();
        const bool optMouseMoved = (std::abs(curScreenMouse.x - s_lastOptMousePos.x) > 2.0F ||
                                    std::abs(curScreenMouse.y - s_lastOptMousePos.y) > 2.0F);
        if (optMouseMoved)
        {
            s_lastOptMousePos = curScreenMouse;
        }

        const float rowW = cardW - 28.0F;
        const float rowX = cardX + 14.0F;
        const float rowH = 54.0F;

        for (int i = 0; i < 6; ++i)
        {
            const float rowY = cardY + 8.0F + static_cast<float>(i) * 58.0F;
            const Rectangle rowRec = { rowX, rowY, rowW, rowH };
            const bool hovered = CheckCollisionPointRec(mpos, rowRec);

            if (hovered && optMouseMoved)
            {
                const_cast<GameContext&>(ctx).optionsSelection = i;
            }

            if (hovered && mClick)
            {
                const_cast<GameContext&>(ctx).optionsSelection = i;
                UITheme::AudioManager::Instance().PlayConfirm();
                if (i == 0) CycleOpponentMode(const_cast<GameContext&>(ctx));
                else if (i == 1) const_cast<GameContext&>(ctx).debugMode = !ctx.debugMode;
                else if (i == 2)
                {
                    UITheme::s_scalingMode = (UITheme::s_scalingMode == UITheme::ScalingMode::FitSmooth)
                        ? UITheme::ScalingMode::PixelPerfect
                        : UITheme::ScalingMode::FitSmooth;
                }
                else if (i == 3)
                {
                    ToggleFullscreen();
                    SetWindowFocused();
                    ForceWindowActive(GetWindowHandle());
                }
                else if (i == 4)
                {
                    UITheme::s_language = (UITheme::s_language == UITheme::Language::Spanish)
                        ? UITheme::Language::English
                        : UITheme::Language::Spanish;
                }
                else if (i == 5)
                {
                    OpenControls(const_cast<GameContext&>(ctx));
                }
            }

            const bool isSelected = (ctx.optionsSelection == i);
            const Color rowBg = isSelected ? UITheme::FOCUS : UITheme::SURFACE;
            const Color rowBorder = isSelected ? UITheme::COLOR_ACCENT_MAIN : UITheme::EDGE;

            UIComponents::DrawCutCornerRect(rowRec, 6.0F, rowBg, rowBorder, isSelected ? 2.0F : 1.0F);

            // Left: Title and description (claros, definidos y legibles)
            UIComponents::DrawTextModern(rows[i].title, rowX + 20.0F, rowY + 7.0F, UITheme::FONT_SIZE_BODY,
                                         isSelected ? UITheme::COLOR_HIGHLIGHT : UITheme::COLOR_TEXT_MAIN, false, false);
            UIComponents::DrawTextModern(rows[i].desc, rowX + 20.0F, rowY + 29.0F, UITheme::FONT_SIZE_SMALL,
                                         UITheme::COLOR_TEXT_MUTED, false, false);

            // Right: Value string
            const Vector2 valSize = UIComponents::MeasureTextModern(rows[i].value.c_str(), UITheme::FONT_SIZE_BODY);
            const float valX = rowX + rowW - 20.0F - valSize.x;
            const float valY = rowY + (rowH - valSize.y) * 0.5F;
            DrawRectangleRec({valX - 12, rowY + 11, 1, rowH - 22}, UITheme::EDGE);
            UIComponents::DrawTextModern(rows[i].value.c_str(), valX, valY, UITheme::FONT_SIZE_BODY, rows[i].valColor, false, false);
        }

        // Boton VOLVER / BACK estandarizado (240x40 px) centrado abajo
        const float backBtnY = 478.0F;
        const float backBtnW = UITheme::BUTTON_WIDTH;
        const float backBtnH = 40.0F;
        const Rectangle backBtnRec = { 480.0F - backBtnW * 0.5F, backBtnY - backBtnH * 0.5F, backBtnW, backBtnH };
        const bool backHovered = CheckCollisionPointRec(mpos, backBtnRec);
        if (backHovered && optMouseMoved)
        {
            const_cast<GameContext&>(ctx).optionsSelection = 6;
        }

        const bool backSelected = (ctx.optionsSelection == 6);
        const char* const backLabel = isEs ? "VOLVER" : "BACK";
        const bool clickedBack = UIComponents::DrawSingleArcadeButton(
            backLabel, 480.0F, backBtnY, backBtnW, backBtnH,
            backSelected, dt);

        if (clickedBack || (backHovered && mClick))
        {
            UITheme::AudioManager::Instance().PlayCancel();
            UIComponents::g_transition.Start(static_cast<int>(ctx.optionsReturnScene));
        }

        // Barra inferior de atajos
        const char* const optHelp = isEs
            ? "[ARRIBA/ABAJO] NAVEGAR    [IZQ/DER / CLIC] CAMBIAR    [ESC / Z] VOLVER"
            : "[UP/DOWN] NAVIGATE    [LEFT/RIGHT / CLICK] CHANGE    [ESC / Z] BACK";
        UIComponents::DrawHelpBar(optHelp, UITheme::VIRTUAL_WIDTH, UITheme::VIRTUAL_HEIGHT);
    }
}
