#include "AudioSystem.h"
#include "Scene.h"
#include "Components.h"
#include "../audio/AudioEngine.h"
#include "physics/VehicleController.h"
#include "../rendering/Animator.h"
#include <random>

namespace AudioSystem {

void UpdateListener(const glm::vec3& position, const glm::vec3& forward,
                     const glm::vec3& up, const glm::vec3& velocity) {
    AudioEngine::Get().SetListener(position, forward, up, velocity);
}

namespace {

// Repositions every persistent looping/ambient source to match its
// entity's current Transform. Covers plain AudioSource emitters (ambient
// loops, anything placed in the world) — vehicle engines are handled
// separately below since they also need pitch/volume driven by RPM.
void UpdateLoopingSources(Scene& scene) {
    auto view = scene.Registry.view<Transform, AudioSource>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& source = view.get<AudioSource>(entity);
        if (source.Handle == kInvalidAudioSource) continue;

        AudioEngine::Get().SetSourcePosition(source.Handle, transform.Position);
    }
}

// Engine note follows RPM: idle sounds low/quiet, redline sounds high/loud.
// ReferenceRpm is whatever RPM should map to MaxPitch/MaxVolume — set it to
// roughly your vehicle's redline so the full pitch range gets used.
void UpdateVehicleEngineSounds(Scene& scene) {
    auto view = scene.Registry.view<Transform, VehicleComponent, VehicleEngineAudio>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& vehicleComp = view.get<VehicleComponent>(entity);
        auto& engineAudio = view.get<VehicleEngineAudio>(entity);

        if (engineAudio.Handle == kInvalidAudioSource || !vehicleComp.Controller) {
            continue;
        }

        AudioEngine::Get().SetSourcePosition(engineAudio.Handle, transform.Position);

        float rpm = vehicleComp.Controller->GetRPM();
        float t = glm::clamp(rpm / engineAudio.ReferenceRpm, 0.0f, 1.0f);
        float pitch = glm::mix(engineAudio.MinPitch, engineAudio.MaxPitch, t);
        float volume = glm::mix(engineAudio.MinVolume, engineAudio.MaxVolume, t);

        AudioEngine::Get().SetSourcePitch(engineAudio.Handle, pitch);
        AudioEngine::Get().SetSourceVolume(engineAudio.Handle, volume);
    }
}

// Picks a random clip from the pool, avoiding an immediate repeat of
// FootstepAudio::LastPlayedIndex when the pool has more than one entry.
AudioClipId PickRandomFootstepClip(FootstepAudio& footsteps, bool isRunning) {
    const auto& pool = (isRunning && !footsteps.RunStepClips.empty())
        ? footsteps.RunStepClips
        : footsteps.WalkStepClips;

    if (pool.empty()) {
        return kInvalidAudioClip;
    }
    if (pool.size() == 1) {
        footsteps.LastPlayedIndex = 0;
        return pool[0];
    }

    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<size_t> dist(0, pool.size() - 1);

    size_t index;
    do {
        index = dist(rng);
    } while (static_cast<int>(index) == footsteps.LastPlayedIndex);

    footsteps.LastPlayedIndex = static_cast<int>(index);
    return pool[index];
}

// Fires footstep one-shots synced to the actual foot-plant frame for any
// entity that has both FootstepAudio and an AnimatorComponent (its clips
// carry "footstep_left"/"footstep_right" AnimationEvents — see
// AnimationStateMachineLoader-authored clips or main.cpp's player setup).
// Falls back to a fixed-interval timer for anything with FootstepAudio but
// no animator (shouldn't normally happen for a walking character, but kept
// as a safety net rather than silently playing no footsteps at all).
void UpdateFootsteps(Scene& scene, float deltaTime) {
    if (deltaTime <= 0.0f) {
        return; // stopped/editor — don't let step timers advance
    }

    // --- Event-driven path: entities with an AnimatorComponent ----------
    auto animatedView = scene.Registry.view<Transform, FootstepAudio, AnimatorComponent>();
    for (auto entity : animatedView) {
        auto& footsteps = animatedView.get<FootstepAudio>(entity);
        auto& animComp = animatedView.get<AnimatorComponent>(entity);

        if (!animComp.AnimatorPtr) continue;

        if (!footsteps.EventCallbackRegistered) {
            // Captured by value: `scene` outlives every entity in it for
            // the lifetime of this callback, and `entity`/`clip lookup` are
            // resolved fresh on each firing rather than captured stale, by
            // re-fetching FootstepAudio/Transform/MovementState/PedestrianAI
            // from the registry inside the lambda instead of capturing
            // references to this frame's view results.
            animComp.AnimatorPtr->SetEventCallback([&scene, entity](const std::string& eventName) {
                if (eventName != "footstep_left" && eventName != "footstep_right") {
                    return; // ignore any other event types a clip might carry later
                }
                if (!scene.Registry.valid(entity) || !scene.Registry.all_of<FootstepAudio, Transform>(entity)) {
                    return;
                }

                auto& fs = scene.Registry.get<FootstepAudio>(entity);
                auto& t = scene.Registry.get<Transform>(entity);

                bool isRunning = false;
                if (scene.Registry.all_of<MovementState>(entity)) {
                    isRunning = scene.Registry.get<MovementState>(entity).IsRunning;
                }

                AudioClipId clip = PickRandomFootstepClip(fs, isRunning);
                if (clip != kInvalidAudioClip) {
                    AudioEngine::Get().PlayOneShot3D(clip, t.Position, fs.Volume);
                }
            });
            footsteps.EventCallbackRegistered = true;
        }
    }

    // --- Fallback timer path: FootstepAudio without an AnimatorComponent -
    auto fallbackView = scene.Registry.view<Transform, FootstepAudio>(entt::exclude<AnimatorComponent>);
    for (auto entity : fallbackView) {
        auto& transform = fallbackView.get<Transform>(entity);
        auto& footsteps = fallbackView.get<FootstepAudio>(entity);

        bool isMoving = false;
        bool isRunning = false;

        if (scene.Registry.all_of<PedestrianAI>(entity)) {
            auto& ai = scene.Registry.get<PedestrianAI>(entity);
            isMoving = ai.WaitTimer <= 0.0f;
        } else if (scene.Registry.all_of<MovementState>(entity)) {
            auto& movement = scene.Registry.get<MovementState>(entity);
            isMoving = movement.IsMoving;
            isRunning = movement.IsRunning;
        }

        if (!isMoving) {
            footsteps.StepTimer = 0.0f;
            continue;
        }

        float interval = footsteps.StrideInterval * (isRunning ? footsteps.RunStrideMultiplier : 1.0f);
        footsteps.StepTimer += deltaTime;
        if (footsteps.StepTimer >= interval) {
            footsteps.StepTimer -= interval;

            AudioClipId clip = PickRandomFootstepClip(footsteps, isRunning);
            if (clip != kInvalidAudioClip) {
                AudioEngine::Get().PlayOneShot3D(clip, transform.Position, footsteps.Volume);
            }
        }
    }
}

} // namespace

void Update(Scene& scene, float deltaTime) {
    UpdateLoopingSources(scene);
    UpdateVehicleEngineSounds(scene);
    UpdateFootsteps(scene, deltaTime);
    AudioEngine::Get().Update();
}

} // namespace AudioSystem