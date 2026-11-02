#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/SevenAudioEngine.h"
#include "params/Parameters.h"

class ProjectSevenAudioProcessor final : public juce::AudioProcessor,
                                         private juce::AudioProcessorValueTreeState::Listener,
                                         private juce::AsyncUpdater
{
public:
    ProjectSevenAudioProcessor();
    ~ProjectSevenAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Project Seven"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

    int getActiveVoiceCount() const noexcept { return engine.getActiveVoiceCount(); }
    int getLastAssignedString() const noexcept { return engine.getLastAssignedString(); }
    int getLastAssignedFret() const noexcept { return engine.getLastAssignedFret(); }
    int getLastHarmonic() const noexcept { return engine.getLastHarmonic(); }
    int getCurrentArticulation() const noexcept { return static_cast<int> (effectiveArticulation()); }

    static juce::AudioProcessor::BusesProperties getBusesProperties();

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    ps::EngineParameters readParameters() const;
    ps::Articulation effectiveArticulation() const;
    void handleMidiEvent (const juce::MidiMessage& message);
    void beginKeyswitch (int midiNote);
    void endKeyswitch (int midiNote);

    juce::AudioProcessorValueTreeState apvts;
    ps::SevenAudioEngine engine;
    juce::AudioBuffer<float> monoBuffer;

    std::atomic<bool> syncing { false };
    std::atomic<int> pendingPreset { -1 };
    std::atomic<bool> pendingCustom { false };
    std::atomic<int> pendingArticulation { -1 };

    std::atomic<int> momentaryArticulation { -1 };
    float pitchBend = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSevenAudioProcessor)
};
