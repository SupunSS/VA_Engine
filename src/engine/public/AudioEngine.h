#pragma once

#include "Types.h"
#include <string>

namespace VAPublic {

// ============================================================================
// AUDIO ENGINE INTERFACE
// ============================================================================

using AudioSourceId = uint32_t;

class IAudioEngine {
public:
    virtual ~IAudioEngine() = default;

    virtual uint32_t LoadClip(const std::string& filePath) = 0;
    virtual void UnloadClip(uint32_t clipId) = 0;

    virtual AudioSourceId PlayClip(uint32_t clipId, const Vec3* position = nullptr,
                                   float volume = 1.0f, float pitch = 1.0f, bool loop = false) = 0;

    virtual void Stop(AudioSourceId sourceId) = 0;
    virtual void Pause(AudioSourceId sourceId) = 0;
    virtual void Resume(AudioSourceId sourceId) = 0;
    virtual bool IsPlaying(AudioSourceId sourceId) = 0;

    virtual void SetVolume(AudioSourceId sourceId, float volume) = 0;
    virtual void SetPosition(AudioSourceId sourceId, const Vec3& position) = 0;
    virtual void SetListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up) = 0;

    virtual void Update(float deltaTime) = 0;
};

} // namespace VAPublic