#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 uViewPos;
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;
uniform vec3 uAlbedoTint;

void main() {
    vec3 norm = normalize(Normal);
    vec3 ambient = 0.2 * uAlbedoTint;

    vec3 lightDir = normalize(-uDirLightDirection);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uDirLightColor * uAlbedoTint;

    FragColor = vec4(ambient + diffuse, 1.0);
}