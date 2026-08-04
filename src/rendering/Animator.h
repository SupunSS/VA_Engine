#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "Animation.h"

constexpr int MAX_BONES = 100;

// Advances the current clip's playback time each frame and produces the
// final array of bone matrices (animated local transform composed with
// parent transforms, then multiplied by each bone's inverse bind pose) that
// gets uploaded to the skinning shader as uBoneMatrices[].
//
// Supports crossfading between two clips: calling PlayAnimation with a
// blendDuration > 0 keeps the previous clip advancing in the background
// (m_PreviousAnimation) and blends its pose with the new clip's pose over
// that duration, instead of snapping instantly. Passing blendDuration = 0
// (the default) preserves the old instant-switch behavior exactly.
class Animator {
public:
    Animator();

    void PlayAnimation(std::shared_ptr<Animation> animation, float blendDuration = 0.0f);
    void UpdateAnimation(float deltaTime);
    void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

    const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

    bool IsBlending() const { return m_IsBlending; }
    std::shared_ptr<Animation> GetCurrentAnimation() const { return m_CurrentAnimation; }

private:
    // Decomposes both matrices into translation/rotation/scale, lerps
    // translation and scale, slerps rotation, then recomposes — a naive
    // 4x4 matrix lerp would produce incorrect results for rotation.
    static glm::mat4 BlendLocalTransforms(const glm::mat4& from, const glm::mat4& to, float t);

    std::vector<glm::mat4> m_FinalBoneMatrices;
    std::shared_ptr<Animation> m_CurrentAnimation;
    float m_CurrentTime = 0.0f;

    std::shared_ptr<Animation> m_PreviousAnimation;
    float m_PreviousTime = 0.0f;
    float m_BlendDuration = 0.0f;
    float m_BlendElapsed = 0.0f;
    bool m_IsBlending = false;
};