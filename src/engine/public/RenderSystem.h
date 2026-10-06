#pragma once

#include "Types.h"
#include <string>

namespace VAPublic {

// ============================================================================
// RENDER SYSTEM INTERFACE
// ============================================================================

class IRenderSystem {
public:
    virtual ~IRenderSystem() = default;

    virtual Vec3 GetCameraPosition() const = 0;
    virtual void SetCameraPosition(const Vec3& position) = 0;

    virtual Vec3 GetCameraForward() const = 0;
    virtual Vec3 GetCameraRight() const = 0;
    virtual Vec3 GetCameraUp() const = 0;

    virtual void SetCameraLookAt(const Vec3& position, const Vec3& target, const Vec3& up) = 0;

    virtual void SetDebugRendering(bool enabled) = 0;
    virtual bool IsDebugRenderingEnabled() const = 0;

    virtual int GetViewportWidth() const = 0;
    virtual int GetViewportHeight() const = 0;

    virtual uint32_t LoadTexture(const std::string& filePath) = 0;
    virtual uint32_t LoadModel(const std::string& filePath) = 0;
};

} // namespace VAPublic