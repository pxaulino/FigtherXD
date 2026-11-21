#pragma once

#include <array>
#include <string>

enum class ControlAction : int
{
    MoveLeft, MoveRight, AimUp, AimDown, Jump, LightAttack, HeavyAttack,
    Special, Dodge, Run, Grab, Pause, Count
};

enum class MovementPreset : int { Wasd, Arrows, Custom };

struct ControlSettings
{
    MovementPreset preset = MovementPreset::Wasd;
    std::array<int, static_cast<int>(ControlAction::Count)> keys{};
};

void LoadControlSettings();
bool SaveControlSettings();
void ResetControlSettings();
void SetMovementPreset(MovementPreset preset);
ControlSettings& GetControlSettings();
int GetControlKey(ControlAction action);
bool IsControlDown(ControlAction action);
bool IsControlPressed(ControlAction action);
bool IsControlReleased(ControlAction action);
const char* GetControlActionName(ControlAction action, bool spanish = true);
std::string GetKeyDisplayName(int key);
bool IsValidRebindKey(int key);
