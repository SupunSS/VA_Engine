#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Gameplay HUD — rendered only in play mode, deliberately separate from
// EditorUI (which owns editor tooling: scene hierarchy, gizmos, asset
// browser, etc.). Built as an undecorated ImGui overlay: no titlebars, no
// borders, just floating elements positioned at fixed screen locations —
// same technique as EditorUI's DrawStatsOverlay/DrawCullingDebugOverlay.
//
// Deliberately takes plain data (floats, positions, strings) rather than
// Scene&/PhysicsWorld& — main.cpp extracts whatever's needed from the ECS
// and passes it in, keeping this class a pure rendering layer with no ECS
// coupling, matching how EditorUI's own overlay methods are already shaped.
class HUD {
public:
    // One blip on the minimap. Position is world-space XZ (Y ignored) —
    // the minimap projects it relative to the player, north-up (not
    // rotated with player facing), for simplicity in this first version.
    struct MinimapBlip {
        enum class BlipType { Vehicle, Pedestrian };

        glm::vec2 WorldPositionXZ;
        BlipType Type;
    };

    // playerYaw in degrees, matching Camera::GetYaw()'s convention.
    void DrawMinimap(const glm::vec2& playerWorldPositionXZ, float playerYaw,
                      const std::vector<MinimapBlip>& blips, float mapRadiusWorldUnits = 60.0f);

    void DrawHealthBar(float currentHealth, float maxHealth);

    void DrawAmmoCounter(int currentAmmo, int reserveAmmo);

    // insideVehicle gates whether this draws at all — no vehicle, no
    // speedometer, rather than showing a meaningless 0/idle readout.
    void DrawSpeedometer(bool insideVehicle, float speedKmh, float rpm, int gear);

    // Empty promptText means nothing is drawn this frame — main.cpp passes
    // "" whenever there's nothing contextually relevant to prompt (no
    // vehicle nearby, no interactable in range), and the actual prompt
    // string (e.g. "Press F to enter vehicle") otherwise.
    void DrawInteractionPrompt(const std::string& promptText);

private:
    int m_windowWidth = 1280;
    int m_windowHeight = 720;

public:
    // Called once per frame from main.cpp alongside the other HUD draws —
    // needed so screen-space element positions (bottom-center prompt,
    // bottom-right speedometer) stay correctly anchored on window resize.
    void SetViewportSize(int width, int height) { m_windowWidth = width; m_windowHeight = height; }
};