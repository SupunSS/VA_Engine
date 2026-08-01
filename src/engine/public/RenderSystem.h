#pragma once

#include "Types.h"
#include <string>

// ============================================================================
// RENDER SYSTEM INTERFACE
// ============================================================================

/**
 * Rendering and graphics system
 */
class RenderSystem {
public:
    virtual ~RenderSystem() = default;

    /**
     * Get current camera position
     * @return Camera position
     */
    virtual Vec3 GetCameraPosition() const = 0;

    /**
     * Set camera position
     * @param position New position
     */
    virtual void SetCameraPosition(const Vec3& position) = 0;

    /**
     * Get camera forward direction
     * @return Forward vector
     */
    virtual Vec3 GetCameraForward() const = 0;

    /**
     * Get camera right direction
     * @return Right vector
     */
    virtual Vec3 GetCameraRight() const = 0;

    /**
     * Get camera up direction
     * @return Up vector
     */
    virtual Vec3 GetCameraUp() const = 0;

    /**
     * Set camera look-at
     * @param position Camera position
     * @param target Point to look at
     * @param up Up direction
     */
    virtual void SetCameraLookAt(const Vec3& position, const Vec3& target, const Vec3& up) = 0;

    /**
     * Enable/disable debug rendering
     * @param enabled Whether to show debug visuals
     */
    virtual void SetDebugRendering(bool enabled) = 0;

    /**
     * Check if debug rendering is enabled
     * @return true if debug visuals are shown
     */
    virtual bool IsDebugRenderingEnabled() const = 0;

    /**
     * Get viewport width
     * @return Width in pixels
     */
    virtual int GetViewportWidth() const = 0;

    /**
     * Get viewport height
     * @return Height in pixels
     */
    virtual int GetViewportHeight() const = 0;

    /**
     * Load a 2D texture
     * @param filePath Path to image file
     * @return Texture ID
     */
    virtual uint32_t LoadTexture(const std::string& filePath) = 0;

    /**
     * Load a 3D model
     * @param filePath Path to model file (OBJ, GLTF, etc.)
     * @return Model ID
     */
    virtual uint32_t LoadModel(const std::string& filePath) = 0;
};
