#include "FootstepClipLoader.h"
#include "../core/AssetPaths.h"
#include <sstream>
#include <iomanip>

namespace FootstepClipLoader {

std::vector<AudioClipId> LoadNumberedSequence(const std::string& prefix, int first, int last,
                                              const std::string& extension, int digits) {
    std::vector<AudioClipId> clips;
    clips.reserve(static_cast<size_t>(last - first + 1));

    for (int i = first; i <= last; ++i) {
        std::ostringstream nameStream;
        nameStream << prefix << std::setw(digits) << std::setfill('0') << i << extension;

        const std::string resolvedPath = AssetPaths::Resolve(AssetPaths::Category::Audio, nameStream.str());
        clips.push_back(AudioEngine::Get().LoadClip(resolvedPath));
    }

    return clips;
}

} // namespace FootstepClipLoader