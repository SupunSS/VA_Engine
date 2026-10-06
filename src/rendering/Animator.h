#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "Animation.h"
#include <functional>

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
    float GetBlendElapsed() const { return m_BlendElapsed; }
    float GetBlendDuration() const { return m_BlendDuration; }
    // Called once per frame per event whose NormalizedTime was crossed
    // during that frame's UpdateAnimation, for whichever clip is currently
    // playing (blended-out previous clips during a crossfade do not fire
    // events — only the current/target clip does). Set to nullptr to
    // disable.
    using EventCallback = std::function<void(const std::string&)>;
    void SetEventCallback(EventCallback callback) { m_EventCallback = std::move(callback); }

    // Editor preview support. While preview mode is on, UpdateAnimation()
    // (the normal per-frame gameplay path) becomes a no-op, so the state
    // machine's own Update(animator) calls each frame don't fight the
    // editor's manual scrubbing. ScrubToNormalizedTime works regardless of
    // preview mode, but is only meaningful to call repeatedly while preview
    // mode is on — see EditorUI::DrawAnimatorTimeline.
    void BeginPreview() { m_PreviewMode = true; }
    void EndPreview() { m_PreviewMode = false; }
    bool IsInPreviewMode() const { return m_PreviewMode; }
    void ScrubToNormalizedTime(std::shared_ptr<Animation> animation, float normalizedTime);

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

    void FireEventsInRange(float previousNormalizedTime, float currentNormalizedTime, bool looped);

    EventCallback m_EventCallback;

    bool m_PreviewMode = false;
};