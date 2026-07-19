#include "Animation.h"

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

Animation::Animation(const aiScene* scene, const aiAnimation* animation,
                      std::map<std::string, BoneInfo>& boneInfoMap, int& boneCount) {
    m_Name = animation->mName.C_Str();
    m_Duration = (float)animation->mDuration;
    m_TicksPerSecond = animation->mTicksPerSecond != 0 ? (float)animation->mTicksPerSecond : 25.0f;

    ReadHierarchyData(m_RootNode, scene->mRootNode);
    ReadMissingBones(animation, boneInfoMap, boneCount);

    // Keep our own copy of the bone map + count as they stood after this
    // animation's channels were merged in — later animations loaded against
    // the same model reuse and extend this map rather than starting fresh.
    m_BoneInfoMap = boneInfoMap;
}

Bone* Animation::FindBone(const std::string& name) {
    for (auto& bone : m_Bones) {
        if (bone.GetBoneName() == name) return &bone;
    }
    return nullptr;
}

void Animation::ReadHierarchyData(AssimpNodeData& dest, const aiNode* src) {
    dest.name = src->mName.C_Str();
    dest.transformation = ConvertMatrix(src->mTransformation);
    dest.childrenCount = src->mNumChildren;

    for (unsigned int i = 0; i < src->mNumChildren; ++i) {
        AssimpNodeData newData;
        ReadHierarchyData(newData, src->mChildren[i]);
        dest.children.push_back(newData);
    }
}

// Every bone this specific clip animates gets a Bone (keyframe track). Bones
// that exist in the skeleton but aren't animated by this particular clip
// (e.g. a prop bone with no keys) are intentionally left out here — they'll
// just keep their bind-pose transform from the hierarchy walk in Animator.
void Animation::ReadMissingBones(const aiAnimation* animation,
                                 std::map<std::string, BoneInfo>& boneInfoMap, int& boneCount) {
    for (unsigned int i = 0; i < animation->mNumChannels; ++i) {
        const aiNodeAnim* channel = animation->mChannels[i];
        std::string boneName = channel->mNodeName.C_Str();

        if (boneInfoMap.find(boneName) == boneInfoMap.end()) {
            boneInfoMap[boneName].id = boneCount;
            boneInfoMap[boneName].offsetMatrix = glm::mat4(1.0f);
            boneCount++;
        }

        m_Bones.push_back(Bone(boneName, boneInfoMap[boneName].id, channel));
    }
}