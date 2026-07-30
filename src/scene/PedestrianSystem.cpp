#include "PedestrianSystem.h"
#include "../scene/Components.h"
#include "../rendering/Animator.h"
#include <glm/gtc/quaternion.hpp>
#include <cmath>

namespace PedestrianSystem {

namespace {
constexpr float kWaitDurationSeconds = 2.0f; // pause at each waypoint before moving on
constexpr float kArrivalRadius = 0.3f;       // "close enough" distance to count as arrived
constexpr float kTurnSpeed = 8.0f;           // slerp rate facing the walk direction
}

void Update(Scene& scene, float deltaTime, const glm::vec3& viewerPosition, float simulationRadius) {
    auto view = scene.Registry.view<Transform, PedestrianAI, AnimatorComponent>();
    const float simulationRadiusSq = simulationRadius * simulationRadius;

    for (auto entity : view) {
        auto& transform = view.get<Transform>(entity);

        // Distance check FIRST, before touching AI state or the animator at
        // all — this is what actually saves the cost. A pedestrian outside
        // simulationRadius does nothing this frame: no waypoint math, no
        // bone matrix recalculation, nothing. It simply resumes exactly
        // where it left off once the viewer comes back within range, which
        // reads fine since nobody was watching it in the meantime anyway.
        const glm::vec3 toViewer = transform.Position - viewerPosition;
        const float distSq = toViewer.x * toViewer.x + toViewer.y * toViewer.y + toViewer.z * toViewer.z;
        if (distSq > simulationRadiusSq) {
            continue;
        }

        auto& ai = view.get<PedestrianAI>(entity);
        auto& animComp = view.get<AnimatorComponent>(entity);

        if (ai.PathWaypoints.empty()) {
            continue;
        }

        AnimatorComponent::State desiredState = AnimatorComponent::State::Idle;

        if (ai.WaitTimer > 0.0f) {
            ai.WaitTimer -= deltaTime;
        } else {
            const glm::vec3& target = ai.PathWaypoints[ai.CurrentWaypointIndex];
            glm::vec3 toTarget = target - transform.Position;
            toTarget.y = 0.0f; // horizontal distance/direction only — walking, not flying
            const float distance = glm::length(toTarget);

            if (distance <= kArrivalRadius) {
                ai.WaitTimer = kWaitDurationSeconds;
                ai.CurrentWaypointIndex = (ai.CurrentWaypointIndex + 1) % static_cast<int>(ai.PathWaypoints.size());
            } else {
                desiredState = AnimatorComponent::State::Walk;

                const glm::vec3 direction = toTarget / distance;
                transform.Position += direction * ai.MoveSpeed * deltaTime;

                const float targetYaw = std::atan2(direction.x, direction.z);
                const glm::quat targetRotation = glm::angleAxis(targetYaw, glm::vec3(0.0f, 1.0f, 0.0f));
                transform.Rotation = glm::slerp(transform.Rotation, targetRotation,
                                                 glm::min(kTurnSpeed * deltaTime, 1.0f));
            }
        }

        if (desiredState != animComp.CurrentState) {
            animComp.CurrentState = desiredState;
            if (animComp.AnimatorPtr) {
                animComp.AnimatorPtr->PlayAnimation(
                    desiredState == AnimatorComponent::State::Walk ? animComp.WalkAnim : animComp.IdleAnim);
            }
        }

        if (animComp.AnimatorPtr) {
            animComp.AnimatorPtr->UpdateAnimation(deltaTime);
        }
    }
}

} // namespace PedestrianSystem