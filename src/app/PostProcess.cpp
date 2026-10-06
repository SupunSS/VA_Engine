#include "PostProcess.h"
#include "../core/Assert.h"
#include <algorithm>

namespace {
GLuint CreateHalfResColorTarget(GLuint& fboOut, int width, int height)
{
    glGenFramebuffers(1, &fboOut);
    glBindFramebuffer(GL_FRAMEBUFFER, fboOut);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    ENGINE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "Bloom half-res framebuffer incomplete");

    return tex;
}
} // namespace

void DestroyPostProcessTargets(PostProcessTargets& t)
{
    if (t.hdrColorTexture) glDeleteTextures(1, &t.hdrColorTexture);
    if (t.hdrDepthRBO) glDeleteRenderbuffers(1, &t.hdrDepthRBO);
    if (t.hdrFBO) glDeleteFramebuffers(1, &t.hdrFBO);
    if (t.brightTexture) glDeleteTextures(1, &t.brightTexture);
    if (t.brightFBO) glDeleteFramebuffers(1, &t.brightFBO);
    for (int i = 0; i < 2; ++i) {
        if (t.pingpongTexture[i]) glDeleteTextures(1, &t.pingpongTexture[i]);
        if (t.pingpongFBO[i]) glDeleteFramebuffers(1, &t.pingpongFBO[i]);
    }
    t = PostProcessTargets{};
}

void CreatePostProcessTargets(PostProcessTargets& t, int width, int height)
{
    DestroyPostProcessTargets(t);
    t.fullWidth = std::max(1, width);
    t.fullHeight = std::max(1, height);
    t.halfWidth = std::max(1, width / 2);
    t.halfHeight = std::max(1, height / 2);

    glGenFramebuffers(1, &t.hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, t.hdrFBO);

    glGenTextures(1, &t.hdrColorTexture);
    glBindTexture(GL_TEXTURE_2D, t.hdrColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, t.fullWidth, t.fullHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.hdrColorTexture, 0);

    glGenRenderbuffers(1, &t.hdrDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, t.hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, t.fullWidth, t.fullHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.hdrDepthRBO);

    ENGINE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
                  "HDR scene framebuffer incomplete");

    t.brightTexture = CreateHalfResColorTarget(t.brightFBO, t.halfWidth, t.halfHeight);
    t.pingpongTexture[0] = CreateHalfResColorTarget(t.pingpongFBO[0], t.halfWidth, t.halfHeight);
    t.pingpongTexture[1] = CreateHalfResColorTarget(t.pingpongFBO[1], t.halfWidth, t.halfHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}