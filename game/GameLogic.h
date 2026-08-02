#pragma once

#include <engine/public/EnginePublic.h>

/**
 * Main game logic controller
 * 
 * This class is where you implement all game behavior:
 * - Player input handling
 * - Entity spawning and management
 * - Game state machine
 * - Win/lose conditions
 * - etc.
 */
class GameLogic {
public:
    GameLogic(VAPublic::IEngine* engine);
    ~GameLogic();

    /**
     * Update game logic
     * @param deltaTime Time since last frame
     * @param input Current input state
     */
    void Update(float deltaTime, const VAPublic::InputState& input);

    /**
     * Render game UI/debug
     */
    void Render();

    /**
     * Spawn the player vehicle
     */
    void SpawnPlayerVehicle();

    /**
     * Spawn an NPC
     * @param position Spawn position
     */
    void SpawnNPC(const VAPublic::Vec3& position);

    /**
     * Get current game state
     */
    enum class GameState {
        MainMenu,
        Playing,
        Paused,
        GameOver
    };

    GameState GetState() const { return m_state; }
    void SetState(GameState newState);

private:
    VAPublic::IEngine* m_engine;
    GameState m_state;
    VAPublic::EntityId m_playerVehicle;
    float m_elapsedTime;

    // Input handling
    void HandleInput(const VAPublic::InputState& input);

    // Gameplay
    void UpdatePlayerVehicle(float deltaTime);
    void UpdateNPCs(float deltaTime);
};