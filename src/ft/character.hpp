#pragma once

#include <array>
#include <raylib.h>

enum class Character { PackMAN, Shadow };

struct CharacterInfo
{
    const char* name;
    const char* spriteDirectory;
    Color accent;
};

inline constexpr std::array<CharacterInfo, 2> Roster{{
    {"PackMAN", "Martial Hero/Sprites", {0,229,255,255}},
    {"Shadow", "Martial Hero 2/Sprites", {255,46,99,255}}
}};

inline constexpr const CharacterInfo& Describe(Character character)
{
    return Roster[static_cast<unsigned>(character)];
}
