#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

namespace ps
{
constexpr int kNumStrings = 7;
constexpr int kMaxPolyphony = 24;

enum class Articulation
{
    open = 0,
    palmMute = 1,
    naturalHarmonic = 2,
    pinchHarmonic = 3
};

struct TuningPreset
{
    const char* name;
    std::array<int, kNumStrings> notes; // low string first
};

// Low-to-high string tunings. MIDI note numbers use scientific pitch notation
// (MIDI 60 == C4).
inline constexpr std::array<TuningPreset, 4> kTuningPresets {{
    { "Standard B", { 35, 40, 45, 50, 55, 59, 64 } }, // B1 E2 A2 D3 G3 B3 E4
    { "Drop A",     { 33, 40, 45, 50, 55, 59, 64 } }, // A1 E2 A2 D3 G3 B3 E4
    { "Drop G",     { 31, 38, 43, 48, 53, 57, 62 } }, // G1 D2 G2 C3 F3 A3 D4
    { "Drop F",     { 29, 36, 41, 46, 51, 55, 60 } }  // F1 C2 F2 Bb2 Eb3 G3 C4
}};

constexpr int kPresetCustomIndex = 4;

namespace ParamID
{
inline constexpr const char* tuningPreset = "tuningPreset";
inline constexpr const char* articulation = "articulation";
inline constexpr const char* pickPosition = "pickPosition";
inline constexpr const char* pickIntensity = "pickIntensity";
inline constexpr const char* pickupPosition = "pickupPosition";
inline constexpr const char* palmMute = "palmMute";
inline constexpr const char* polyphony = "polyphony";
inline constexpr const char* bendRange = "bendRange";
inline constexpr const char* outputLevel = "outputLevel";

inline constexpr const char* stringParam (int index) noexcept
{
    // Stable IDs: string1 .. string7 (string1 is the lowest string).
    switch (index)
    {
        case 0: return "string1";
        case 1: return "string2";
        case 2: return "string3";
        case 3: return "string4";
        case 4: return "string5";
        case 5: return "string6";
        case 6: return "string7";
        default: return "stringInvalid";
    }
}
} // namespace ParamID

juce::String midiNoteName (int note);
double midiNoteToFrequency (int note) noexcept;

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Snapshot of everything the audio engine needs for one block.
struct EngineParameters
{
    std::array<int, kNumStrings> tuning { 35, 40, 45, 50, 55, 59, 64 };
    Articulation articulation = Articulation::open;
    float pickPosition = 0.11f;
    float pickIntensity = 0.70f;
    float pickupPosition = 0.12f;
    float palmMute = 0.0f;
    int polyphony = 12;
    int bendRange = 2;
    float outputGain = 0.5f;

    // The coupled physical engine is the instrument. The original 1.x engine is
    // preserved, unmodified, behind this flag so both can be rendered from the
    // same MIDI performance for A/B validation (see docs/EXISTING_ENGINE_AUDIT.md).
    bool useLegacyEngine = false;
};

} // namespace ps
