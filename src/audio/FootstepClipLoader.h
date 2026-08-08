#pragma once
#include <vector>
#include "AudioEngine.h"

// Loads a numbered sequence of footstep audio files (e.g.
// "Steps_floor-001.ogg" through "Steps_floor-021.ogg") into a vector of
// AudioClipIds, for use with FootstepAudio's random-pick pool.
namespace FootstepClipLoader {

// prefix/extension example: LoadNumberedSequence("sfx/Steps_floor-", 1, 21, ".ogg", 3)
// generates sfx/Steps_floor-001.ogg ... sfx/Steps_floor-021.ogg (digits=3
// controls zero-padding width) and loads each via AudioEngine::LoadClip,
// resolved through AssetPaths::Category::Audio.
std::vector<AudioClipId> LoadNumberedSequence(const std::string& prefix, int first, int last,
                                              const std::string& extension, int digits);

} // namespace FootstepClipLoader