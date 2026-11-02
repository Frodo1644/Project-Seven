#pragma once

#include <array>
#include <optional>

#include <juce_core/juce_core.h>

#include "params/Parameters.h"

namespace ps
{
// Saves and loads user-defined seven-string tunings as small XML files.
class PresetManager
{
public:
    static juce::File getTuningDirectory();
    static juce::String getFileExtension() { return ".ps7t"; }

    static bool saveTuning (const juce::File& file,
                            const std::array<int, kNumStrings>& notes,
                            const juce::String& displayName = {});

    static std::optional<std::array<int, kNumStrings>> loadTuning (const juce::File& file);
};
} // namespace ps
