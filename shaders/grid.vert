#version 450 core

layout(location = 0) in vec3 aPos;

uniform mat4 uInvViewProj;

out vec3 vNearPoint;
out vec3 vFarPoint;

vec3 UnprojectPoint(float x, float y, float z, mat4 invViewProj) {
    vec4 unprojected = invViewProj * vec4(x, y, z, 1.0);
    return unprojected.xyz / unprojected.w;
}

void main() {
    vNearPoint = UnprojectPoint(aPos.x, aPos.y, 0.0, uInvViewProj);
    vFarPoint  = UnprojectPoint(aPos.x, aPos.y, 1.0, uInvViewProj);
    gl_Position = vec4(aPos, 1.0);
}