#include "Animator.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>


Animator::Animator() {
    m_FinalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
}

void Animator::PlayAnimation(std::shared_ptr<Animation> animation, float blendDuration) {

    if (m_PreviewMode) return;
    if (animation == m_CurrentAnimation) {
        return; // already playing this clip — avoid restarting/re-blending on redundant calls
    }


    if (blendDuration > 0.0f && m_CurrentAnimation) {
        // NOTE: if a new PlayAnimation arrives while already mid-blend, the
        // in-progress blend is discarded in favor of blending from wherever
        // the current clip's pose is right now — not a true "blend of
        // blends". This can produce a small pop on rapid state changes
        // (e.g. toggling Walk/Run faster than blendDuration). Acceptable
        // for locomotion-style transitions; a fully correct fix would
        // sample and freeze the actual blended pose as the new "from"
        // pose, which is a larger feature on its own.
        m_PreviousAnimation = m_CurrentAnimation;
        m_PreviousTime = m_CurrentTime;
        m_BlendDuration = blendDuration;
        m_BlendElapsed = 0.0f;
        m_IsBlending = true;
    } else {
        m_IsBlending = false;
        m_PreviousAnimation = nullptr;
    }

    m_CurrentAnimation = animation;
    m_CurrentTime = 0.0f;
}

void Animator::UpdateAnimation(float deltaTime) {
    if (m_PreviewMode) return;
    if (!m_CurrentAnimation) return;

    m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * deltaTime;
    m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());

    if (m_IsBlending && m_PreviousAnimation) {
        m_PreviousTime += m_PreviousAnimation->GetTicksPerSecond() * deltaTime;
        m_PreviousTime = fmod(m_PreviousTime, m_PreviousAnimation->GetDuration());

        m_BlendElapsed += deltaTime;
        if (m_BlendElapsed >= m_BlendDuration) {
            m_IsBlending = false;
            m_PreviousAnimation = nullptr;
        }
    }

    CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
}

glm::mat4 Animator::BlendLocalTransforms(const glm::mat4& from, const glm::mat4& to, float t) {
    glm::vec3 skewFrom, scaleFrom, translationFrom;
    glm::vec4 perspectiveFrom;
    glm::quat rotationFrom;
    glm::decompose(from, scaleFrom, rotationFrom, translationFrom, skewFrom, perspectiveFrom);

    glm::vec3 skewTo, scaleTo, translationTo;
    glm::vec4 perspectiveTo;
    glm::quat rotationTo;
    glm::decompose(to, scaleTo, rotationTo, translationTo, skewTo, perspectiveTo);

    const glm::vec3 blendedTranslation = glm::mix(translationFrom, translationTo, t);
    const glm::vec3 blendedScale = glm::mix(scaleFrom, scaleTo, t);
    const glm::quat blendedRotation = glm::slerp(rotationFrom, rotationTo, t);

    return glm::translate(glm::mat4(1.0f), blendedTranslation)
         * glm::mat4_cast(blendedRotation)
         * glm::scale(glm::mat4(1.0f), blendedScale);
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform) {
    const std::string& nodeName = node->name;

    glm::mat4 currentLocal = node->transformation;
    Bone* currentBone = m_CurrentAnimation->FindBone(nodeName);
    if (currentBone) {
        currentBone->Update(m_CurrentTime);
        currentLocal = currentBone->GetLocalTransform();
    }

    glm::mat4 blendedLocal = currentLocal;

    if (m_IsBlending && m_PreviousAnimation) {
        glm::mat4 previousLocal = node->transformation;
        Bone* previousBone = m_PreviousAnimation->FindBone(nodeName);
        if (previousBone) {
            previousBone->Update(m_PreviousTime);
            previousLocal = previousBone->GetLocalTransform();
        }

        const float t = m_BlendDuration > 0.0f
            ? glm::clamp(m_BlendElapsed / m_BlendDuration, 0.0f, 1.0f)
            : 1.0f;
        blendedLocal = BlendLocalTransforms(previousLocal, currentLocal, t);
    }

    glm::mat4 globalTransform = parentTransform * blendedLocal;

    const auto& boneInfoMap = m_CurrentAnimation->GetBoneIDMap();
    auto it = boneInfoMap.find(nodeName);
    if (it != boneInfoMap.end()) {
        int index = it->second.id;
        if (index >= 0 && index < MAX_BONES) {
            m_FinalBoneMatrices[index] = globalTransform * it->second.offsetMatrix;
        }
    }

    for (int i = 0; i < node->childrenCount; ++i) {
        CalculateBoneTransform(&node->children[i], globalTransform);
    }
}

void Animator::FireEventsInRange(float previousNormalizedTime, float currentNormalizedTime, bool looped) {
    const auto& events = m_CurrentAnimation->GetEvents();

    for (const auto& event : events) {
        bool crossed = false;
        if (!looped) {
            // Normal case: fire if the event's timestamp falls within
            // (previous, current].
            crossed = event.NormalizedTime > previousNormalizedTime &&
                      event.NormalizedTime <= currentNormalizedTime;
        } else {
            // Looped this frame: the playhead wrapped from near 1.0 back
            // to near 0.0. An event fires if it falls in EITHER the tail
            // (previous, 1.0] or the head [0.0, current] of that wrap —
            // without this, events sitting right at the loop point would
            // never fire, since previousNormalizedTime > currentNormalizedTime
            // and a naive single-range check would always be false.
            crossed = event.NormalizedTime > previousNormalizedTime ||
                      event.NormalizedTime <= currentNormalizedTime;
        }

        if (crossed) {
            m_EventCallback(event.Name);
        }
    }
}

void Animator::ScrubToNormalizedTime(std::shared_ptr<Animation> animation, float normalizedTime) {
    if (!animation) return;
    m_CurrentAnimation = animation;
    m_IsBlending = false;
    m_PreviousAnimation = nullptr;
    m_CurrentTime = glm::clamp(normalizedTime, 0.0f, 1.0f) * animation->GetDuration();
    CalculateBoneTransform(&animation->GetRootNode(), glm::mat4(1.0f));
}