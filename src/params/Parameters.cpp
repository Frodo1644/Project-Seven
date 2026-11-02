#include "params/Parameters.h"

#include <cmath>

namespace ps
{
juce::String midiNoteName (int note)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto clamped = juce::jlimit (0, 127, note);
    const auto octave = clamped / 12 - 1;
    return juce::String (names[clamped % 12]) + juce::String (octave);
}

double midiNoteToFrequency (int note) noexcept
{
    return 440.0 * std::pow (2.0, (static_cast<double> (note) - 69.0) / 12.0);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::tuningPreset, 1 }, "Tuning Preset",
        StringArray { "Standard B", "Drop A", "Drop G", "Drop F", "Custom" }, 0));

    for (int i = 0; i < kNumStrings; ++i)
    {
        const auto defaultNote = kTuningPresets[0].notes[static_cast<size_t> (i)];
        layout.add (std::make_unique<AudioParameterInt> (
            ParameterID { ParamID::stringParam (i), 1 },
            "String " + String (kNumStrings - i),
            0, 127, defaultNote,
            AudioParameterIntAttributes().withStringFromValueFunction (
                [] (int value, int) { return midiNoteName (value); })));
    }

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { ParamID::articulation, 1 }, "Articulation",
        StringArray { "Open", "Palm Muted", "Natural Harmonic", "Pinch Harmonic" }, 0));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::pickPosition, 1 }, "Pick Position",
        NormalisableRange<float> (0.02f, 0.32f, 0.001f), 0.11f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return String (roundToInt (value * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::pickIntensity, 1 }, "Pick Intensity",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.70f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return String (roundToInt (value * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::pickupPosition, 1 }, "Pickup Position",
        NormalisableRange<float> (0.04f, 0.35f, 0.001f), 0.12f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return String (roundToInt (value * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::palmMute, 1 }, "Palm Mute",
        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return String (roundToInt (value * 100.0f)) + "%"; })));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamID::polyphony, 1 }, "Polyphony", 1, kMaxPolyphony, 12));

    layout.add (std::make_unique<AudioParameterInt> (
        ParameterID { ParamID::bendRange, 1 }, "Pitch Bend Range", 0, 12, 2,
        AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int value, int) { return String (value) + " st"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamID::outputLevel, 1 }, "Output Level",
        NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float value, int) { return String (value, 1) + " dB"; })));

    return layout;
}

} // namespace ps
