#pragma once

/**
 * VA Engine Public API
 * 
 * This header includes all public APIs for game developers.
 * Do NOT include any engine internal headers directly.
 * 
 * Usage:
 *   #include <engine/EnginePublic.h>
 *   
 *   int main() {
 *       IEngine* engine = InitializeEngine("config/engine.yaml");
 *       
 *       while (!engine->ShouldExit()) {
 *           // Game loop
 *       }
 *       
 *       ShutdownEngine();
 *   }
 */

#include "Types.h"
#include "EventSystem.h"
#include "InputSystem.h"
#include "Scene.h"
#include "PhysicsWorld.h"
#include "AudioEngine.h"
#include "ScriptingEngine.h"
#include "RenderSystem.h"
#include "EngineAPI.h"
