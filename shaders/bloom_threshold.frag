#version 460 core

in vec2 vNDC;
out vec4 FragColor;

uniform sampler2D uSceneColor;
uniform float uThreshold; // luminance level bloom starts kicking in above
uniform float uKnee;      // soft transition width around uThreshold — avoids a hard on/off popping edge

void main() {
    vec2 uv = vNDC * 0.5 + 0.5;
    vec3 color = texture(uSceneColor, uv).rgb;

    // Soft-knee threshold (same approach Unity/Unreal bloom use): blend
    // smoothly over a small range around uThreshold instead of a hard
    // cutoff, so bright cloud edges near the sun fade into bloom gradually
    // rather than getting a visible boundary ring.
    float brightness = max(color.r, max(color.g, color.b));
    float soft = brightness - uThreshold + uKnee;
    soft = clamp(soft, 0.0, 2.0 * uKnee);
    soft = soft * soft / (4.0 * uKnee + 0.0001);
    float contribution = max(soft, brightness - uThreshold);
    contribution /= max(brightness, 0.0001);

    FragColor = vec4(color * contribution, 1.0);
}