#version 450 core

in vec3 vNearPoint;
in vec3 vFarPoint;

uniform mat4 uView;
uniform mat4 uProj;
uniform float uNear;
uniform float uFar;

out vec4 FragColor;

vec4 Grid(vec3 fragPos3D, float scale, vec4 lineColor) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 gridLine = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(gridLine.x, gridLine.y);
    vec4 color = lineColor;
    color.a *= 1.0 - min(line, 1.0);
    return color;
}

float ComputeDepth(vec3 pos) {
    vec4 clipSpace = uProj * uView * vec4(pos, 1.0);
    return (clipSpace.z / clipSpace.w);
}

float ComputeLinearDepth(vec3 pos) {
    vec4 clipSpace = uProj * uView * vec4(pos, 1.0);
    float clipDepth = (clipSpace.z / clipSpace.w) * 2.0 - 1.0;
    float linearDepth = (2.0 * uNear * uFar) / (uFar + uNear - clipDepth * (uFar - uNear));
    return linearDepth / uFar;
}

void main() {
    float t = -vNearPoint.y / (vFarPoint.y - vNearPoint.y);
    vec3 fragPos3D = vNearPoint + t * (vFarPoint - vNearPoint);

    gl_FragDepth = (ComputeDepth(fragPos3D) + 1.0) * 0.5;

    float linearDepth = ComputeLinearDepth(fragPos3D);
    float fading = max(0.0, 0.5 - linearDepth);

    vec4 fineGrid = Grid(fragPos3D, 1.0, vec4(0.4, 0.4, 0.4, 1.0));
    vec4 boldGrid = Grid(fragPos3D, 0.1, vec4(0.7, 0.7, 0.7, 1.0));

    FragColor = fineGrid + boldGrid;
    FragColor.a *= fading * float(t > 0.0);
}