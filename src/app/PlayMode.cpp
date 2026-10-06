#include "PlayMode.h"
#include "AppState.h"
#include "WindowUtils.h"
#include "../scene/Components.h"
#include "../audio/AudioEngine.h"
#include <GLFW/glfw3.h>
#include <vector>

namespace PlayMode {

void Start(AppState& app)
{
    app.play.playMode = true;
    SetCursorMode(app.window, GLFW_CURSOR_DISABLED, true);
    app.input.mouseLookEnabled = false;
    app.input.mouseLookNeedsReset = true;

    const glm::vec3& spawn = app.play.playerSpawnPosition;
    app.world.characterController->SetPosition(spawn);
    app.world.scene->Registry.get<Transform>(app.player.entity).Position = spawn;
    app.world.chunkManager->Update(spawn, *app.world.scene, *app.world.physicsWorld,
                                   app.culling.maxRenderDistance);
}

void StopKeepState(AppState& app)
{
    app.play.playMode = false;
    SetCursorMode(app.window, GLFW_CURSOR_NORMAL, false);
    app.input.mouseLookEnabled = false;
    app.input.mouseLookNeedsReset = true;
}

void DestroyAllVehicles(AppState& app)
{
    Scene& scene = *app.world.scene;

    auto existingVehicles = scene.Registry.view<VehicleTag, VehicleComponent>();
    std::vector<entt::entity> vehiclesToDestroy(existingVehicles.begin(), existingVehicles.end());
    for (auto oldVehicleEntity : vehiclesToDestroy) {
        if (scene.Registry.all_of<VehicleEngineAudio>(oldVehicleEntity)) {
            auto& engineAudio = scene.Registry.get<VehicleEngineAudio>(oldVehicleEntity);
            AudioEngine::Get().DestroySource(engineAudio.Handle);
        }

        auto& oldVehicleComp = scene.Registry.get<VehicleComponent>(oldVehicleEntity);
        for (auto wheelEnt : oldVehicleComp.WheelEntities) {
            if (wheelEnt != entt::null && scene.Registry.valid(wheelEnt)) {
                scene.DestroyEntity(wheelEnt);
            }
        }
        scene.DestroyEntity(oldVehicleEntity);
    }
}

void ResetToInitialState(AppState& app)
{
    Scene& scene = *app.world.scene;
    PhysicsWorld& physicsWorld = *app.world.physicsWorld;

    if (app.play.insideVehicle) {
        scene.Registry.get<Transform>(app.player.visualEntity).Scale = glm::vec3(0.01f);
        app.play.insideVehicle = false;
    }
    app.play.activeVehicleEntity = entt::null;

    DestroyAllVehicles(app);

    {
        std::vector<entt::entity> toDestroy;
        auto view = scene.Registry.view<RigidBody, PhysicsTestBody>();
        for (auto entity : view) {
            toDestroy.push_back(entity);
        }
        for (auto entity : toDestroy) {
            auto& rb = scene.Registry.get<RigidBody>(entity);
            if (!rb.BodyId.IsInvalid()) {
                physicsWorld.DestroyBody(rb.BodyId);
            }
            scene.DestroyEntity(entity);
        }
    }

    const glm::vec3& spawn = app.play.playerSpawnPosition;
    app.world.characterController->SetPosition(spawn);
    scene.Registry.get<Transform>(app.player.entity).Position = spawn;
    scene.Registry.get<Transform>(app.player.entity).Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    app.player.stateMachine->SetInitialState("Idle");
    app.player.animator->PlayAnimation(app.player.idleAnim);

    app.world.camera->Position = app.play.initialCameraPosition;
    app.world.camera->SetYawPitch(app.play.initialCameraYaw, app.play.initialCameraPitch);

    app.editorObjects.ui->SelectedEntity = entt::null;
}

void ResetWorldForProject(AppState& app)
{
    Scene& scene = *app.world.scene;
    PhysicsWorld& physicsWorld = *app.world.physicsWorld;

    if (app.play.playMode) {
        StopKeepState(app);
    }
    ResetToInitialState(app); // vehicles, test bodies, player, camera, selection

    // Remove the chunk-streamed city and stop it streaming back in
    app.world.chunkManager->ForceReloadAll(scene, physicsWorld);
    app.world.chunkManager->SetStreamingEnabled(false);

    // Remove pedestrians
    {
        std::vector<entt::entity> pedestrians;
        for (auto e : scene.Registry.view<PedestrianTag>()) {
            pedestrians.push_back(e);
        }
        for (auto e : pedestrians) {
            scene.DestroyEntity(e);
        }
    }

    // Fresh flat terrain
    app.world.terrainSystem->Clear();
    app.world.terrainSystem->Update(app.world.camera->Position);

    // One big static floor whose top sits at y=0, matching the flat terrain.
    // Physics only: terrain already provides the visual.
    if (app.play.blankFloorBodyId.IsInvalid()) {
        const glm::vec3 floorHalfExtents(1000.0f, 0.5f, 1000.0f);
        const glm::vec3 floorCenter(0.0f, -0.5f, 0.0f);
        app.play.blankFloorBodyId = physicsWorld.CreateBoxBody(floorCenter, floorHalfExtents, /*isStatic=*/true);
    }

    app.play.blankWorld = true;
}

} // namespace PlayMode