cmake_minimum_required(VERSION 3.20)
project(GTAEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Stb REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)

# ============================================================================
# ENGINE CORE LIBRARY
# Everything in /src/ except main.cpp — this is the piece game developers
# never need to see or recompile. Ship this + the /public/ headers to them.
# ============================================================================
add_library(va_engine_core STATIC
    src/core/Log.cpp
    src/core/memory/StackAllocator.cpp
    src/core/memory/PoolAllocator.cpp
    src/core/jobs/JobSystem.cpp
    src/rendering/Shader.cpp
    src/rendering/Camera.cpp
    src/rendering/Texture.cpp
    src/rendering/Mesh.cpp
    src/rendering/Model.cpp
    src/rendering/Material.cpp
    src/rendering/Skybox.cpp
    src/scene/Components.cpp
    src/scene/Scene.cpp
    src/scene/SpatialGrid.cpp
    src/scene/SceneLoader.cpp
    src/scripting/ScriptEngine.cpp
    src/scene/ChunkManager.cpp
    src/editor/EditorUI.cpp
    src/rendering/GridRenderer.cpp
    src/physics/PhysicsWorld.cpp
    src/rendering/ResourceManager.cpp
    src/physics/CharacterController.cpp
    src/rendering/FollowCamera.cpp
    src/rendering/Animation.cpp
    src/rendering/Animator.cpp
    src/rendering/Bone.cpp
    src/rendering/Primitives.cpp
    src/rendering/Frustum.cpp
    src/rendering/FrustumRenderer.cpp
    src/physics/VehicleController.cpp
    src/rendering/VehicleCamera.cpp
    src/scene/CityLayoutConfig.cpp
    src/scene/CityLayout.cpp
    src/scene/PedestrianSystem.cpp
    src/scene/PedestrianSpawnSystem.cpp
    src/editor/HUD.cpp
    src/audio/AudioEngine.cpp
    src/scene/AudioSystem.cpp
    src/scene/ChunkDeltaStore.cpp
    src/scene/Savesystem.cpp
    src/engine/Config.cpp
    src/engine/Engine.cpp
    src/engine/EventSystem.cpp
    src/engine/HotReload.cpp
    src/engine/LuaBindings.cpp
    src/engine/Prefab.cpp
    src/core/AssetPaths.cpp
    src/rendering/AnimationStateMachine.cpp
    src/rendering/AnimationStateMachineLoader.cpp
)

target_include_directories(va_engine_core PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/engine/public
    ${Stb_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
)

# Fixes the GLM experimental extension error (e.g., component_wise.hpp)
target_compile_definitions(va_engine_core PUBLIC GLM_ENABLE_EXPERIMENTAL)

target_link_libraries(va_engine_core PUBLIC
    glfw glm::glm Threads::Threads glad::glad assimp::assimp EnTT::EnTT
    nlohmann_json::nlohmann_json sol2::sol2 ${LUA_LIBRARIES} imgui::imgui
    Jolt::Jolt imguizmo::imguizmo winmm
)
target_link_libraries(va_engine_core PRIVATE yaml-cpp::yaml-cpp)

# ============================================================================
# ENGINE DEV SANDBOX EXECUTABLE
# Your existing test/dev harness — unchanged behavior, just now linked
# against the core library instead of being compiled monolithically.
# ============================================================================
add_executable(engine src/main.cpp)
target_link_libraries(engine PRIVATE va_engine_core)

# ============================================================================
# GAME EXECUTABLE
# This is what game developers actually build. It only ever touches
# /game/ and /assets/ — never /src/.
# ============================================================================
add_executable(game
    game/main.cpp
    game/GameLogic.cpp
)
target_link_libraries(game PRIVATE va_engine_core)