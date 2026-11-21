#pragma once

#include <array>
#include <string>
#include <raylib.h>

#include "cm/camera.hpp"
#include "ft/fighter.hpp"
#include "if/hud.hpp"
#include "if/menu_renderer.hpp"
#include "if/sprite_renderer.hpp"
#include "in/controls.hpp"
#include "lb/combat.hpp"
#include "mp/stage.hpp"
#include "ui/ui_components.hpp"

enum class SceneState
{
    MainMenu,
    CharacterSelect,
    Controls,
    Options,
    ControlsConfig,
    InGame,
    Paused
};

// Estado de entrada del Jugador 1. Los eventos (pulsaciones) se acumulan en cada frame de
// render y se consumen en el siguiente tick fisico real de 60 Hz; los contadores solo
// avanzan en ticks fisicos.
struct PlayerInputState
{
    bool leftPressed = false;
    bool rightPressed = false;
    bool jumpPressed = false;
    bool jumpReleased = false;
    bool attackPressed = false;
    bool heavyPressed = false;
    bool specialPressed = false;
    bool shieldPressed = false;
    bool downReleased = false;

    int lastHorizDir = 0;
    int heldDir = 0;
    int horizHoldFrames = 0;
    int lastTapDir = 0;
    int ticksSinceTap = 999;
    bool dashLatch = false;
    int downHoldFrames = 0;
};

struct GameContext
{
    SceneState scene = SceneState::MainMenu;
    int mainMenuSelection = 0;   // 0: Play, 1: Controls, 2: Options, 3: Quit
    int optionsSelection = 0;    // 0: Modo CPU, 1: Modo Debug, 2: Back
    SceneState optionsReturnScene = SceneState::MainMenu;
    int controlsSelection = 0;
    int controlsScroll = 0;
    bool controlsCapturing = false;
    ControlSettings controlsOriginal{};
    int controlsCaptureAction = -1;
    int controlsConflictAction = -1;
    int controlsConflictKey = KEY_NULL;
    std::string controlsMessage{};
    float controlsMessageTimer = 0.0F;
    int pauseMenuSelection = 0;
    float pauseEntranceTimer = 0.0f; // Animacion de entrada 120 ms
    OpponentMode opponentMode = OpponentMode::DummyQuiet;
    bool debugMode = false;
    bool shouldQuit = false;

    Character selected = Character::PackMAN;
    Character highlighted = Character::PackMAN;
    SceneState selectReturn = SceneState::MainMenu;
    float selectionPulse = 0;
    bool stickLatched = false;
    bool confirmed = false;
    std::array<float,2> stockFlash{};
    Stage stage{};
    std::array<Fighter, 2> fighters{};
    CompetitiveCamera camera{};
    SpriteRenderer sprites{};
    MenuRenderer menuRenderer{};
    PlayerInputState p1Input{};
    int menuAnimFrame = 0;
    UIComponents::SlidingMenuState slidingMenu{};
    UIComponents::SlidingMenuState pauseMenuSliding{};
    double accumulator = 0.0;
};

void InitGame(GameContext& ctx);
void ResetMatch(GameContext& ctx);
void UpdateGame(GameContext& ctx);
void RenderWorld(const GameContext& ctx);
void RenderInterface(const GameContext& ctx);
void ShutdownGame(GameContext& ctx);
