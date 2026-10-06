#pragma once
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>
#include <assimp/scene.h>
#include "Bone.h"
#include "AnimData.h"

// Mirrors Assimp's aiNode hierarchy but stores only what we need each frame:
// the node's name, its bind-pose local transform, and its children. Kept
// separate from aiScene so Animation doesn't hold a pointer into Assimp's
// importer after the importer goes out of scope.
struct AssimpNodeData {
    glm::mat4 transformation;
    std::string name;
    int childrenCount;
    std::vector<AssimpNodeData> children;
};

// A named point in a clip's timeline, expressed as a fraction of the clip's
// full duration (0.0 = start, 1.0 = end) rather than raw ticks — keeps
// event authoring independent of a clip's TicksPerSecond/Duration values.
struct AnimationEvent {
    std::string Name;
    float NormalizedTime = 0.0f; // 0..1
};

class Animation {
public:
    Animation() = default;
    Animation(const aiScene* scene, const aiAnimation* animation,
              std::map<std::string, BoneInfo>& boneInfoMap, int& boneCount);

    Bone* FindBone(const std::string& name);

    float GetTicksPerSecond() const { return m_TicksPerSecond; }
    float GetDuration() const { return m_Duration; }
    const AssimpNodeData& GetRootNode() const { return m_RootNode; }
    const std::map<std::string, BoneInfo>& GetBoneIDMap() const { return m_BoneInfoMap; }
    const std::string& GetName() const { return m_Name; }
    void AddEvent(const std::string& name, float normalizedTime);
    const std::vector<AnimationEvent>& GetEvents() const { return m_Events; }

    void SetSourcePath(const std::string& path) { m_SourcePath = path; }
    const std::string& GetSourcePath() const { return m_SourcePath; }

    void RemoveEvent(size_t index);
    std::vector<AnimationEvent>& GetEventsMutable() { return m_Events; }

private:
    void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src);
    void ReadMissingBones(const aiAnimation* animation,
                          std::map<std::string, BoneInfo>& boneInfoMap, int& boneCount);

    float m_Duration = 0.0f;
    float m_TicksPerSecond = 25.0f;
    std::vector<Bone> m_Bones;
    AssimpNodeData m_RootNode;
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    std::string m_Name;
    std::vector<AnimationEvent> m_Events;
    std::string m_SourcePath;
};