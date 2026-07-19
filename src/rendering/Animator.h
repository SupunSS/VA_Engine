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
class Animator {
public:
    Animator();

    void PlayAnimation(std::shared_ptr<Animation> animation);
    void UpdateAnimation(float deltaTime);
    void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform);

    const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

private:
    std::vector<glm::mat4> m_FinalBoneMatrices;
    std::shared_ptr<Animation> m_CurrentAnimation;
    float m_CurrentTime = 0.0f;
};