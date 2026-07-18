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

add_executable(engine 
    src/main.cpp
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
)

target_include_directories(engine PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${Stb_INCLUDE_DIR}
    ${LUA_INCLUDE_DIR}
)
target_link_libraries(engine PRIVATE glfw glm::glm Threads::Threads glad::glad assimp::assimp EnTT::EnTT nlohmann_json::nlohmann_json sol2::sol2 ${LUA_LIBRARIES} imgui::imgui Jolt::Jolt imguizmo::imguizmo)