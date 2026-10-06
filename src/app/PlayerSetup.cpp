#include "PlayerSetup.h"
#include "AppState.h"
#include "../scene/Components.h"
#include "../scene/SceneLoader.h"
#include "../rendering/AnimationStateMachineLoader.h"
#include "../audio/FootstepClipLoader.h"
#include "../core/AssetPaths.h"
#include "../core/Assert.h"

void CreatePlayer(AppState& app)
{
    Scene& scene = *app.world.scene;
    PlayerState& player = app.player;

    player.entity = scene.CreateEntity();
    scene.Registry.emplace<PlayerTag>(player.entity);
    scene.Registry.emplace<Health>(player.entity);
    scene.Registry.emplace<Ammo>(player.entity);
    scene.Registry.get<Transform>(player.entity).Position = glm::vec3(0.0f, 1.0f, 0.0f);

    scene.Registry.emplace<MovementState>(player.entity);
    auto& playerFootsteps = scene.Registry.emplace<FootstepAudio>(player.entity);
    playerFootsteps.WalkStepClips = FootstepClipLoader::LoadNumberedSequence("sfx/Steps_floor-", 1, 21, ".wav", 3);
    // No separate run clips — reuses the same pool for both walk and run
    // (PickRandomFootstepClip falls back to WalkStepClips when RunStepClips
    // is empty). Add a dedicated RunStepClips pool later if you get
    // sprint-specific footstep audio.

    player.visualEntity = scene.CreateEntity();
    auto& playerVisualTransform = scene.Registry.get<Transform>(player.visualEntity);
    playerVisualTransform.Parent = player.entity;
    playerVisualTransform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    playerVisualTransform.Scale = glm::vec3(0.01f, 0.01f, 0.01f);

    player.model = SceneLoader::GetOrLoadModel("player/player.fbx");
    scene.Registry.emplace<MeshRenderer>(player.visualEntity, player.model, nullptr);

    player.idleAnim = player.model->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Idle.fbx"));
    player.walkAnim = player.model->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Walking.fbx"));
    player.runAnim  = player.model->LoadAnimation(AssetPaths::Resolve(AssetPaths::Category::Models, "player/Running.fbx"));

    player.animator = std::make_shared<Animator>();
    player.animator->PlayAnimation(player.idleAnim);

    player.stateMachine = AnimationStateMachineLoader::LoadFromFile("player.json", *player.model);
    ENGINE_ASSERT(player.stateMachine != nullptr, "Failed to load player animation state machine");
    player.stateMachine->SetInitialState("Idle");

    // Attach the player's animator to the ECS so systems that iterate
    // AnimatorComponent (e.g. AudioSystem's event-driven footstep path)
    // pick up the player the same way they already do pedestrians.
    auto& playerAnimComp = scene.Registry.emplace<AnimatorComponent>(player.entity);
    playerAnimComp.AnimatorPtr = player.animator;
    playerAnimComp.StateMachine = player.stateMachine;
    playerAnimComp.SourceModel = player.model;
}