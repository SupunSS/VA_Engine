#pragma once
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <vector>
#include <string>
#include <assimp/scene.h>

struct KeyPosition { glm::vec3 position; float timeStamp; };
struct KeyRotation  { glm::quat orientation; float timeStamp; };
struct KeyScale     { glm::vec3 scale; float timeStamp; };

// Holds the keyframe tracks for a single bone within one animation clip, and
// produces that bone's local transform at an arbitrary point in time via
// linear (position/scale) and spherical-linear (rotation) interpolation
// between the two surrounding keyframes.
class Bone {
public:
    Bone(const std::string& name, int id, const aiNodeAnim* channel);

    void Update(float animationTime);
    glm::mat4 GetLocalTransform() const { return m_LocalTransform; }
    const std::string& GetBoneName() const { return m_Name; }
    int GetBoneID() const { return m_ID; }

private:
    int GetPositionIndex(float animationTime) const;
    int GetRotationIndex(float animationTime) const;
    int GetScaleIndex(float animationTime) const;

    float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime) const;

    glm::mat4 InterpolatePosition(float animationTime) const;
    glm::mat4 InterpolateRotation(float animationTime) const;
    glm::mat4 InterpolateScaling(float animationTime) const;

    std::vector<KeyPosition> m_Positions;
    std::vector<KeyRotation> m_Rotations;
    std::vector<KeyScale> m_Scales;

    glm::mat4 m_LocalTransform{ 1.0f };
    std::string m_Name;
    int m_ID;
};