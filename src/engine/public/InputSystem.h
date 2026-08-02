#pragma once

#include "Types.h"
#include <string>
#include <unordered_map>

namespace VAPublic {

// ============================================================================
// INPUT SYSTEM INTERFACE
// ============================================================================

class IInputSystem {
public:
    virtual ~IInputSystem() = default;

    virtual InputState GetCurrentInput() const = 0;
    virtual bool IsKeyHeld(KeyCode key) const = 0;
    virtual bool IsKeyJustPressed(KeyCode key) const = 0;

    virtual glm::vec2 GetMousePosition() const = 0;
    virtual glm::vec2 GetMouseDelta() const = 0;

    virtual void BindKey(const std::string& actionName, KeyCode key) = 0;
    virtual void UnbindAction(const std::string& actionName) = 0;
    virtual bool IsActionActive(const std::string& actionName) const = 0;

    virtual void LoadConfig(const std::string& configPath) = 0;
    virtual void SaveConfig(const std::string& configPath) = 0;
};

} // namespace VAPublic