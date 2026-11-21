#include "in/controls.hpp"

#include <raylib.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <windows.h>
#endif

namespace
{
constexpr int kActionCount = static_cast<int>(ControlAction::Count);
ControlSettings g_settings{};

const std::array<const char*, kActionCount> kFieldNames = {
    "move_left", "move_right", "move_up", "move_down", "jump", "light_attack",
    "heavy_attack", "special_attack", "dodge", "run", "pause"
};

std::filesystem::path SettingsPath()
{
    if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        return std::filesystem::path(localAppData) / "FighterXD" / "config" / "controls.json";
    if (const char* home = std::getenv("HOME"))
        return std::filesystem::path(home) / ".config" / "FighterXD" / "controls.json";
    return std::filesystem::path(GetApplicationDirectory()) / "config" / "controls.json";
}

int DefaultKey(ControlAction action)
{
    switch (action)
    {
    case ControlAction::MoveLeft: return KEY_A;
    case ControlAction::MoveRight: return KEY_D;
    case ControlAction::AimUp: return KEY_W;
    case ControlAction::AimDown: return KEY_S;
    case ControlAction::Jump: return KEY_UP;
    case ControlAction::LightAttack: return KEY_J;
    case ControlAction::HeavyAttack: return KEY_K;
    case ControlAction::Special: return KEY_I;
    case ControlAction::Dodge: return KEY_L;
    case ControlAction::Run: return KEY_LEFT_SHIFT;
    case ControlAction::Pause: return KEY_P;
    default: return KEY_NULL;
    }
}

std::string KeyToken(int key)
{
    switch (key)
    {
    case KEY_SPACE: return "KEY_SPACE";
    case KEY_APOSTROPHE: return "KEY_APOSTROPHE";
    case KEY_COMMA: return "KEY_COMMA";
    case KEY_MINUS: return "KEY_MINUS";
    case KEY_PERIOD: return "KEY_PERIOD";
    case KEY_SLASH: return "KEY_SLASH";
    case KEY_SEMICOLON: return "KEY_SEMICOLON";
    case KEY_EQUAL: return "KEY_EQUAL";
    case KEY_LEFT_BRACKET: return "KEY_LEFT_BRACKET";
    case KEY_BACKSLASH: return "KEY_BACKSLASH";
    case KEY_RIGHT_BRACKET: return "KEY_RIGHT_BRACKET";
    case KEY_GRAVE: return "KEY_GRAVE";
    case KEY_ESCAPE: return "KEY_ESCAPE";
    case KEY_ENTER: return "KEY_ENTER";
    case KEY_TAB: return "KEY_TAB";
    case KEY_BACKSPACE: return "KEY_BACKSPACE";
    case KEY_INSERT: return "KEY_INSERT";
    case KEY_DELETE: return "KEY_DELETE";
    case KEY_RIGHT: return "KEY_RIGHT";
    case KEY_LEFT: return "KEY_LEFT";
    case KEY_DOWN: return "KEY_DOWN";
    case KEY_UP: return "KEY_UP";
    case KEY_PAGE_UP: return "KEY_PAGE_UP";
    case KEY_PAGE_DOWN: return "KEY_PAGE_DOWN";
    case KEY_HOME: return "KEY_HOME";
    case KEY_END: return "KEY_END";
    case KEY_CAPS_LOCK: return "KEY_CAPS_LOCK";
    case KEY_SCROLL_LOCK: return "KEY_SCROLL_LOCK";
    case KEY_NUM_LOCK: return "KEY_NUM_LOCK";
    case KEY_PRINT_SCREEN: return "KEY_PRINT_SCREEN";
    case KEY_PAUSE: return "KEY_PAUSE";
    case KEY_LEFT_SHIFT: return "KEY_LEFT_SHIFT";
    case KEY_LEFT_CONTROL: return "KEY_LEFT_CONTROL";
    case KEY_LEFT_ALT: return "KEY_LEFT_ALT";
    case KEY_LEFT_SUPER: return "KEY_LEFT_SUPER";
    case KEY_RIGHT_SHIFT: return "KEY_RIGHT_SHIFT";
    case KEY_RIGHT_CONTROL: return "KEY_RIGHT_CONTROL";
    case KEY_RIGHT_ALT: return "KEY_RIGHT_ALT";
    case KEY_RIGHT_SUPER: return "KEY_RIGHT_SUPER";
    case KEY_KB_MENU: return "KEY_KB_MENU";
    default:
        if (key >= KEY_ZERO && key <= KEY_NINE) return std::string("KEY_") + static_cast<char>('0' + key - KEY_ZERO);
        if (key >= KEY_A && key <= KEY_Z) return std::string("KEY_") + static_cast<char>('A' + key - KEY_A);
        if (key >= KEY_F1 && key <= KEY_F12) return "KEY_F" + std::to_string(key - KEY_F1 + 1);
        if (key >= KEY_KP_0 && key <= KEY_KP_9) return "KEY_KP_" + std::to_string(key - KEY_KP_0);
        return "KEY_NULL";
    }
}

int TokenKey(const std::string& token)
{
    for (int key = KEY_SPACE; key <= KEY_KB_MENU; ++key)
        if (IsValidRebindKey(key) && KeyToken(key) == token) return key;
    return KEY_NULL;
}

std::string FieldValue(const std::string& source, const std::string& field)
{
    const std::string needle = "\"" + field + "\"";
    const auto pos = source.find(needle);
    if (pos == std::string::npos) return {};
    const auto colon = source.find(':', pos + needle.size());
    const auto quote1 = source.find('"', colon == std::string::npos ? pos : colon + 1);
    if (quote1 == std::string::npos) return {};
    const auto quote2 = source.find('"', quote1 + 1);
    return quote2 == std::string::npos ? std::string{} : source.substr(quote1 + 1, quote2 - quote1 - 1);
}

bool ValidSettings(const ControlSettings& settings)
{
    for (int i = 0; i < kActionCount; ++i)
    {
        if (!IsValidRebindKey(settings.keys[static_cast<std::size_t>(i)])) return false;
        for (int j = 0; j < i; ++j)
            if (settings.keys[static_cast<std::size_t>(i)] == settings.keys[static_cast<std::size_t>(j)] &&
                !AreControlsCompatible(static_cast<ControlAction>(i), static_cast<ControlAction>(j))) return false;
    }
    return true;
}
}

void ResetControlSettings()
{
    for (int i = 0; i < kActionCount; ++i)
        g_settings.keys[static_cast<std::size_t>(i)] = DefaultKey(static_cast<ControlAction>(i));
    g_settings.preset = MovementPreset::Wasd;
}

void LoadControlSettings()
{
    ResetControlSettings();
    std::ifstream file(SettingsPath());
    if (!file) return;
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();
    const auto versionField = json.find("\"version\"");
    const auto versionColon = versionField == std::string::npos ? std::string::npos : json.find(':', versionField);
    if (versionColon == std::string::npos) return;
    const auto versionStart = json.find_first_not_of(" \t\r\n", versionColon + 1);
    if (versionStart == std::string::npos ||
        (json[versionStart] != '1' && json[versionStart] != '2') ||
        (versionStart + 1 < json.size() && std::isdigit(static_cast<unsigned char>(json[versionStart + 1])))) return;
    const int version = json[versionStart] - '0';

    ControlSettings candidate = g_settings;
    const std::string preset = FieldValue(json, "preset");
    if (preset == "ARROWS") candidate.preset = MovementPreset::Arrows;
    else if (preset == "CUSTOM") candidate.preset = MovementPreset::Custom;
    else if (preset == "WASD") candidate.preset = MovementPreset::Wasd;
    else return;

    for (int i = 0; i < kActionCount; ++i)
    {
        const std::string token = FieldValue(json, kFieldNames[static_cast<std::size_t>(i)]);
        if (token.empty()) return;
        candidate.keys[static_cast<std::size_t>(i)] = TokenKey(token);
    }
    if (version == 1 && candidate.keys[static_cast<int>(ControlAction::Jump)] == KEY_SPACE)
    {
        ControlSettings legacyDefaults{};
        for (int i = 0; i < kActionCount; ++i)
            legacyDefaults.keys[static_cast<std::size_t>(i)] = DefaultKey(static_cast<ControlAction>(i));
        legacyDefaults.keys[static_cast<int>(ControlAction::Jump)] = KEY_SPACE;
        const bool arrows = candidate.preset == MovementPreset::Arrows;
        legacyDefaults.keys[static_cast<int>(ControlAction::MoveLeft)] = arrows ? KEY_LEFT : KEY_A;
        legacyDefaults.keys[static_cast<int>(ControlAction::MoveRight)] = arrows ? KEY_RIGHT : KEY_D;
        legacyDefaults.keys[static_cast<int>(ControlAction::AimUp)] = arrows ? KEY_UP : KEY_W;
        legacyDefaults.keys[static_cast<int>(ControlAction::AimDown)] = arrows ? KEY_DOWN : KEY_S;
        if (candidate.keys == legacyDefaults.keys)
            candidate.keys[static_cast<int>(ControlAction::Jump)] = KEY_UP;
    }
    if (ValidSettings(candidate)) g_settings = candidate;
}

bool SaveControlSettings()
{
    if (!ValidSettings(g_settings)) return false;
    std::error_code ec;
    std::filesystem::create_directories(SettingsPath().parent_path(), ec);
    if (ec) return false;
    const std::filesystem::path destination = SettingsPath();
    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (!file) return false;
    const char* preset = g_settings.preset == MovementPreset::Wasd ? "WASD" :
                         g_settings.preset == MovementPreset::Arrows ? "ARROWS" : "CUSTOM";
    file << "{\n  \"version\": 2,\n  \"keyboard\": {\n    \"preset\": \"" << preset << "\",\n";
    for (int i = 0; i < kActionCount; ++i)
        file << "    \"" << kFieldNames[static_cast<std::size_t>(i)] << "\": \""
             << KeyToken(g_settings.keys[static_cast<std::size_t>(i)]) << "\""
             << (i + 1 == kActionCount ? "\n" : ",\n");
    file << "  }\n}\n";
    file.flush();
    if (!file.good()) return false;
    file.close();
#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::error_code cleanupError;
        std::filesystem::remove(temporary, cleanupError);
        return false;
    }
#else
    std::filesystem::rename(temporary, destination, ec);
    if (ec) return false;
#endif
    return true;
}

bool SetMovementPreset(MovementPreset preset)
{
    ControlSettings candidate = g_settings;
    candidate.preset = preset;
    if (preset == MovementPreset::Custom)
    {
        g_settings = candidate;
        return true;
    }
    const bool arrows = preset == MovementPreset::Arrows;
    candidate.keys[static_cast<int>(ControlAction::MoveLeft)] = arrows ? KEY_LEFT : KEY_A;
    candidate.keys[static_cast<int>(ControlAction::MoveRight)] = arrows ? KEY_RIGHT : KEY_D;
    candidate.keys[static_cast<int>(ControlAction::AimUp)] = arrows ? KEY_UP : KEY_W;
    candidate.keys[static_cast<int>(ControlAction::AimDown)] = arrows ? KEY_DOWN : KEY_S;
    if (!ValidSettings(candidate)) return false;
    g_settings = candidate;
    return true;
}

ControlSettings& GetControlSettings() { return g_settings; }
int GetControlKey(ControlAction action) { return g_settings.keys[static_cast<std::size_t>(action)]; }
bool IsControlDown(ControlAction action) { return IsKeyDown(GetControlKey(action)); }
bool IsControlPressed(ControlAction action) { return IsKeyPressed(GetControlKey(action)); }
bool IsControlReleased(ControlAction action) { return IsKeyReleased(GetControlKey(action)); }

const char* GetControlActionName(ControlAction action, bool spanish)
{
    static const char* es[kActionCount] = { "Mover izquierda", "Mover derecha", "Apuntar arriba", "Apuntar abajo", "Saltar", "Ataque ligero", "Ataque fuerte", "Ataque especial", "Esquivar", "Correr", "Pausar" };
    static const char* en[kActionCount] = { "Move left", "Move right", "Aim up", "Aim down", "Jump", "Light attack", "Heavy attack", "Special attack", "Dodge", "Run", "Pause" };
    const int index = static_cast<int>(action);
    return spanish ? es[index] : en[index];
}

std::string GetKeyDisplayName(int key)
{
    std::string token = KeyToken(key);
    if (token.rfind("KEY_", 0) == 0) token.erase(0, 4);
    if (token.size() > 5 && token.rfind("LEFT_", 0) == 0) token = "LEFT " + token.substr(5);
    if (token.size() > 6 && token.rfind("RIGHT_", 0) == 0) token = "RIGHT " + token.substr(6);
    return token;
}

bool IsValidRebindKey(int key)
{
    if (key == KEY_NULL || key == KEY_ESCAPE) return false;
    if ((key >= KEY_SPACE && key <= KEY_KB_MENU) || (key >= KEY_KP_0 && key <= KEY_KP_EQUAL)) return true;
    return false;
}

bool AreControlsCompatible(ControlAction first, ControlAction second)
{
    return (first == ControlAction::Jump && second == ControlAction::AimUp) ||
           (first == ControlAction::AimUp && second == ControlAction::Jump);
}
