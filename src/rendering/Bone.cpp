#include "Bone.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace {
glm::mat4 ConvertMatrix(const aiMatrix4x4& m) {
    glm::mat4 result;
    result[0][0] = m.a1; result[1][0] = m.a2; result[2][0] = m.a3; result[3][0] = m.a4;
    result[0][1] = m.b1; result[1][1] = m.b2; result[2][1] = m.b3; result[3][1] = m.b4;
    result[0][2] = m.c1; result[1][2] = m.c2; result[2][2] = m.c3; result[3][2] = m.c4;
    result[0][3] = m.d1; result[1][3] = m.d2; result[2][3] = m.d3; result[3][3] = m.d4;
    return result;
}
}

Bone::Bone(const std::string& name, int id, const aiNodeAnim* channel)
    : m_Name(name), m_ID(id) {
    for (unsigned int i = 0; i < channel->mNumPositionKeys; ++i) {
        aiVector3D p = channel->mPositionKeys[i].mValue;
        m_Positions.push_back({ glm::vec3(p.x, p.y, p.z), (float)channel->mPositionKeys[i].mTime });
    }
    for (unsigned int i = 0; i < channel->mNumRotationKeys; ++i) {
        aiQuaternion q = channel->mRotationKeys[i].mValue;
        m_Rotations.push_back({ glm::quat(q.w, q.x, q.y, q.z), (float)channel->mRotationKeys[i].mTime });
    }
    for (unsigned int i = 0; i < channel->mNumScalingKeys; ++i) {
        aiVector3D s = channel->mScalingKeys[i].mValue;
        m_Scales.push_back({ glm::vec3(s.x, s.y, s.z), (float)channel->mScalingKeys[i].mTime });
    }
}

int Bone::GetPositionIndex(float animationTime) const {
    for (int i = 0; i < (int)m_Positions.size() - 1; ++i) {
        if (animationTime < m_Positions[i + 1].timeStamp) return i;
    }
    return (int)m_Positions.size() - 2 >= 0 ? (int)m_Positions.size() - 2 : 0;
}

int Bone::GetRotationIndex(float animationTime) const {
    for (int i = 0; i < (int)m_Rotations.size() - 1; ++i) {
        if (animationTime < m_Rotations[i + 1].timeStamp) return i;
    }
    return (int)m_Rotations.size() - 2 >= 0 ? (int)m_Rotations.size() - 2 : 0;
}

int Bone::GetScaleIndex(float animationTime) const {
    for (int i = 0; i < (int)m_Scales.size() - 1; ++i) {
        if (animationTime < m_Scales[i + 1].timeStamp) return i;
    }
    return (int)m_Scales.size() - 2 >= 0 ? (int)m_Scales.size() - 2 : 0;
}

float Bone::GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) const {
    float midWayLength = animationTime - lastTimeStamp;
    float framesDiff = nextTimeStamp - lastTimeStamp;
    if (framesDiff <= 0.0f) return 0.0f;
    return std::clamp(midWayLength / framesDiff, 0.0f, 1.0f);
}

glm::mat4 Bone::InterpolatePosition(float animationTime) const {
    if (m_Positions.size() == 1) return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

    int p0 = GetPositionIndex(animationTime);
    int p1 = p0 + 1;
    float factor = GetScaleFactor(m_Positions[p0].timeStamp, m_Positions[p1].timeStamp, animationTime);
    glm::vec3 finalPos = glm::mix(m_Positions[p0].position, m_Positions[p1].position, factor);
    return glm::translate(glm::mat4(1.0f), finalPos);
}

glm::mat4 Bone::InterpolateRotation(float animationTime) const {
    if (m_Rotations.size() == 1) return glm::toMat4(glm::normalize(m_Rotations[0].orientation));

    int p0 = GetRotationIndex(animationTime);
    int p1 = p0 + 1;
    float factor = GetScaleFactor(m_Rotations[p0].timeStamp, m_Rotations[p1].timeStamp, animationTime);
    glm::quat finalRot = glm::slerp(m_Rotations[p0].orientation, m_Rotations[p1].orientation, factor);
    return glm::toMat4(glm::normalize(finalRot));
}

glm::mat4 Bone::InterpolateScaling(float animationTime) const {
    if (m_Scales.size() == 1) return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

    int p0 = GetScaleIndex(animationTime);
    int p1 = p0 + 1;
    float factor = GetScaleFactor(m_Scales[p0].timeStamp, m_Scales[p1].timeStamp, animationTime);
    glm::vec3 finalScale = glm::mix(m_Scales[p0].scale, m_Scales[p1].scale, factor);
    return glm::scale(glm::mat4(1.0f), finalScale);
}

void Bone::Update(float animationTime) {
    m_LocalTransform = InterpolatePosition(animationTime) *
                       InterpolateRotation(animationTime) *
                       InterpolateScaling(animationTime);
}