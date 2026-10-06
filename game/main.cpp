/**
 * VA Engine - Game Entry Point
 * 
 * DEVELOPERS: Modify this file and files in the /game/ folder only.
 * Do NOT modify files in /src/ (engine code).
 * 
 * This is the game layer - it uses the public EngineAPI to drive your game.
 */

#include <engine/public/EnginePublic.h>
#include "GameLogic.h"
#include <iostream>
#include "core/AssetPaths.h"

using namespace VAPublic;

// Global engine pointer (initialized by engine)
IEngine* g_engine = nullptr;

// Game state
GameLogic* g_gameLogic = nullptr;

/**
 * Initialize game (called by engine)
 */
void InitializeGame(IEngine* engine) {
    g_engine = engine;
    
    g_engine->Log("=== Game Initialization ===");
    
    // Create game logic
    g_gameLogic = new GameLogic(engine);
    
    // Load the initial scene
    g_engine->LoadScene(AssetPaths::Resolve(AssetPaths::Category::Scenes, "test_scene.json"));
    
    // Subscribe to events
    g_engine->GetEvents().Subscribe(Events::OnGameStarted, [](const void*) {
        g_engine->Log("Game started!");
    });
    
    g_engine->Log("Game initialized");
}

/**
 * Update game logic (called every frame)
 */
void UpdateGame(float deltaTime) {
    if (!g_gameLogic) return;
    
    // Get input
    InputState input = g_engine->GetInput();
    
    // Update game logic
    g_gameLogic->Update(deltaTime, input);
}

/**
 * Render game (called after physics/update)
 */
void RenderGame() {
    if (!g_gameLogic) return;
    g_gameLogic->Render();
}

/**
 * Shutdown game (called when engine exits)
 */
void ShutdownGame() {
    if (g_gameLogic) {
        delete g_gameLogic;
        g_gameLogic = nullptr;
    }
    
    g_engine->Log("Game shutdown");
}

/**
 * Main entry point (for standalone game build)
 * When building with the engine, the engine's main() calls InitializeGame/UpdateGame/RenderGame
 */
int main(int argc, char* argv[]) {
    // Initialize engine
    IEngine* engine = InitializeEngine("config/engine.yaml");
    if (!engine) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return 1;
    }
    
    // Initialize game
    InitializeGame(engine);
    
    // Main loop
    while (!engine->ShouldExit()) {
        float deltaTime = engine->GetDeltaTime();
        
        UpdateGame(deltaTime);
        RenderGame();
    }
    
    // Cleanup
    ShutdownGame();
    ShutdownEngine();
    
    return 0;
}