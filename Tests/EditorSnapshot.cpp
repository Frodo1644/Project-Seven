#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

// Offline development tool: renders the editor to a PNG without a host.
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    ProjectSevenAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
        return 1;

    const int width = argc > 2 ? juce::String (argv[2]).getIntValue() : 1000;
    const int height = argc > 3 ? juce::String (argv[3]).getIntValue() : 650;
    const int preset = argc > 4 ? juce::String (argv[4]).getIntValue() : -1;
    editor->setSize (width, height);

    if (preset >= 0)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (ps::ParamID::tuningPreset))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (preset)));

        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    }

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.85f), 0);
    processor.processBlock (buffer, midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    const auto output = argc > 1 ? juce::File (juce::String (argv[1]))
                                 : juce::File::getCurrentWorkingDirectory().getChildFile ("editor_preview.png");

    if (auto stream = output.createOutputStream())
    {
        juce::PNGImageFormat png;
        if (png.writeImageToStream (image, *stream))
            return 0;
    }

    return 2;
}
