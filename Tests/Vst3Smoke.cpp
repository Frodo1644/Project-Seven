#include <cmath>
#include <cstdio>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    if (! condition)
    {
        ++failures;
        std::printf ("FAIL: %s\n", message);
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (! std::isfinite (buffer.getSample (channel, sample)))
                return false;

    return true;
}

float peak (const juce::AudioBuffer<float>& buffer)
{
    float result = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        result = juce::jmax (result, buffer.getMagnitude (channel, 0, buffer.getNumSamples()));
    return result;
}
} // namespace

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::printf ("usage: ProjectSevenVst3Smoke <path to Project Seven.vst3>\n");
        return 2;
    }

    const juce::String pluginPath (argv[1]);
    std::printf ("VST3 smoke test\n");
    std::printf ("bundle: %s\n", pluginPath.toRawUTF8());

    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formats.getFormats())
        format->findAllTypesForFile (descriptions, pluginPath);

    check (! descriptions.isEmpty(), "VST3 bundle exposes a plugin description");
    if (descriptions.isEmpty())
        return 1;

    const auto& description = *descriptions.getFirst();
    std::printf ("name: %s\n", description.name.toRawUTF8());

    check (description.isInstrument, "Plugin is described as an instrument");
    check (description.pluginFormatName.containsIgnoreCase ("VST3"),
           "Plugin is a VST3");

    juce::String error;
    auto instance = formats.createPluginInstance (description, 48000.0, 512, error);
    check (instance != nullptr, "VST3 instance created");
    if (instance == nullptr)
    {
        std::printf ("create error: %s\n", error.toRawUTF8());
        return 1;
    }

    check (instance->getName().containsIgnoreCase ("Project Seven"),
           "Instance reports the Project Seven name");
    check (instance->getTotalNumInputChannels() == 0, "Instrument has no audio inputs");
    check (instance->getTotalNumOutputChannels() == 2, "Instrument has a stereo output");
    check (instance->acceptsMidi(), "Instrument accepts MIDI");
    check (instance->getTailLengthSeconds() >= 1.0, "Instrument exposes a sustain tail");

    instance->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.85f), 0);
    instance->processBlock (buffer, midi);

    check (allFinite (buffer), "First hosted block is finite");
    check (peak (buffer) > 0.0001f, "Hosted note-on produces audio");

    double maximum = 0.0;
    bool finite = true;

    for (int block = 0; block < 120; ++block)
    {
        buffer.clear();
        juce::MidiBuffer empty;
        instance->processBlock (buffer, empty);
        finite = finite && allFinite (buffer);
        maximum = juce::jmax (maximum, static_cast<double> (peak (buffer)));
    }

    check (finite, "Two seconds of hosted audio stay finite");
    check (maximum < 4.0, "Hosted output remains bounded");
    std::printf ("hosted peak: %.4f\n", maximum);

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    check (state.getSize() > 0, "Hosted state serialises");
    instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    buffer.clear();
    juce::MidiBuffer resetMidi;
    instance->processBlock (buffer, resetMidi);
    check (allFinite (buffer), "Hosted instance renders after state restore");

    if (instance->hasEditor())
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (instance->createEditorIfNeeded());
        check (editor != nullptr, "Hosted editor can be created");
        if (editor != nullptr)
        {
            editor->setSize (1000, 650);
            check (editor->getWidth() == 1000 && editor->getHeight() == 650,
                   "Hosted editor accepts its size");
        }
    }
    else
    {
        check (false, "Hosted instrument declares an editor");
    }

    std::printf ("%s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
