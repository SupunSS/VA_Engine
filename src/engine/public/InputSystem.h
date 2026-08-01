#pragma once

#include "Types.h"
#include <string>
#include <unordered_map>

// ============================================================================
// INPUT SYSTEM INTERFACE
// ============================================================================

class IInputSystem {
public:
    virtual ~IInputSystem() = default;

    /**
     * Get current input state
     * @return Current keyboard and mouse state
     */
    virtual InputState GetCurrentInput() const = 0;

    /**
     * Check if a key is held down
     * @param key KeyCode to check
     * @return true if key is currently pressed
     */
    virtual bool IsKeyHeld(KeyCode key) const = 0;

    /**
     * Check if a key was just pressed this frame
     * @param key KeyCode to check
     * @return true if key transitioned from up to down
     */
    virtual bool IsKeyJustPressed(KeyCode key) const = 0;

    /**
     * Get mouse position
     * @return Mouse X, Y in screen space
     */
    virtual glm::vec2 GetMousePosition() const = 0;

    /**
     * Get mouse delta since last frame
     * @return Change in mouse position
     */
    virtual glm::vec2 GetMouseDelta() const = 0;

    /**
     * Bind a key to an action name
     * @param actionName Name of action (e.g., "MoveForward")
     * @param key KeyCode to bind
     */
    virtual void BindKey(const std::string& actionName, KeyCode key) = 0;

    /**
     * Unbind an action
     * @param actionName Name of action to unbind
     */
    virtual void UnbindAction(const std::string& actionName) = 0;

    /**
     * Check if an action is active
     * @param actionName Name of action
     * @return true if any bound key is pressed
     */
    virtual bool IsActionActive(const std::string& actionName) const = 0;

    /**
     * Load input bindings from file
     * @param configPath Path to input config file
     */
    virtual void LoadConfig(const std::string& configPath) = 0;

    /**
     * Save current input bindings to file
     * @param configPath Path to save to
     */
    virtual void SaveConfig(const std::string& configPath) = 0;
};
