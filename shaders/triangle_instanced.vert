#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aTangent;
// locations 4/5 (bone IDs/weights) exist on the shared VAO layout but aren't
// read here — instancing is only ever used for static (non-skinned) props.
layout (location = 6) in mat4 aInstanceModel; // consumes locations 6,7,8,9 (a mat4 attribute always spans 4 consecutive locations)

uniform mat4 uView;
uniform mat4 uProjection;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

void main() {
    FragPos = vec3(aInstanceModel * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(aInstanceModel))) * aNormal;
    TexCoord = aTexCoord;

    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
