#include "Texture.h"
#include "../core/Log.h"
#include "../core/Assert.h"
#include <glad/glad.h>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace {
constexpr int kFallbackTextureSize = 64;  // final texture resolution, in pixels
constexpr int kFallbackCheckerCells = 8;  // 8x8 grid of alternating squares

// Unreal-style "missing texture" checkerboard: bright pink alternating with
// mid gray, generated procedurally so no fallback asset file is needed.
std::vector<unsigned char> GenerateCheckerboardFallback() {
    std::vector<unsigned char> pixels(static_cast<size_t>(kFallbackTextureSize) * kFallbackTextureSize * 3);
    const int cellSize = kFallbackTextureSize / kFallbackCheckerCells;

    constexpr unsigned char kPink[3] = {255, 0, 200};
    constexpr unsigned char kGray[3] = {90, 90, 90};

    for (int y = 0; y < kFallbackTextureSize; ++y) {
        for (int x = 0; x < kFallbackTextureSize; ++x) {
            const int cellX = x / cellSize;
            const int cellY = y / cellSize;
            const bool isPink = (cellX + cellY) % 2 == 0;
            const unsigned char* color = isPink ? kPink : kGray;

            const size_t index = (static_cast<size_t>(y) * kFallbackTextureSize + x) * 3;
            pixels[index + 0] = color[0];
            pixels[index + 1] = color[1];
            pixels[index + 2] = color[2];
        }
    }

    return pixels;
}
}

Texture::Texture(const std::string& path) {
    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (data == nullptr) {
        Log::Warn("Failed to load texture {}, using checkerboard fallback", path);
        const std::vector<unsigned char> fallbackPixels = GenerateCheckerboardFallback();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, kFallbackTextureSize, kFallbackTextureSize, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, fallbackPixels.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        return;
    }

    GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    Log::Info("Texture loaded: {} ({}x{}, {} channels)", path, width, height, channels);
}

Texture::~Texture() {
    glDeleteTextures(1, &m_textureID);
}

void Texture::Bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_textureID);
}