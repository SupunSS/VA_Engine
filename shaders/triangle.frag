#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D uAlbedoMap;
uniform int uHasAlbedoMap;
uniform vec3 uAlbedoTint;
uniform vec3 uViewPos;

// directional light
uniform vec3 uDirLightDirection;
uniform vec3 uDirLightColor;

// point light
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;

void main() {
    vec3 norm = normalize(Normal);
    vec3 albedo = uAlbedoTint;
    if (uHasAlbedoMap == 1) {
        albedo *= texture(uAlbedoMap, TexCoord).rgb;
    }

    // ambient
    vec3 ambient = 0.15 * albedo;

    // directional light (diffuse)
    vec3 lightDir = normalize(-uDirLightDirection);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 dirDiffuse = diff * uDirLightColor * albedo;

    // point light (diffuse + attenuation)
    vec3 pointDir = normalize(uPointLightPos - FragPos);
    float pointDiff = max(dot(norm, pointDir), 0.0);
    float distance = length(uPointLightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    vec3 pointDiffuse = pointDiff * uPointLightColor * albedo * attenuation;

    vec3 result = ambient + dirDiffuse + pointDiffuse;
    FragColor = vec4(result, 1.0);
}