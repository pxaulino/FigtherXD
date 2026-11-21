#include "if/character_select.hpp"
#include "gm/game.hpp"
#include <algorithm>
#include <cmath>

namespace
{
constexpr Rectangle back{216,422,218,44};
constexpr Rectangle confirm{454,422,290,44};

Rectangle Card(int i) { return {float(144 + i * 352), 96, 320, 296}; }

void Highlight(GameContext& game, Character choice)
{
    if (game.highlighted == choice) return;
    game.highlighted = choice;
    game.selectionPulse = 1;
    UITheme::AudioManager::Instance().PlayCursor();
}

void TransitionTo(SceneState scene, bool flash = false)
{
    auto& transition = UIComponents::g_transition;
    transition = {};
    transition.Start(static_cast<int>(scene), flash);
}
}

void OpenCharacterSelect(GameContext& game)
{
    game.selectReturn = game.scene;
    game.highlighted = game.selected;
    game.confirmed = false;
    game.stickLatched = false;
    game.selectionPulse = 1;
    TransitionTo(SceneState::CharacterSelect);
}

void UpdateCharacterSelect(GameContext& game)
{
    game.selectionPulse *= std::exp(-8 * GetFrameTime());
    if (UIComponents::g_transition.active && UIComponents::g_transition.fadingOut) return;
    const bool pad = IsGamepadAvailable(0);
    const auto button = [pad](int b) { return pad && IsGamepadButtonPressed(0,b); };
    const float axis = pad ? GetGamepadAxisMovement(0,GAMEPAD_AXIS_LEFT_X) : 0;
    if (std::abs(axis) < 0.32f) game.stickLatched = false;
    if (IsKeyPressed(KEY_LEFT) || button(GAMEPAD_BUTTON_LEFT_FACE_LEFT) || (!game.stickLatched && axis < -0.6f))
        Highlight(game, Character::PackMAN);
    if (IsKeyPressed(KEY_RIGHT) || button(GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || (!game.stickLatched && axis > 0.6f))
        Highlight(game, Character::Shadow);
    if (std::abs(axis) > 0.6f) game.stickLatched = true;
    const auto mouse = UITheme::GetVirtualMousePosition();
    const bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (click)
        for (int i = 0; i < int(Roster.size()); ++i)
            if (CheckCollisionPointRec(mouse, Card(i))) Highlight(game, static_cast<Character>(i));

    if (IsKeyPressed(KEY_ESCAPE) || button(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || (click && CheckCollisionPointRec(mouse,back)))
    {
        UITheme::AudioManager::Instance().PlayCancel();
        TransitionTo(game.selectReturn);
    }
    else if (IsKeyPressed(KEY_ENTER) || button(GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || (click && CheckCollisionPointRec(mouse,confirm)))
    {
        game.selected = game.highlighted;
        game.confirmed = true;
        ResetMatch(game);
        UITheme::AudioManager::Instance().PlayStart();
        TransitionTo(SceneState::InGame, true);
    }
}

void DrawCharacterSelect(const GameContext& game)
{
    using namespace UITheme;
    using namespace UIComponents;
    const bool es = s_language == Language::Spanish;
    DrawTextModern(es ? "SELECCIONAR LUCHADOR" : "CHOOSE YOUR FIGHTER",480,24,FONT_SIZE_TITLE,COLOR_TEXT_MAIN,true,true);
    for (int i = 0; i < int(Roster.size()); ++i)
    {
        const auto& hero = Roster[i];
        const bool focused = i == int(game.highlighted);
        Rectangle r = Card(i);
        const float grow = focused ? game.selectionPulse * 4 : 0;
        r = {r.x-grow,r.y-grow,r.width+2*grow,r.height+2*grow};
        const float cx = r.x+r.width/2;
        DrawCutCornerRect(r,13,PANEL,focused ? hero.accent : EDGE, focused ? 2 + grow/2 : 1);
        DrawRectangleRec({r.x+16,r.y,r.width-32,3}, ColorAlpha(hero.accent,focused ? 1.0f : 0.35f));
        DrawCircleGradient({cx,238},95,ColorAlpha(hero.accent,focused ? 0.26f : 0.1f),ColorAlpha(hero.accent,0));
        if (focused) DrawTextModern("P1",r.x+18,r.y+15,21,P1_COLOR,false,true);
        if (i == int(Character::Shadow)) DrawTextModern("CPU",r.x+r.width-45,r.y+15,21,P2_COLOR,true,true);
        DrawEllipse(int(cx),305,68,9,ColorAlpha(hero.accent,0.22f));
        game.sprites.Draw(i,SpriteAnimation::Idle,game.menuAnimFrame,true,i == 0 ? 1 : -1,
                          {cx-64-grow,142-grow,128+2*grow,162+grow},305);
        DrawTextModern(hero.name,cx,327,32,focused ? COLOR_TEXT_MAIN : COLOR_TEXT_MUTED,true,true);
    }
    const auto mouse = GetVirtualMousePosition();
    const auto draw = [&](Rectangle r,const char* label,bool active) {
        DrawButtonSurface(r,active || CheckCollisionPointRec(mouse,r) ? ButtonState::Focused : ButtonState::Normal);
        DrawTextModern(label,r.x+r.width/2,r.y+9,FONT_SIZE_BUTTON,COLOR_TEXT_MAIN,true,false);
    };
    draw(back,es ? "VOLVER" : "BACK",false);
    draw(confirm,game.confirmed ? (es ? "¡LISTO!" : "READY!") : (es ? "CONFIRMAR" : "CONFIRM"),true);
    DrawHelpBar(es ? "[IZQ / DER] ELEGIR   [ENTER] CONFIRMAR   [ESC] VOLVER" :
                     "[LEFT / RIGHT] CHOOSE   [ENTER] CONFIRM   [ESC] BACK");
}
