// This is the one and only translation unit that gets miniaudio's actual
// implementation compiled in — every other file just sees the declarations
// via miniaudio.h's header portion (pulled in transitively through nothing;
// AudioEngine.h forward-declares ma_engine/ma_sound so nobody else needs to
// touch this file at all).
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "AudioEngine.h"
#include "../core/Log.h"

namespace {
ma_sound* AllocSound() { return new ma_sound(); }
}

AudioEngine& AudioEngine::Get() {
    static AudioEngine instance;
    return instance;
}

bool AudioEngine::Initialize() {
    m_engine = new ma_engine();
    ma_result result = ma_engine_init(nullptr, m_engine);
    if (result != MA_SUCCESS) {
        Log::Info("AudioEngine: ma_engine_init failed (code {})", static_cast<int>(result));
        delete m_engine;
        m_engine = nullptr;
        return false;
    }

    Log::Info("AudioEngine initialized ({} channels, {} Hz).",
              ma_engine_get_channels(m_engine), ma_engine_get_sample_rate(m_engine));
    return true;
}

void AudioEngine::Shutdown() {
    for (auto* sound : m_activeOneShots) {
        ma_sound_uninit(sound);
        delete sound;
    }
    m_activeOneShots.clear();

    for (auto& [handle, source] : m_sources) {
        if (source.Sound) {
            ma_sound_uninit(source.Sound);
            delete source.Sound;
        }
    }
    m_sources.clear();

    if (m_engine) {
        ma_engine_uninit(m_engine);
        delete m_engine;
        m_engine = nullptr;
    }

    Log::Info("AudioEngine shut down.");
}

AudioClipId AudioEngine::LoadClip(const std::string& path) {
    auto it = m_clipLookup.find(path);
    if (it != m_clipLookup.end()) {
        return it->second;
    }

    AudioClipId id = static_cast<AudioClipId>(m_clips.size());
    m_clips.push_back(LoadedClip{ path });
    m_clipLookup[path] = id;
    return id;
}

void AudioEngine::SetListener(const glm::vec3& position, const glm::vec3& forward,
                               const glm::vec3& up, const glm::vec3& velocity) {
    if (!m_engine) return;

    ma_engine_listener_set_position(m_engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(m_engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(m_engine, 0, up.x, up.y, up.z);
    ma_engine_listener_set_velocity(m_engine, 0, velocity.x, velocity.y, velocity.z);
}

void AudioEngine::SetMasterVolume(float volume) {
    if (!m_engine) return;
    ma_engine_set_volume(m_engine, volume);
}

void AudioEngine::PlayOneShot3D(AudioClipId clip, const glm::vec3& position,
                                 float volume, float minDistance, float maxDistance) {
    if (!m_engine || clip < 0 || clip >= static_cast<AudioClipId>(m_clips.size())) {
        return;
    }

    ma_sound* sound = AllocSound();
    ma_result result = ma_sound_init_from_file(
        m_engine, m_clips[clip].Path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        Log::Info("AudioEngine: failed to play one-shot '{}' (code {})", m_clips[clip].Path, static_cast<int>(result));
        delete sound;
        return;
    }

    ma_sound_set_position(sound, position.x, position.y, position.z);
    ma_sound_set_volume(sound, volume);
    ma_sound_set_min_distance(sound, minDistance);
    ma_sound_set_max_distance(sound, maxDistance);
    ma_sound_start(sound);

    m_activeOneShots.push_back(sound);
}

AudioSourceHandle AudioEngine::CreateSource3D(AudioClipId clip, const glm::vec3& position,
                                               bool loop, bool autoplay,
                                               float volume, float minDistance, float maxDistance) {
    if (!m_engine || clip < 0 || clip >= static_cast<AudioClipId>(m_clips.size())) {
        return kInvalidAudioSource;
    }

    ma_sound* sound = AllocSound();
    ma_result result = ma_sound_init_from_file(
        m_engine, m_clips[clip].Path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        Log::Info("AudioEngine: failed to create source for '{}' (code {})", m_clips[clip].Path, static_cast<int>(result));
        delete sound;
        return kInvalidAudioSource;
    }

    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_position(sound, position.x, position.y, position.z);
    ma_sound_set_volume(sound, volume);
    ma_sound_set_min_distance(sound, minDistance);
    ma_sound_set_max_distance(sound, maxDistance);

    if (autoplay) {
        ma_sound_start(sound);
    }

    AudioSourceHandle handle = m_nextHandle++;
    m_sources[handle] = PersistentSource{ sound };
    return handle;
}

void AudioEngine::SetSourcePosition(AudioSourceHandle handle, const glm::vec3& position) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end() || !it->second.Sound) return;
    ma_sound_set_position(it->second.Sound, position.x, position.y, position.z);
}

void AudioEngine::SetSourceVolume(AudioSourceHandle handle, float volume) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end() || !it->second.Sound) return;
    ma_sound_set_volume(it->second.Sound, volume);
}

void AudioEngine::SetSourcePitch(AudioSourceHandle handle, float pitch) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end() || !it->second.Sound) return;
    ma_sound_set_pitch(it->second.Sound, pitch);
}

void AudioEngine::PlaySource(AudioSourceHandle handle) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end() || !it->second.Sound) return;
    ma_sound_start(it->second.Sound);
}

void AudioEngine::StopSource(AudioSourceHandle handle) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end() || !it->second.Sound) return;
    ma_sound_stop(it->second.Sound);
}

void AudioEngine::DestroySource(AudioSourceHandle handle) {
    auto it = m_sources.find(handle);
    if (it == m_sources.end()) return;

    if (it->second.Sound) {
        ma_sound_uninit(it->second.Sound);
        delete it->second.Sound;
    }
    m_sources.erase(it);
}

void AudioEngine::Update() {
    if (!m_engine) return;

    // miniaudio doesn't auto-free ma_sound objects created via
    // ma_sound_init_from_file — this is the pool cleanup for the
    // fire-and-forget one-shot path. Swap-and-pop since order doesn't matter.
    for (size_t i = 0; i < m_activeOneShots.size(); ) {
        ma_sound* sound = m_activeOneShots[i];
        if (ma_sound_at_end(sound)) {
            ma_sound_uninit(sound);
            delete sound;
            m_activeOneShots[i] = m_activeOneShots.back();
            m_activeOneShots.pop_back();
        } else {
            ++i;
        }
    }
}