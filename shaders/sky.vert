#version 460 core

out vec2 vNDC;

void main() {
    // Classic no-vertex-buffer fullscreen triangle: three oversized points
    // that fully cover NDC space once clipped, indexed purely by gl_VertexID.
    vec2 positions[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vNDC = positions[gl_VertexID];
    // z = w = 1.0 places this at the far plane after the perspective divide,
    // so it sits behind all real geometry even if depth testing were re-enabled.
    gl_Position = vec4(vNDC, 1.0, 1.0);
}
