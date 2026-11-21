#if __has_include("cm/camera.hpp")
#include "cm/camera.hpp"
#else
#include "camera.hpp"
#endif

#include <algorithm>
#include <cmath>

void InitializeCamera(CompetitiveCamera& camera)
{
    camera.target = Vector2{ 480.0F, 300.0F };
    camera.zoom = 1.0F;
    camera.camera = Camera2D{ Vector2{ 480.0F, 270.0F }, camera.target, 0.0F, camera.zoom };
}

void UpdateCamera(CompetitiveCamera& camera, const std::array<Fighter, 2>& fighters, float dt)
{
    const Fighter& first = fighters[0];
    const Fighter& second = fighters[1];
    const float midpointX = (first.position.x + second.position.x) * 0.5F;
    // position = pies; se resta la altura estandar para centrar la camara en el torso
    const float midpointY = (first.position.y + second.position.y) * 0.5F - 110.0F;
    const float separation = std::abs(first.position.x - second.position.x);
    const float desiredZoom = std::clamp(960.0F / (separation + 360.0F), 0.70F, 1.12F);
    const float predictX = (first.velocity.x + second.velocity.x) * 0.06F;
    const Vector2 desired{ std::clamp(midpointX + predictX, 320.0F, 640.0F),
                           std::clamp(midpointY - 90.0F, 140.0F, 470.0F) };
    const float smoothing = 1.0F - std::exp(-5.5F * std::max(dt, 0.0F));
    camera.target.x += (desired.x - camera.target.x) * smoothing;
    camera.target.y += (desired.y - camera.target.y) * smoothing;
    camera.zoom += (desiredZoom - camera.zoom) * smoothing;
    camera.camera.offset = Vector2{ 480.0F, 270.0F }; // Centro exacto del lienzo virtual de 960x540
    camera.camera.target = camera.target;
    camera.camera.zoom = camera.zoom;
}
