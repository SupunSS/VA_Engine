#include "VehicleSpawner.h"
#include "../scene/Components.h"
#include "../physics/PhysicsWorld.h"
#include "../physics/VehicleController.h"
#include "../rendering/Primitives.h"
#include "../rendering/Material.h"
#include "../audio/AudioEngine.h"
#include "../core/AssetPaths.h"
#include "../core/Log.h"
#include <memory>

entt::entity SpawnTestVehicle(Scene& scene, PhysicsWorld& physicsWorld, const glm::vec3& position) {
    constexpr float kHalfX = 0.9f;
    constexpr float kHalfY = 0.4f;
    constexpr float kHalfZ = 1.8f;
    constexpr float kSpawnHeightOffset = 0.90f;

    const glm::vec3 spawnPosition = position + glm::vec3(0.0f, kSpawnHeightOffset, 0.0f);

    auto vehicleEntity = scene.CreateEntity();
    scene.Registry.emplace<VehicleTag>(vehicleEntity);
    scene.Registry.emplace<VehicleOccupant>(vehicleEntity);

    auto& transform = scene.Registry.get<Transform>(vehicleEntity);
    transform.Position = spawnPosition;
    transform.Scale = glm::vec3(1.0f);

    auto chassisModel = Primitives::CreateVehicleBody(kHalfX, kHalfY, kHalfZ, 1.1f);
    auto chassisMaterial = std::make_shared<Material>();
    chassisMaterial->albedoTint = glm::vec3(0.08f, 0.40f, 0.80f);
    scene.Registry.emplace<MeshRenderer>(vehicleEntity, chassisModel, chassisMaterial);

    auto controller = std::make_shared<VehicleController>(physicsWorld, spawnPosition);
    auto& vehicleComp = scene.Registry.emplace<VehicleComponent>(vehicleEntity);
    vehicleComp.Controller = controller;

    AudioClipId engineClip = AudioEngine::Get().LoadClip(AssetPaths::Resolve(AssetPaths::Category::Audio, "sfx/vehicle_engine_loop.wav"));
    auto& engineAudio = scene.Registry.emplace<VehicleEngineAudio>(vehicleEntity);
    engineAudio.EngineLoopClip = engineClip;
    engineAudio.Handle = AudioEngine::Get().CreateSource3D(
        engineClip, position, true, true,
        engineAudio.MinVolume, 3.0f, 60.0f);

    constexpr float kWheelRadius = 0.35f;
    constexpr float kWheelWidth  = 0.25f;
    auto wheelModel = Primitives::CreateWheel(kWheelRadius, kWheelWidth, 16);

    auto tyreMaterial = std::make_shared<Material>();
    tyreMaterial->albedoTint = glm::vec3(0.12f, 0.12f, 0.12f);

    auto rimMaterial = std::make_shared<Material>();
    rimMaterial->albedoTint = glm::vec3(0.75f, 0.75f, 0.80f);

    for (int i = 0; i < 4; ++i) {
        auto wheelEntity = scene.CreateEntity();
        scene.Registry.emplace<MeshRenderer>(wheelEntity, wheelModel, tyreMaterial);

        glm::vec3 wheelPos;
        glm::quat wheelRot;
        controller->GetWheelTransform(i, wheelPos, wheelRot);

        auto& wt = scene.Registry.get<Transform>(wheelEntity);
        wt.Position = wheelPos;
        wt.Rotation = wheelRot;
        wt.Scale    = glm::vec3(1.0f);

        vehicleComp.WheelEntities[i] = wheelEntity;
    }

    Log::Info("Spawned low-poly vehicle entity with 4 wheels at ({}, {}, {}).",
              position.x, position.y, position.z);
    return vehicleEntity;
}