#pragma once

#include "Types.h"
#include "EventSystem.h"
#include <string>
#include <memory>

// Forward declarations
class Scene;
class PhysicsWorld;
class RenderSystem;
class AudioEngine;
class ScriptingEngine;

// ============================================================================
// ENGINE INTERFACE
// ============================================================================

/**
 * Main interface for accessing all engine systems.
 * Game code should ONLY interact with the engine through this interface.
 * Do not include or use engine internal headers.
 */
class IEngine {
public:
    virtual ~IEngine() = default;

    // ========================================================================
    // SCENE MANAGEMENT
    // ========================================================================

    /**
     * Load a scene from JSON file
     * @param sceneFilePath Path to .json scene file
     */
    virtual void LoadScene(const std::string& sceneFilePath) = 0;

    /**
     * Save current scene to JSON file
     * @param sceneFilePath Path to save to
     */
    virtual void SaveScene(const std::string& sceneFilePath) = 0;

    /**
     * Get the active scene
     * @return Reference to current scene
     */
    virtual Scene& GetScene() = 0;

    /**
     * Get the ECS registry for direct entity access (advanced)
     * @return EnTT registry
     */
    virtual entt::registry& GetRegistry() = 0;

    // ========================================================================
    // ENTITY MANAGEMENT
    // ========================================================================

    /**
     * Spawn an entity from a prefab
     * @param prefabName Name of prefab (e.g., "car_sport")
     * @param position Spawn position
     * @return EntityId of created entity
     */
    virtual EntityId SpawnEntity(const std::string& prefabName, const Vec3& position) = 0;

    /**
     * Destroy an entity
     * @param entityId ID of entity to destroy
     */
    virtual void DestroyEntity(EntityId entityId) = 0;

    /**
     * Check if entity exists
     * @param entityId ID to check
     * @return true if entity is valid
     */
    virtual bool IsEntityValid(EntityId entityId) const = 0;

    // ========================================================================
    // TRANSFORM QUERIES
    // ========================================================================

    /**
     * Set entity position
     * @param entityId Target entity
     * @param position New position
     */
    virtual void SetPosition(EntityId entityId, const Vec3& position) = 0;

    /**
     * Get entity position
     * @param entityId Target entity
     * @return Current position
     */
    virtual Vec3 GetPosition(EntityId entityId) const = 0;

    /**
     * Set entity rotation
     * @param entityId Target entity
     * @param rotation New rotation as quaternion
     */
    virtual void SetRotation(EntityId entityId, const Quat& rotation) = 0;

    /**
     * Get entity rotation
     * @param entityId Target entity
     * @return Current rotation
     */
    virtual Quat GetRotation(EntityId entityId) const = 0;

    // ========================================================================
    // PHYSICS
    // ========================================================================

    /**
     * Get physics world for advanced queries
     * @return Reference to physics system
     */
    virtual PhysicsWorld& GetPhysics() = 0;

    /**
     * Raycast from a point in a direction
     * @param origin Starting position
     * @param direction Direction vector (should be normalized)
     * @param maxDistance Maximum raycast distance
     * @return RaycastHit with results
     */
    virtual RaycastHit Raycast(const Vec3& origin, const Vec3& direction, float maxDistance = 1000.0f) = 0;

    /**
     * Apply impulse to an entity
     * @param entityId Target entity
     * @param impulse Force to apply
     */
    virtual void ApplyImpulse(EntityId entityId, const Vec3& impulse) = 0;

    // ========================================================================
    // AUDIO
    // ========================================================================

    /**
     * Get audio engine
     * @return Reference to audio system
     */
    virtual AudioEngine& GetAudio() = 0;

    /**
     * Play a sound at a location
     * @param soundPath Path to audio file
     * @param position 3D position (nullptr = camera position)
     * @param volume Volume multiplier (0-1)
     * @param loop Whether to loop
     */
    virtual void PlaySound(const std::string& soundPath, const Vec3* position = nullptr, 
                          float volume = 1.0f, bool loop = false) = 0;

    // ========================================================================
    // INPUT
    // ========================================================================

    /**
     * Get current input state
     * @return Struct with key/mouse states
     */
    virtual InputState GetInput() const = 0;

    /**
     * Check if a key is currently held
     * @param key KeyCode to check
     * @return true if key is down
     */
    virtual bool IsKeyHeld(KeyCode key) const = 0;

    /**
     * Remap an input action (advanced)
     * @param actionName Name of action (e.g., "Move Forward")
     * @param key Key to bind
     */
    virtual void RemapInput(const std::string& actionName, KeyCode key) = 0;

    // ========================================================================
    // SCRIPTING
    // ========================================================================

    /**
     * Get scripting engine for Lua access
     * @return Reference to scripting system
     */
    virtual ScriptingEngine& GetScripting() = 0;

    /**
     * Execute a Lua script
     * @param scriptPath Path to .lua file
     * @param scriptName Name for debug purposes
     */
    virtual void ExecuteScript(const std::string& scriptPath, const std::string& scriptName = "") = 0;

    /**
     * Hot-reload a Lua script
     * @param scriptName Name of script to reload
     */
    virtual void ReloadScript(const std::string& scriptName) = 0;

    // ========================================================================
    // RENDERING
    // ========================================================================

    /**
     * Get render system
     * @return Reference to rendering
     */
    virtual RenderSystem& GetRender() = 0;

    /**
     * Get camera position
     * @return Current camera position
     */
    virtual Vec3 GetCameraPosition() const = 0;

    /**
     * Set camera position
     * @param position New camera position
     */
    virtual void SetCameraPosition(const Vec3& position) = 0;

    // ========================================================================
    // EVENTS
    // ========================================================================

    /**
     * Get event system for custom events
     * @return Reference to event system
     */
    virtual IEventSystem& GetEvents() = 0;

    // ========================================================================
    // DEBUG
    // ========================================================================

    /**
     * Check if running in developer mode
     * @return true if dev mode is active
     */
    virtual bool IsDeveloperMode() const = 0;

    /**
     * Log a message (engine logging)
     * @param message Message to log
     */
    virtual void Log(const std::string& message) = 0;

    /**
     * Enable/disable debug rendering
     * @param enabled Whether to show debug visuals
     */
    virtual void SetDebugRendering(bool enabled) = 0;

    // ========================================================================
    // LIFECYCLE
    // ========================================================================

    /**
     * Check if engine should exit
     * @return true if quit requested
     */
    virtual bool ShouldExit() const = 0;

    /**
     * Request engine shutdown
     */
    virtual void RequestExit() = 0;

    /**
     * Get delta time since last frame
     * @return Time in seconds
     */
    virtual float GetDeltaTime() const = 0;

    /**
     * Get total elapsed time
     * @return Time in seconds
     */
    virtual float GetElapsedTime() const = 0;
};

// ============================================================================
// ENGINE INITIALIZATION
// ============================================================================

/**
 * Get the global engine instance
 * @return Pointer to singleton IEngine
 */
extern IEngine* GetEngine();

/**
 * Initialize the engine
 * @param configPath Path to engine config file
 * @return Engine instance
 */
extern IEngine* InitializeEngine(const std::string& configPath = "config/engine.yaml");

/**
 * Shutdown the engine
 */
extern void ShutdownEngine();

/**
 * Connect real engine systems to the public API
 * Called from main() after creating all systems
 * 
 * Usage:
 *   IEngine* engine = InitializeEngine("config/engine.yaml");
 *   ConnectEngineSystems(engine, &scene, &physicsWorld, &audioEngine, &scriptEngine, &camera);
 */
extern void ConnectEngineSystems(IEngine* engine, void* scene, void* physicsWorld,
                                 void* audioEngine, void* scriptEngine, void* camera);
