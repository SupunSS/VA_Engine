#pragma once

#include "Types.h"
#include <string>
#include <memory>

// ============================================================================
// SCENE INTERFACE
// ============================================================================

/**
 * Scene container for all entities and components
 */
class Scene {
public:
    virtual ~Scene() = default;

    /**
     * Create a new entity in this scene
     * @return New EntityId
     */
    virtual EntityId CreateEntity() = 0;

    /**
     * Destroy an entity
     * @param entityId Entity to destroy
     */
    virtual void DestroyEntity(EntityId entityId) = 0;

    /**
     * Get entity by name
     * @param name Entity name
     * @return EntityId (0 if not found)
     */
    virtual EntityId FindEntityByName(const std::string& name) = 0;

    /**
     * Get number of entities in scene
     * @return Entity count
     */
    virtual uint32_t GetEntityCount() const = 0;

    /**
     * Set simulation speed (1.0 = normal)
     * @param speed Multiplier for time
     */
    virtual void SetSimulationSpeed(float speed) = 0;

    /**
     * Get current simulation speed
     * @return Speed multiplier
     */
    virtual float GetSimulationSpeed() const = 0;

    /**
     * Pause simulation
     */
    virtual void Pause() = 0;

    /**
     * Resume simulation
     */
    virtual void Resume() = 0;

    /**
     * Check if scene is paused
     * @return true if paused
     */
    virtual bool IsPaused() const = 0;
};
