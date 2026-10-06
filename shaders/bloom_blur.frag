#version 460 core

in vec2 vNDC;
out vec4 FragColor;

uniform sampler2D uSourceTexture;
uniform vec2 uTexelSize; // 1.0 / source texture resolution
uniform int uHorizontal; // 1 = blur along X this pass, 0 = blur along Y

void main() {
    vec2 uv = vNDC * 0.5 + 0.5;

    // 9-tap Gaussian (sigma ~2), separable into two 1D passes — same
    // visual result as a full 2D kernel for far fewer texture samples.
    const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

    vec2 direction = (uHorizontal == 1) ? vec2(uTexelSize.x, 0.0) : vec2(0.0, uTexelSize.y);

    vec3 result = texture(uSourceTexture, uv).rgb * weights[0];
    for (int i = 1; i < 5; ++i) {
        vec2 offset = direction * float(i);
        result += texture(uSourceTexture, uv + offset).rgb * weights[i];
        result += texture(uSourceTexture, uv - offset).rgb * weights[i];
    }

    FragColor = vec4(result, 1.0);
}