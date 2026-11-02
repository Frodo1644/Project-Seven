#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

extern "C"
{
    // VST3 entry point - must be exported for hosts to detect this plugin
    // This is the function that VST3 hosts look for in the DLL
    // JUCE's juce_add_plugin should normally handle this, but we'll add it
    // explicitly to ensure the export is present
    __declspec(dllexport) int JUCE_CALLTYPE VST3Main(void** pHost,
                                                      void** hostState)
    {
        // Return 0 to indicate successful initialization
        juce::ignoreUnused(pHost, hostState);
        return 0;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProjectSevenAudioProcessor();
}
