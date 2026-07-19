#version 460 core

in vec2 vNDC;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform sampler2D uBloomTexture;
uniform float uBloomIntensity;
uniform float uExposure;

// Narkowicz's fitted ACES filmic curve.
vec3 ACESFilm(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 uv = vNDC * 0.5 + 0.5;
    vec3 sceneColor = texture(uSceneColor, uv).rgb;
    vec3 bloomColor = texture(uBloomTexture, uv).rgb;

    vec3 color = sceneColor + bloomColor * uBloomIntensity;
     color *= uExposure;

    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}