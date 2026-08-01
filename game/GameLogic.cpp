/**
 * Game Logic Implementation
 * 
 * This is where all your game mechanics live.
 * Modify this file to add/change gameplay behavior.
 */

#include "GameLogic.h"
#include <glm/glm.hpp>

GameLogic::GameLogic(IEngine* engine)
    : m_engine(engine)
    , m_state(GameState::MainMenu)
    , m_playerVehicle(0)
    , m_elapsedTime(0.0f) {
    
    m_engine->Log("GameLogic created");
}

GameLogic::~GameLogic() {
    m_engine->Log("GameLogic destroyed");
}

void GameLogic::Update(float deltaTime, const InputState& input) {
    m_elapsedTime += deltaTime;
    
    switch (m_state) {
        case GameState::MainMenu:
            // TODO: Implement main menu logic
            // For now, auto-start game
            if (m_elapsedTime > 0.1f) {
                SetState(GameState::Playing);
            }
            break;

        case GameState::Playing:
            HandleInput(input);
            UpdatePlayerVehicle(deltaTime);
            UpdateNPCs(deltaTime);
            break;

        case GameState::Paused:
            HandleInput(input);
            break;

        case GameState::GameOver:
            // TODO: Implement game over screen
            if (m_engine->IsKeyHeld(KeyCode::Escape)) {
                m_engine->RequestExit();
            }
            break;
    }
}

void GameLogic::Render() {
    // TODO: Render UI, HUD, debug info
    // Example:
    //   - Draw speedometer
    //   - Draw minimap
    //   - Draw debug text
    // All done via ImGui or custom UI system
}

void GameLogic::HandleInput(const InputState& input) {
    // Example input handling
    if (m_engine->IsKeyHeld(KeyCode::Escape)) {
        if (m_state == GameState::Playing) {
            SetState(GameState::Paused);
        } else if (m_state == GameState::Paused) {
            SetState(GameState::Playing);
        }
    }

    // Spawn vehicle on 'F' key
    if (m_engine->IsKeyHeld(KeyCode::F) && m_playerVehicle == 0) {
        SpawnPlayerVehicle();
    }

    // Player vehicle controls
    if (m_playerVehicle != 0 && m_state == GameState::Playing) {
        // TODO: Send throttle/brake/steer to vehicle controller
        // This would be done via engine API or Lua script
    }
}

void GameLogic::SpawnPlayerVehicle() {
    Vec3 spawnPos = m_engine->GetCameraPosition() + glm::vec3(0.0f, 2.0f, 0.0f);
    
    // Spawn from prefab
    m_playerVehicle = m_engine->SpawnEntity("car_sport", spawnPos);
    
    if (m_playerVehicle != 0) {
        m_engine->Log("Player vehicle spawned");
    } else {
        m_engine->Log("Failed to spawn player vehicle - prefab not found");
    }
}

void GameLogic::SpawnNPC(const Vec3& position) {
    EntityId npcId = m_engine->SpawnEntity("npc_pedestrian", position);
    
    if (npcId != 0) {
        m_engine->Log("NPC spawned at position");
    }
}

void GameLogic::UpdatePlayerVehicle(float deltaTime) {
    if (m_playerVehicle == 0) return;
    
    // Get input for vehicle control
    InputState input = m_engine->GetInput();
    
    // Map input to vehicle control (0-1 range)
    // This would normally go through a scripting system or physics component
    float throttle = 0.0f;
    float brake = 0.0f;
    float steer = 0.0f;

    if (m_engine->IsKeyHeld(KeyCode::W)) throttle = 1.0f;
    if (m_engine->IsKeyHeld(KeyCode::S)) brake = 1.0f;
    if (m_engine->IsKeyHeld(KeyCode::A)) steer = -1.0f;
    if (m_engine->IsKeyHeld(KeyCode::D)) steer = 1.0f;

    // TODO: Apply to vehicle via engine API
    // Once vehicle controller system is exposed via EngineAPI:
    //   m_engine->GetVehicleController(m_playerVehicle).SetInput(throttle, brake, steer);
}

void GameLogic::UpdateNPCs(float deltaTime) {
    // TODO: Update all NPC entities
    // - Run pathfinding
    // - Update animations
    // - Handle spawning/despawning
}

void GameLogic::SetState(GameState newState) {
    if (m_state == newState) return;

    // Exit old state
    switch (m_state) {
        case GameState::Playing:
            m_engine->GetEvents().Emit(Events::OnGamePaused);
            break;
        default:
            break;
    }

    m_state = newState;

    // Enter new state
    switch (m_state) {
        case GameState::MainMenu:
            m_engine->Log("Entering MainMenu state");
            break;
        case GameState::Playing:
            m_engine->Log("Entering Playing state");
            m_engine->GetEvents().Emit(Events::OnGameStarted);
            break;
        case GameState::Paused:
            m_engine->Log("Entering Paused state");
            m_engine->GetEvents().Emit(Events::OnGamePaused);
            break;
        case GameState::GameOver:
            m_engine->Log("Entering GameOver state");
            m_engine->GetEvents().Emit(Events::OnGameEnded);
            break;
    }
}
