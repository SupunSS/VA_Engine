#pragma once

#include "Types.h"
#include <string>
#include <memory>

namespace VAPublic {

// ============================================================================
// SCENE INTERFACE
// ============================================================================

class IScene {
public:
    virtual ~IScene() = default;

    virtual EntityId CreateEntity() = 0;
    virtual void DestroyEntity(EntityId entityId) = 0;
    virtual EntityId FindEntityByName(const std::string& name) = 0;
    virtual uint32_t GetEntityCount() const = 0;

    virtual void SetSimulationSpeed(float speed) = 0;
    virtual float GetSimulationSpeed() const = 0;

    virtual void Pause() = 0;
    virtual void Resume() = 0;
    virtual bool IsPaused() const = 0;
};

} // namespace VAPublic