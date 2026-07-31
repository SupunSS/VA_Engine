#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

// Forward-declared rather than included here — keeps miniaudio.h (and its
// implementation macro) confined to AudioEngine.cpp instead of leaking into
// every translation unit that touches Components.h or AudioSystem.h.
struct ma_engine;
struct ma_sound;

using AudioClipId = int32_t;
constexpr AudioClipId kInvalidAudioClip = -1;

// Handle to a persistent, positioned sound instance (vehicle engine loop,
// ambient emitter, etc). Opaque on purpose — components just store this and
// hand it back to AudioEngine to reposition/retune/stop it. Fire-and-forget
// one-shots (footsteps, impacts) don't get a handle at all; see PlayOneShot3D.
using AudioSourceHandle = uint32_t;
constexpr AudioSourceHandle kInvalidAudioSource = 0;

// Thin wrapper around miniaudio's ma_engine. Singleton-style (Get()) since
// there's exactly one audio device for the whole app, same shape as how
// Log:: is used elsewhere in this codebase.
class AudioEngine {
public:
    static AudioEngine& Get();

    bool Initialize();
    void Shutdown();

    // Loads (or returns the cached id for) a sound file. The underlying
    // audio data is only decoded once and shared by every instance played
    // from it, so it's cheap to call this again for a clip you already
    // loaded elsewhere (e.g. every spawned vehicle just re-requests the
    // same engine-loop path and gets the same id back).
    AudioClipId LoadClip(const std::string& path);

    // Ear position/orientation for this frame — every positional sound's
    // panning, attenuation, and (if given a velocity) doppler shift is
    // computed relative to this. Call once per frame before Update().
    void SetListener(const glm::vec3& position, const glm::vec3& forward,
                      const glm::vec3& up, const glm::vec3& velocity = glm::vec3(0.0f));

    void SetMasterVolume(float volume);

    // Fire-and-forget positional one-shot — footsteps, impacts, anything
    // played from a world position that doesn't need to be repositioned or
    // stopped early. Cleans itself up automatically once finished (see
    // Update()).
    void PlayOneShot3D(AudioClipId clip, const glm::vec3& position,
                        float volume = 1.0f, float minDistance = 1.0f, float maxDistance = 40.0f);

    // Persistent positional source for anything that needs to keep playing
    // and be repositioned/retuned frame to frame (vehicle engines, ambient
    // loops). Starts immediately if autoplay is true.
    AudioSourceHandle CreateSource3D(AudioClipId clip, const glm::vec3& position,
                                      bool loop, bool autoplay,
                                      float volume = 1.0f, float minDistance = 1.0f, float maxDistance = 40.0f);

    void SetSourcePosition(AudioSourceHandle handle, const glm::vec3& position);
    void SetSourceVolume(AudioSourceHandle handle, float volume);
    void SetSourcePitch(AudioSourceHandle handle, float pitch);
    void PlaySource(AudioSourceHandle handle);
    void StopSource(AudioSourceHandle handle);

    // Always call this when the owning entity is destroyed (e.g. a vehicle
    // despawning) — otherwise its looping source keeps playing forever with
    // nothing left to reposition it.
    void DestroySource(AudioSourceHandle handle);

    // Call once per frame — sweeps finished one-shots so they don't leak.
    void Update();

private:
    AudioEngine() = default;
    ~AudioEngine() = default;
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    ma_engine* m_engine = nullptr;

    struct LoadedClip {
        std::string Path;
    };
    std::vector<LoadedClip> m_clips;
    std::unordered_map<std::string, AudioClipId> m_clipLookup;

    struct PersistentSource {
        ma_sound* Sound = nullptr;
    };
    std::unordered_map<AudioSourceHandle, PersistentSource> m_sources;
    AudioSourceHandle m_nextHandle = 1;

    // One-shots in flight — polled each Update() and freed once finished.
    std::vector<ma_sound*> m_activeOneShots;
};