#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 4) in ivec4 aBoneIDs;
layout (location = 5) in vec4 aBoneWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

const int MAX_BONES = 100;
uniform mat4 uBoneMatrices[MAX_BONES];

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    vec4 skinnedPosition = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    float weightSum = 0.0;

    for (int i = 0; i < 4; ++i) {
        int boneID = aBoneIDs[i];
        float weight = aBoneWeights[i];
        if (boneID < 0 || boneID >= MAX_BONES || weight <= 0.0) continue;

        skinnedPosition += (uBoneMatrices[boneID] * vec4(aPos, 1.0)) * weight;
        skinnedNormal    += (mat3(uBoneMatrices[boneID]) * aNormal) * weight;
        weightSum += weight;
    }

    vec4 finalPosition;
    vec3 finalNormal;

    // Static meshes never populate BoneIDs (Mesh.h defaults them to -1), so
    // weightSum naturally stays 0 for them — this branch is what lets the
    // exact same shader draw both the skinned player and every static prop
    // without needing a separate "is this mesh animated" uniform.
    if (weightSum > 0.0001) {
        finalPosition = skinnedPosition;
        finalNormal = skinnedNormal;
    } else {
        finalPosition = vec4(aPos, 1.0);
        finalNormal = aNormal;
    }

    FragPos = vec3(uModel * finalPosition);
    Normal = mat3(transpose(inverse(uModel))) * finalNormal;
    TexCoord = aTexCoord;

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}