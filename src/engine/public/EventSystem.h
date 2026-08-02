#pragma once

#include "Types.h"
#include <string>

namespace VAPublic {

// ============================================================================
// EVENT SYSTEM INTERFACE
// ============================================================================

class IEventSystem {
public:
    virtual ~IEventSystem() = default;

    virtual void Subscribe(const std::string& eventName, EventCallback callback) = 0;
    virtual void Unsubscribe(const std::string& eventName, EventCallback callback) = 0;
    virtual void Emit(const std::string& eventName, const void* data = nullptr) = 0;
    virtual void Clear(const std::string& eventName) = 0;
};

// ============================================================================
// BUILT-IN EVENT NAMES
// ============================================================================

namespace Events {
    constexpr const char* OnEntitySpawned = "OnEntitySpawned";
    constexpr const char* OnEntityDestroyed = "OnEntityDestroyed";
    constexpr const char* OnEntityMoved = "OnEntityMoved";

    constexpr const char* OnKeyPressed = "OnKeyPressed";
    constexpr const char* OnKeyReleased = "OnKeyReleased";
    constexpr const char* OnMouseMoved = "OnMouseMoved";

    constexpr const char* OnCollisionEnter = "OnCollisionEnter";
    constexpr const char* OnCollisionExit = "OnCollisionExit";

    constexpr const char* OnGameStarted = "OnGameStarted";
    constexpr const char* OnGamePaused = "OnGamePaused";
    constexpr const char* OnGameResumed = "OnGameResumed";
    constexpr const char* OnGameEnded = "OnGameEnded";
}

} // namespace VAPublic