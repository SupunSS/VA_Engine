#pragma once

#include "Types.h"
#include <string>

// ============================================================================
// AUDIO ENGINE INTERFACE
// ============================================================================

using AudioSourceId = uint32_t;

/**
 * Audio playback and 3D sound system
 */
class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    /**
     * Load an audio file into memory
     * @param filePath Path to audio file (WAV, FLAC, MP3, etc.)
     * @return Clip ID for playback
     */
    virtual uint32_t LoadClip(const std::string& filePath) = 0;

    /**
     * Unload an audio clip
     * @param clipId Clip ID to unload
     */
    virtual void UnloadClip(uint32_t clipId) = 0;

    /**
     * Play a sound effect
     * @param clipId Loaded clip to play
     * @param position Optional 3D position (nullptr = no 3D)
     * @param volume Volume 0-1
     * @param pitch Pitch multiplier (1.0 = normal)
     * @param loop Loop the sound
     * @return Source ID for control
     */
    virtual AudioSourceId PlayClip(uint32_t clipId, const Vec3* position = nullptr,
                                   float volume = 1.0f, float pitch = 1.0f, bool loop = false) = 0;

    /**
     * Stop a playing sound
     * @param sourceId Source to stop
     */
    virtual void Stop(AudioSourceId sourceId) = 0;

    /**
     * Pause a playing sound
     * @param sourceId Source to pause
     */
    virtual void Pause(AudioSourceId sourceId) = 0;

    /**
     * Resume a paused sound
     * @param sourceId Source to resume
     */
    virtual void Resume(AudioSourceId sourceId) = 0;

    /**
     * Check if a source is playing
     * @param sourceId Source to check
     * @return true if currently playing
     */
    virtual bool IsPlaying(AudioSourceId sourceId) = 0;

    /**
     * Set volume of a playing source
     * @param sourceId Source to modify
     * @param volume New volume 0-1
     */
    virtual void SetVolume(AudioSourceId sourceId, float volume) = 0;

    /**
     * Set 3D position of a source
     * @param sourceId Source to modify
     * @param position New position
     */
    virtual void SetPosition(AudioSourceId sourceId, const Vec3& position) = 0;

    /**
     * Set listener (camera) position
     * @param position Listener position
     * @param forward Forward direction
     * @param up Up direction
     */
    virtual void SetListenerTransform(const Vec3& position, const Vec3& forward, const Vec3& up) = 0;

    /**
     * Update audio system
     * @param deltaTime Time since last update
     */
    virtual void Update(float deltaTime) = 0;
};
