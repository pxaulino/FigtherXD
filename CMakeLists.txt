cmake_minimum_required(VERSION 3.25)

project(PlatformFighterBase LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(PLATFORM RGFW CACHE STRING "" FORCE)
add_subdirectory(libs/raylib)

set(SOURCES
    src/main.cpp
    src/gm/game.cpp
    src/if/character_select.cpp
    src/if/sprite_renderer.cpp
    src/if/hud.cpp
    src/if/menu_renderer.cpp
    src/ft/fighter.cpp
    src/lb/combat.cpp
    src/mp/stage.cpp
    src/cm/camera.cpp
    src/ui/ui_components.cpp
    src/win32_icon.cpp
    src/in/controls.cpp
)

if(WIN32)
    list(APPEND SOURCES app.rc)
endif()

add_executable(PlatformFighterBase WIN32 ${SOURCES})

target_compile_features(PlatformFighterBase PRIVATE cxx_std_17)
target_include_directories(PlatformFighterBase PRIVATE src)
target_link_libraries(PlatformFighterBase PRIVATE raylib)

set_target_properties(PlatformFighterBase PROPERTIES
    OUTPUT_NAME "PlatformFighterBase"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_CURRENT_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Sincronizar automaticamente PlatformFighterBase.exe con FighterXD.exe
add_custom_command(TARGET PlatformFighterBase POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:PlatformFighterBase>"
        "${CMAKE_CURRENT_SOURCE_DIR}/FighterXD.exe"
    COMMENT "Sincronizando binario a FighterXD.exe"
)


if(MSVC)
    target_compile_options(PlatformFighterBase PRIVATE /W4 /permissive-)
elseif(MINGW)
    target_compile_options(PlatformFighterBase PRIVATE -Wall -Wextra -Wpedantic)
    target_link_options(PlatformFighterBase PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
    )
else()
    target_compile_options(PlatformFighterBase PRIVATE -Wall -Wextra -Wpedantic)
endif()
