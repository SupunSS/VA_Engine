#include "AudioSystem.h"
#include "Scene.h"
#include "Components.h"
#include "../audio/AudioEngine.h"
#include "physics/VehicleController.h"

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

// Fires footstep one-shots on a stride timer for anything that walks.
// Pedestrians report movement via their own PedestrianAI::WaitTimer (no
// extra bookkeeping needed there); anything else — namely the player —
// reports movement via the generic MovementState component, which
// main.cpp keeps up to date from the character controller's input.
void UpdateFootsteps(Scene& scene, float deltaTime) {
    if (deltaTime <= 0.0f) {
        return; // stopped/editor — don't let step timers advance
    }

    auto view = scene.Registry.view<Transform, FootstepAudio>();
    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);
        auto& footsteps = view.get<FootstepAudio>(entity);

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
            // Reset so the next step lands immediately on the first frame
            // of movement instead of waiting out a stale partial interval.
            footsteps.StepTimer = 0.0f;
            continue;
        }

        float interval = footsteps.StrideInterval * (isRunning ? footsteps.RunStrideMultiplier : 1.0f);
        footsteps.StepTimer += deltaTime;
        if (footsteps.StepTimer >= interval) {
            footsteps.StepTimer -= interval;

            AudioClipId clip = isRunning ? footsteps.RunStepClip : footsteps.WalkStepClip;
            if (clip == kInvalidAudioClip) {
                clip = footsteps.WalkStepClip; // fall back if no dedicated run clip was assigned
            }
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