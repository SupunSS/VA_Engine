#include "Animator.h"
#include <cmath>

Animator::Animator() {
    m_FinalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
}

void Animator::PlayAnimation(std::shared_ptr<Animation> animation) {
    m_CurrentAnimation = animation;
    m_CurrentTime = 0.0f;
}

void Animator::UpdateAnimation(float deltaTime) {
    if (!m_CurrentAnimation) return;

    m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * deltaTime;
    m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());

    CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
}

void Animator::CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform) {
    const std::string& nodeName = node->name;
    glm::mat4 nodeTransform = node->transformation;

    Bone* bone = m_CurrentAnimation->FindBone(nodeName);
    if (bone) {
        bone->Update(m_CurrentTime);
        nodeTransform = bone->GetLocalTransform();
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

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