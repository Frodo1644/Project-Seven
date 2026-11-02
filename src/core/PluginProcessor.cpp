#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "params/PresetManager.h"
#include "params/TuningManager.h"

namespace
{
constexpr int kKeyswitchOpen = 12;       // C0
constexpr int kKeyswitchPalm = 13;       // C#0
constexpr int kKeyswitchNatural = 14;    // D0
constexpr int kKeyswitchPinch = 15;      // D#0

ps::Articulation articulationFromKey (int note) noexcept
{
    switch (note)
    {
        case kKeyswitchOpen: return ps::Articulation::open;
        case kKeyswitchPalm: return ps::Articulation::palmMute;
        case kKeyswitchNatural: return ps::Articulation::naturalHarmonic;
        case kKeyswitchPinch: return ps::Articulation::pinchHarmonic;
        default: return ps::Articulation::open;
    }
}

bool isKeyswitch (int note) noexcept
{
    return note >= kKeyswitchOpen && note <= kKeyswitchPinch;
}
} // namespace

ProjectSevenAudioProcessor::BusesProperties ProjectSevenAudioProcessor::getBusesProperties()
{
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

ProjectSevenAudioProcessor::ProjectSevenAudioProcessor()
    : AudioProcessor (getBusesProperties()),
      apvts (*this, nullptr, "ProjectSevenState", ps::createParameterLayout())
{
    apvts.addParameterListener (ps::ParamID::tuningPreset, this);
    for (int i = 0; i < ps::kNumStrings; ++i)
        apvts.addParameterListener (ps::ParamID::stringParam (i), this);
}

ProjectSevenAudioProcessor::~ProjectSevenAudioProcessor()
{
    apvts.removeParameterListener (ps::ParamID::tuningPreset, this);
    for (int i = 0; i < ps::kNumStrings; ++i)
        apvts.removeParameterListener (ps::ParamID::stringParam (i), this);

    cancelPendingUpdate();
}

void ProjectSevenAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    setLatencySamples (0);

    engine.prepare (sampleRate, samplesPerBlock);
    engine.setParameters (readParameters());
    engine.setPitchBend (pitchBend);

    // A generous fixed scratch buffer lets processBlock() render oversized host
    // blocks in chunks without ever allocating on the audio thread.
    monoBuffer.setSize (1, juce::jmax (8192, samplesPerBlock), false, true, false);
    monoBuffer.clear();
}

void ProjectSevenAudioProcessor::releaseResources()
{
    engine.reset();
}

bool ProjectSevenAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

ps::EngineParameters ProjectSevenAudioProcessor::readParameters() const
{
    ps::EngineParameters result;

    for (int i = 0; i < ps::kNumStrings; ++i)
    {
        if (const auto* value = apvts.getRawParameterValue (ps::ParamID::stringParam (i)))
            result.tuning[static_cast<size_t> (i)] = juce::jlimit (0, 127, juce::roundToInt (value->load()));
    }

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::articulation))
        result.articulation = static_cast<ps::Articulation> (
            juce::jlimit (0, 3, juce::roundToInt (value->load())));

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::pickPosition))
        result.pickPosition = juce::jlimit (0.02f, 0.32f, value->load());

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::pickIntensity))
        result.pickIntensity = juce::jlimit (0.0f, 1.0f, value->load());

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::pickupPosition))
        result.pickupPosition = juce::jlimit (0.04f, 0.35f, value->load());

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::palmMute))
        result.palmMute = juce::jlimit (0.0f, 1.0f, value->load());

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::polyphony))
        result.polyphony = juce::jlimit (1, ps::kMaxPolyphony, juce::roundToInt (value->load()));

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::bendRange))
        result.bendRange = juce::jlimit (0, 12, juce::roundToInt (value->load()));

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::outputLevel))
        result.outputGain = juce::Decibels::decibelsToGain (juce::jlimit (-24.0f, 6.0f, value->load()));

    return result;
}

ps::Articulation ProjectSevenAudioProcessor::effectiveArticulation() const
{
    const int momentary = momentaryArticulation.load (std::memory_order_relaxed);
    if (momentary >= 0 && momentary <= 3)
        return static_cast<ps::Articulation> (momentary);

    if (const auto* value = apvts.getRawParameterValue (ps::ParamID::articulation))
        return static_cast<ps::Articulation> (
            juce::jlimit (0, 3, juce::roundToInt (value->load())));

    return ps::Articulation::open;
}

void ProjectSevenAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, 0, numSamples);

    auto parameters = readParameters();
    parameters.articulation = effectiveArticulation();
    engine.setParameters (parameters);

    // The instrument is mono by nature. Both output channels carry the same DI
    // signal; this is not stereo widening.
    const int chunkCapacity = monoBuffer.getNumSamples();
    if (chunkCapacity <= 0)
    {
        midiMessages.clear();
        return;
    }

    for (int blockOffset = 0; blockOffset < numSamples; blockOffset += chunkCapacity)
    {
        const int chunkSize = juce::jmin (chunkCapacity, numSamples - blockOffset);
        auto* mono = monoBuffer.getWritePointer (0);
        int position = 0;

        for (const auto metadata : midiMessages)
        {
            const int eventPosition = metadata.samplePosition - blockOffset;
            if (eventPosition < 0 || eventPosition >= chunkSize)
                continue;

            if (eventPosition > position)
            {
                engine.render (mono + position, eventPosition - position);
                position = eventPosition;
            }

            handleMidiEvent (metadata.getMessage());
        }

        if (position < chunkSize)
            engine.render (mono + position, chunkSize - position);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.copyFrom (channel, blockOffset, monoBuffer, 0, 0, chunkSize);
    }

    midiMessages.clear();
}

void ProjectSevenAudioProcessor::handleMidiEvent (const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        const int note = message.getNoteNumber();
        const float velocity = message.getFloatVelocity();

        if (isKeyswitch (note))
            beginKeyswitch (note);
        else
            engine.noteOn (note, velocity);
    }
    else if (message.isNoteOff())
    {
        const int note = message.getNoteNumber();
        if (isKeyswitch (note))
            endKeyswitch (note);
        else
            engine.noteOff (note);
    }
    else if (message.isPitchWheel())
    {
        pitchBend = juce::jlimit (-1.0f, 1.0f,
                                  (static_cast<float> (message.getPitchWheelValue()) - 8192.0f) / 8192.0f);
        engine.setPitchBend (pitchBend);
    }
    else if (message.isController())
    {
        const int controller = message.getControllerNumber();
        const int value = message.getControllerValue();

        if (controller == 64)
        {
            engine.setSustainPedal (value >= 64);
        }
        else if (controller == 20)
        {
            // Latched articulation selection. Applied through the parameter so
            // the editor and host automation stay in sync.
            const int articulation = value < 32 ? 0 : (value < 64 ? 1 : (value < 96 ? 2 : 3));
            pendingArticulation.store (articulation);
            triggerAsyncUpdate();
        }
        else if (controller == 120) // All Sound Off
        {
            engine.allNotesOff (true);
        }
        else if (controller == 123) // All Notes Off
        {
            engine.allNotesOff (false);
        }
        else if (controller == 121) // Reset All Controllers
        {
            pitchBend = 0.0f;
            engine.setPitchBend (0.0f);
            engine.setSustainPedal (false);
            engine.allNotesOff (false);
        }
    }
}

void ProjectSevenAudioProcessor::beginKeyswitch (int midiNote)
{
    momentaryArticulation.store (static_cast<int> (articulationFromKey (midiNote)),
                                 std::memory_order_relaxed);
}

void ProjectSevenAudioProcessor::endKeyswitch (int midiNote)
{
    if (momentaryArticulation.load (std::memory_order_relaxed)
        == static_cast<int> (articulationFromKey (midiNote)))
        momentaryArticulation.store (-1, std::memory_order_relaxed);
}

void ProjectSevenAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (syncing.load())
        return;

    if (parameterID == ps::ParamID::tuningPreset)
    {
        const int preset = juce::roundToInt (newValue);
        if (preset >= 0 && preset < static_cast<int> (ps::kTuningPresets.size()))
        {
            pendingPreset.store (preset);
            triggerAsyncUpdate();
        }
    }
    else if (parameterID.startsWith ("string"))
    {
        pendingCustom.store (true);
        triggerAsyncUpdate();
    }
}

void ProjectSevenAudioProcessor::handleAsyncUpdate()
{
    if (syncing.exchange (true))
        return;

    struct SyncGuard
    {
        std::atomic<bool>& flag;
        ~SyncGuard() { flag.store (false); }
    } guard { syncing };

    const int preset = pendingPreset.exchange (-1);

    if (preset >= 0 && preset < static_cast<int> (ps::kTuningPresets.size()))
    {
        pendingCustom.store (false);

        for (int i = 0; i < ps::kNumStrings; ++i)
        {
            if (auto* parameter = apvts.getParameter (ps::ParamID::stringParam (i)))
            {
                const auto note = ps::kTuningPresets[static_cast<size_t> (preset)]
                                      .notes[static_cast<size_t> (i)];
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (note)));
            }
        }
    }
    else if (pendingCustom.exchange (false))
    {
        if (auto* parameter = apvts.getParameter (ps::ParamID::tuningPreset))
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (ps::kPresetCustomIndex)));
    }

    const int articulation = pendingArticulation.exchange (-1);
    if (articulation >= 0 && articulation <= 3)
    {
        if (auto* parameter = apvts.getParameter (ps::ParamID::articulation))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (articulation)));
    }
}

void ProjectSevenAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ProjectSevenAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    if (! xml->hasTagName (apvts.state.getType()))
        return;

    syncing.store (true);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    pendingPreset.store (-1);
    pendingCustom.store (false);
    pendingArticulation.store (-1);
    syncing.store (false);

    // Do not rely on a pending UI update: the next process block reads the
    // restored values directly.
    engine.setParameters (readParameters());
}

juce::AudioProcessorEditor* ProjectSevenAudioProcessor::createEditor()
{
    return new ProjectSevenAudioProcessorEditor (*this);
}

