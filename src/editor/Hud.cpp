#include "HUD.h"
#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace {
constexpr ImGuiWindowFlags kOverlayFlags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoFocusOnAppearing |
    ImGuiWindowFlags_NoNav |
    ImGuiWindowFlags_NoInputs |
    ImGuiWindowFlags_NoBackground;
}

void HUD::DrawMinimap(const glm::vec2& playerWorldPositionXZ, float playerYaw,
                       const std::vector<MinimapBlip>& blips, float mapRadiusWorldUnits) {
    constexpr float kMapSizePx = 160.0f;
    constexpr float kMargin = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(m_windowWidth) - kMapSizePx - kMargin, kMargin));
    ImGui::SetNextWindowSize(ImVec2(kMapSizePx, kMapSizePx));
    ImGui::SetNextWindowBgAlpha(0.35f);

    ImGui::Begin("##Minimap", nullptr, kOverlayFlags | ImGuiWindowFlags_NoBackground);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 center(origin.x + kMapSizePx * 0.5f, origin.y + kMapSizePx * 0.5f);
    const float radiusPx = kMapSizePx * 0.5f;

    // Circular backing so the map reads as a radar, not just a floating dot
    // cluster with no frame of reference.
    drawList->AddCircleFilled(center, radiusPx, IM_COL32(10, 10, 15, 160), 48);
    drawList->AddCircle(center, radiusPx, IM_COL32(200, 200, 200, 180), 48, 1.5f);

    const float scale = radiusPx / mapRadiusWorldUnits;

    for (const auto& blip : blips) {
        const glm::vec2 relative = blip.WorldPositionXZ - playerWorldPositionXZ;
        const float distFromCenter = glm::length(relative) * scale;
        if (distFromCenter > radiusPx) {
            continue; // outside the radar's radius — skip rather than clamp to the edge
        }

        // North-up projection (not rotated with player facing) — simplest
        // correct behavior for a first version; world +X maps to screen
        // right, world +Z maps to screen down (matches this engine's -Z
        // forward convention used elsewhere for player/pedestrian facing).
        const ImVec2 screenPos(center.x + relative.x * scale, center.y + relative.y * scale);

        const ImU32 color = blip.Type == MinimapBlip::BlipType::Vehicle
            ? IM_COL32(255, 210, 60, 255)
            : IM_COL32(120, 200, 255, 255);
        drawList->AddCircleFilled(screenPos, 3.0f, color);
    }

    // Player marker: a small triangle pointing in the facing direction,
    // always drawn at dead center regardless of blip positions.
    {
        const float yawRad = glm::radians(playerYaw);
        constexpr float kTriangleSize = 7.0f;
        const ImVec2 tip(center.x + std::sin(yawRad) * kTriangleSize, center.y - std::cos(yawRad) * kTriangleSize);
        const ImVec2 left(center.x + std::sin(yawRad + 2.5f) * kTriangleSize * 0.6f,
                           center.y - std::cos(yawRad + 2.5f) * kTriangleSize * 0.6f);
        const ImVec2 right(center.x + std::sin(yawRad - 2.5f) * kTriangleSize * 0.6f,
                            center.y - std::cos(yawRad - 2.5f) * kTriangleSize * 0.6f);
        drawList->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 255));
    }

    ImGui::End();
}

void HUD::DrawHealthBar(float currentHealth, float maxHealth) {
    constexpr float kBarWidth = 220.0f;
    constexpr float kBarHeight = 22.0f;
    constexpr float kMargin = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(kMargin, static_cast<float>(m_windowHeight) - kBarHeight - kMargin));
    ImGui::SetNextWindowSize(ImVec2(kBarWidth, kBarHeight));
    ImGui::Begin("##HealthBar", nullptr, kOverlayFlags);

    const float fraction = maxHealth > 0.0f ? std::clamp(currentHealth / maxHealth, 0.0f, 1.0f) : 0.0f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 fillEnd(origin.x + kBarWidth * fraction, origin.y + kBarHeight);

    drawList->AddRectFilled(origin, ImVec2(origin.x + kBarWidth, origin.y + kBarHeight), IM_COL32(40, 40, 40, 200), 4.0f);

    // Green -> yellow -> red as health drops, rather than a flat color —
    // reads at a glance without needing to look at the number.
    ImU32 fillColor = IM_COL32(80, 200, 90, 230);
    if (fraction < 0.5f) fillColor = IM_COL32(230, 200, 60, 230);
    if (fraction < 0.25f) fillColor = IM_COL32(220, 70, 60, 230);

    drawList->AddRectFilled(origin, fillEnd, fillColor, 4.0f);
    drawList->AddRect(origin, ImVec2(origin.x + kBarWidth, origin.y + kBarHeight), IM_COL32(220, 220, 220, 200), 4.0f);

    char label[32];
    snprintf(label, sizeof(label), "%.0f / %.0f", currentHealth, maxHealth);
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2(origin.x + (kBarWidth - textSize.x) * 0.5f, origin.y + (kBarHeight - textSize.y) * 0.5f),
                       IM_COL32(255, 255, 255, 255), label);

    ImGui::End();
}

void HUD::DrawAmmoCounter(int currentAmmo, int reserveAmmo) {
    constexpr float kWidth = 140.0f;
    constexpr float kHeight = 30.0f;
    constexpr float kMargin = 20.0f;
    constexpr float kHealthBarHeight = 22.0f;
    constexpr float kSpacing = 8.0f;

    ImGui::SetNextWindowPos(ImVec2(kMargin, static_cast<float>(m_windowHeight) - kHealthBarHeight - kSpacing - kHeight - kMargin));
    ImGui::SetNextWindowSize(ImVec2(kWidth, kHeight));
    ImGui::Begin("##AmmoCounter", nullptr, kOverlayFlags);

    char label[32];
    snprintf(label, sizeof(label), "%d / %d", currentAmmo, reserveAmmo);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 textSize = ImGui::CalcTextSize(label);
    drawList->AddText(ImVec2(origin.x, origin.y + (kHeight - textSize.y) * 0.5f),
                       IM_COL32(255, 255, 255, 235), label);

    ImGui::End();
}

void HUD::DrawSpeedometer(bool insideVehicle, float speedKmh, float rpm, int gear) {
    if (!insideVehicle) {
        return;
    }

    constexpr float kWidth = 160.0f;
    constexpr float kHeight = 70.0f;
    constexpr float kMargin = 20.0f;

    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(m_windowWidth) - kWidth - kMargin,
                                    static_cast<float>(m_windowHeight) - kHeight - kMargin));
    ImGui::SetNextWindowSize(ImVec2(kWidth, kHeight));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##Speedometer", nullptr, kOverlayFlags & ~ImGuiWindowFlags_NoBackground);

    char speedLabel[32];
    snprintf(speedLabel, sizeof(speedLabel), "%.0f km/h", speedKmh);
    ImGui::SetWindowFontScale(1.3f);
    ImGui::Text("%s", speedLabel);
    ImGui::SetWindowFontScale(1.0f);

    const char* gearLabel = gear < 0 ? "R" : (gear == 0 ? "N" : nullptr);
    if (gearLabel) {
        ImGui::Text("Gear: %s   RPM: %.0f", gearLabel, rpm);
    } else {
        ImGui::Text("Gear: %d   RPM: %.0f", gear, rpm);
    }

    ImGui::End();
}

void HUD::DrawInteractionPrompt(const std::string& promptText) {
    if (promptText.empty()) {
        return;
    }

    const ImVec2 textSize = ImGui::CalcTextSize(promptText.c_str());
    constexpr float kPaddingX = 16.0f;
    constexpr float kPaddingY = 8.0f;
    const float boxWidth = textSize.x + kPaddingX * 2.0f;
    const float boxHeight = textSize.y + kPaddingY * 2.0f;
    constexpr float kBottomMargin = 120.0f; // sits above the health bar/ammo stack

    ImGui::SetNextWindowPos(ImVec2((static_cast<float>(m_windowWidth) - boxWidth) * 0.5f,
                                    static_cast<float>(m_windowHeight) - boxHeight - kBottomMargin));
    ImGui::SetNextWindowSize(ImVec2(boxWidth, boxHeight));
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##InteractionPrompt", nullptr, kOverlayFlags & ~ImGuiWindowFlags_NoBackground);
    ImGui::Text("%s", promptText.c_str());
    ImGui::End();
}