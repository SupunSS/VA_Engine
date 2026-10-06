#include "SaveLoadHandler.h"
#include "AppState.h"
#include "PlayMode.h"
#include "VehicleSpawner.h"
#include "../scene/Components.h"
#include "../scene/SaveSystem.h"
#include "../physics/VehicleController.h"
#include "../core/Log.h"
#include <string>
#include <vector>

namespace {

void SaveToSlot(AppState& app, const std::string& slotName)
{
    Scene& scene = *app.world.scene;
    const entt::entity playerEntity = app.player.entity;

    SaveGameData data;

    auto& savePlayerTransform = scene.Registry.get<Transform>(playerEntity);
    data.Player.PlayerTransform.Position = savePlayerTransform.Position;
    data.Player.PlayerTransform.Rotation = savePlayerTransform.Rotation;

    auto& saveHealth = scene.Registry.get<Health>(playerEntity);
    data.Player.Health = saveHealth.Current;
    data.Player.MaxHealth = saveHealth.Max;

    auto& saveAmmo = scene.Registry.get<Ammo>(playerEntity);
    data.Player.AmmoCurrent = saveAmmo.Current;
    data.Player.AmmoReserve = saveAmmo.Reserve;

    data.Player.InsideVehicle = app.play.insideVehicle;
    data.Player.ActiveVehicleIndex = -1;

    int vehicleIndex = 0;
    auto saveVehicleView = scene.Registry.view<Transform, VehicleTag, VehicleComponent>();
    for (auto vEntity : saveVehicleView) {
        auto& vTransform = saveVehicleView.get<Transform>(vEntity);
        SavedVehicleState vs;
        vs.ChassisTransform.Position = vTransform.Position;
        vs.ChassisTransform.Rotation = vTransform.Rotation;
        data.Vehicles.push_back(vs);

        if (vEntity == app.play.activeVehicleEntity) {
            data.Player.ActiveVehicleIndex = vehicleIndex;
        }
        ++vehicleIndex;
    }

    SaveSystem::WriteSaveFile(slotName, data);
    Log::Info("Saved game to slot '{}'.", slotName);
}

void LoadFromSlot(AppState& app, const std::string& slotName)
{
    SaveGameData data;
    if (!SaveSystem::ReadSaveFile(slotName, data)) {
        Log::Warn("Failed to load save slot '{}'.", slotName);
        return;
    }

    Scene& scene = *app.world.scene;
    PhysicsWorld& physicsWorld = *app.world.physicsWorld;
    const entt::entity playerEntity = app.player.entity;

    if (app.play.insideVehicle) {
        scene.Registry.get<Transform>(app.player.visualEntity).Scale = glm::vec3(0.01f);
        app.play.insideVehicle = false;
    }
    app.play.activeVehicleEntity = entt::null;
    PlayMode::DestroyAllVehicles(app);

    app.world.characterController->SetPosition(data.Player.PlayerTransform.Position);
    auto& loadPlayerTransform = scene.Registry.get<Transform>(playerEntity);
    loadPlayerTransform.Position = data.Player.PlayerTransform.Position;
    loadPlayerTransform.Rotation = data.Player.PlayerTransform.Rotation;

    auto& loadHealth = scene.Registry.get<Health>(playerEntity);
    loadHealth.Current = data.Player.Health;
    loadHealth.Max = data.Player.MaxHealth;

    auto& loadAmmo = scene.Registry.get<Ammo>(playerEntity);
    loadAmmo.Current = data.Player.AmmoCurrent;
    loadAmmo.Reserve = data.Player.AmmoReserve;

    // Re-stream every loaded chunk so building/entity deltas apply
    app.world.chunkManager->ForceReloadAll(scene, physicsWorld);
    app.world.chunkManager->Update(data.Player.PlayerTransform.Position, scene, physicsWorld,
                                   app.culling.maxRenderDistance);

    // Recreate vehicles at their saved transforms
    std::vector<entt::entity> recreatedVehicles;
    for (const auto& vehicleData : data.Vehicles) {
        entt::entity newVehicle = SpawnTestVehicle(scene, physicsWorld, vehicleData.ChassisTransform.Position);
        auto& vc = scene.Registry.get<VehicleComponent>(newVehicle);
        if (vc.Controller) {
            vc.Controller->SetChassisTransform(vehicleData.ChassisTransform.Position, vehicleData.ChassisTransform.Rotation);
        }
        recreatedVehicles.push_back(newVehicle);
    }

    // Restore vehicle occupancy if the player was driving when they saved
    if (data.Player.InsideVehicle && data.Player.ActiveVehicleIndex >= 0 &&
        data.Player.ActiveVehicleIndex < static_cast<int>(recreatedVehicles.size())) {
        app.play.activeVehicleEntity = recreatedVehicles[data.Player.ActiveVehicleIndex];
        app.play.insideVehicle = true;
        scene.Registry.get<VehicleOccupant>(app.play.activeVehicleEntity).DriverEntity = playerEntity;
        scene.Registry.get<Transform>(app.player.visualEntity).Scale = glm::vec3(0.0f);

        glm::vec3 snapPos; glm::quat snapRot;
        auto& vc = scene.Registry.get<VehicleComponent>(app.play.activeVehicleEntity);
        if (vc.Controller) {
            vc.Controller->GetChassisTransform(snapPos, snapRot);
            app.world.vehicleCamera->SetTarget(snapPos, snapRot, 0.0f, 0.016f);
        }
    }

    Log::Info("Loaded save slot '{}'.", slotName);
}

} // namespace

void HandleSaveLoadPanel(AppState& app)
{
    std::string requestedSlotName;
    bool isSaveAction = false;
    if (!app.editorObjects.ui->DrawSaveLoadPanel(requestedSlotName, isSaveAction)) {
        return;
    }

    if (isSaveAction) {
        SaveToSlot(app, requestedSlotName);
    } else {
        LoadFromSlot(app, requestedSlotName);
    }
}