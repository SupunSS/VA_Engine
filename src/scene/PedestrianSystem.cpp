#include "PedestrianSystem.h"
#include "../scene/Components.h"
#include "../rendering/Animator.h"
#include "../rendering/AnimationStateMachine.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include "../rendering/AnimationStateMachineLoader.h"
#include "../core/Log.h"

namespace PedestrianSystem {

namespace {
constexpr float kWaitDurationSeconds = 2.0f; // pause at each waypoint before moving on
constexpr float kArrivalRadius = 0.3f;       // "close enough" distance to count as arrived
constexpr float kTurnSpeed = 8.0f;           // slerp rate facing the walk direction
constexpr float kAnimBlendDuration = 0.25f;

// Built once per entity, the first time it's needed. Each pedestrian was
// assigned an archetype JSON path at spawn (AnimatorComponent::StateMachinePath
// — see PedestrianSpawnSystem), so different pedestrians can genuinely run
// different state machines (different clips, different transition sets)
// rather than all sharing one shape.
void EnsureStateMachine(AnimatorComponent& animComp, Model& pedestrianModel) {
    if (animComp.StateMachine) return;

    if (animComp.StateMachinePath.empty()) {
        Log::Error("PedestrianSystem: entity has no StateMachinePath assigned — was it spawned via PedestrianSpawnSystem?");
        return;
    }

    animComp.StateMachine = AnimationStateMachineLoader::LoadFromFile(animComp.StateMachinePath, pedestrianModel);
    if (!animComp.StateMachine) {
        Log::Error("PedestrianSystem: failed to load '{}' — this pedestrian will have no animation transitions",
                   animComp.StateMachinePath);
    }
}
}

void Update(Scene& scene, float deltaTime, const glm::vec3& viewerPosition, float simulationRadius) {
    auto view = scene.Registry.view<Transform, PedestrianAI, AnimatorComponent>();
    const float simulationRadiusSq = simulationRadius * simulationRadius;

    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);

        const glm::vec3 toViewer = transform.Position - viewerPosition;
        const float distSq = toViewer.x * toViewer.x + toViewer.y * toViewer.y + toViewer.z * toViewer.z;
        if (distSq > simulationRadiusSq) {
            continue;
        }

        auto& ai = view.get<PedestrianAI>(entity);
        auto& animComp = view.get<AnimatorComponent>(entity);

        if (animComp.SourceModel) {
            EnsureStateMachine(animComp, *animComp.SourceModel);
        }

        if (ai.PathWaypoints.empty()) {
            continue;
        }

        bool isMoving = false;

        if (ai.WaitTimer > 0.0f) {
            ai.WaitTimer -= deltaTime;
        } else {
            const glm::vec3& target = ai.PathWaypoints[ai.CurrentWaypointIndex];
            glm::vec3 toTarget = target - transform.Position;
            toTarget.y = 0.0f;
            const float distance = glm::length(toTarget);

            if (distance <= kArrivalRadius) {
                ai.WaitTimer = kWaitDurationSeconds;
                ai.CurrentWaypointIndex = (ai.CurrentWaypointIndex + 1) % static_cast<int>(ai.PathWaypoints.size());
            } else {
                isMoving = true;

                const glm::vec3 direction = toTarget / distance;
                transform.Position += direction * ai.MoveSpeed * deltaTime;

                const float targetYaw = std::atan2(direction.x, direction.z);
                const glm::quat targetRotation = glm::angleAxis(targetYaw, glm::vec3(0.0f, 1.0f, 0.0f));
                transform.Rotation = glm::slerp(transform.Rotation, targetRotation,
                                                 glm::min(kTurnSpeed * deltaTime, 1.0f));
            }
        }

        if (animComp.StateMachine) {
            animComp.StateMachine->SetBool("IsMoving", isMoving);
            if (animComp.AnimatorPtr) {
                animComp.StateMachine->Update(*animComp.AnimatorPtr);
            }
        }

        if (animComp.AnimatorPtr) {
            animComp.AnimatorPtr->UpdateAnimation(deltaTime);
        }
    }
}

} // namespace PedestrianSystem