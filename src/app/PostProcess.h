#pragma once
#include <glad/glad.h>

// HDR scene target + half-resolution bright/ping-pong targets used by the bloom pass.
struct PostProcessTargets {
    GLuint hdrFBO = 0, hdrColorTexture = 0, hdrDepthRBO = 0;
    GLuint brightFBO = 0, brightTexture = 0;
    GLuint pingpongFBO[2] = { 0, 0 };
    GLuint pingpongTexture[2] = { 0, 0 };
    int fullWidth = 0, fullHeight = 0;
    int halfWidth = 0, halfHeight = 0;
};

void DestroyPostProcessTargets(PostProcessTargets& targets);
void CreatePostProcessTargets(PostProcessTargets& targets, int width, int height);