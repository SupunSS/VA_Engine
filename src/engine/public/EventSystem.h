#pragma once

#include "Types.h"
#include <string>

// ============================================================================
// EVENT SYSTEM INTERFACE
// ============================================================================

class IEventSystem {
public:
    virtual ~IEventSystem() = default;

    /**
     * Subscribe to an event by name
     * @param eventName Name of the event (e.g., "OnEntityDestroyed")
     * @param callback Function to call when event fires
     */
    virtual void Subscribe(const std::string& eventName, EventCallback callback) = 0;

    /**
     * Unsubscribe from an event
     * @param eventName Name of the event
     * @param callback Callback to remove (must be exact match)
     */
    virtual void Unsubscribe(const std::string& eventName, EventCallback callback) = 0;

    /**
     * Emit an event (executes all callbacks)
     * @param eventName Name of the event
     * @param data Optional data to pass to callbacks (may be nullptr)
     */
    virtual void Emit(const std::string& eventName, const void* data = nullptr) = 0;

    /**
     * Clear all subscriptions for an event
     * @param eventName Name of the event
     */
    virtual void Clear(const std::string& eventName) = 0;
};

// ============================================================================
// BUILT-IN EVENT NAMES
// ============================================================================

namespace Events {
    // Entity lifecycle
    constexpr const char* OnEntitySpawned = "OnEntitySpawned";       // EntityId*
    constexpr const char* OnEntityDestroyed = "OnEntityDestroyed";   // EntityId*
    constexpr const char* OnEntityMoved = "OnEntityMoved";           // EntityId*

    // Input
    constexpr const char* OnKeyPressed = "OnKeyPressed";             // KeyCode*
    constexpr const char* OnKeyReleased = "OnKeyReleased";           // KeyCode*
    constexpr const char* OnMouseMoved = "OnMouseMoved";             // glm::vec2* (x, y)

    // Physics
    constexpr const char* OnCollisionEnter = "OnCollisionEnter";     // EntityId* (pair)
    constexpr const char* OnCollisionExit = "OnCollisionExit";       // EntityId* (pair)

    // Game
    constexpr const char* OnGameStarted = "OnGameStarted";           // nullptr
    constexpr const char* OnGamePaused = "OnGamePaused";             // nullptr
    constexpr const char* OnGameResumed = "OnGameResumed";           // nullptr
    constexpr const char* OnGameEnded = "OnGameEnded";               // nullptr
}
