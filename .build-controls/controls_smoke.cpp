#include "in/controls.hpp"
#include <raylib.h>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "save";
    if (mode == "save") {
        LoadControlSettings();
        ResetControlSettings();
        const int light = GetControlKey(ControlAction::LightAttack);
        SetMovementPreset(MovementPreset::Arrows);
        assert(GetControlKey(ControlAction::MoveLeft) == KEY_LEFT);
        assert(GetControlKey(ControlAction::LightAttack) == light);
        GetControlSettings().keys[(int)ControlAction::LightAttack] = KEY_Q;
        SetMovementPreset(MovementPreset::Custom);
        assert(SaveControlSettings());
    } else if (mode == "load") {
        LoadControlSettings();
        assert(GetControlSettings().preset == MovementPreset::Custom);
        assert(GetControlKey(ControlAction::MoveLeft) == KEY_LEFT);
        assert(GetControlKey(ControlAction::LightAttack) == KEY_Q);
        GetControlSettings().keys[(int)ControlAction::MoveRight] = KEY_Q;
        assert(!SaveControlSettings());
    } else if (mode == "corrupt") {
        const char* root = std::getenv("LOCALAPPDATA");
        assert(root);
        const auto path = std::filesystem::path(root) / "FighterXD" / "config" / "controls.json";
        std::ofstream(path, std::ios::trunc) << "{broken";
        LoadControlSettings();
        assert(GetControlSettings().preset == MovementPreset::Wasd);
        assert(GetControlKey(ControlAction::MoveLeft) == KEY_A);
        assert(GetControlKey(ControlAction::LightAttack) == KEY_J);
    }
    return 0;
}