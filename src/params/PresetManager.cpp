#include "params/PresetManager.h"

namespace ps
{
juce::File PresetManager::getTuningDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Project Seven")
                   .getChildFile ("Tunings");
    dir.createDirectory();
    return dir;
}

bool PresetManager::saveTuning (const juce::File& file,
                                const std::array<int, kNumStrings>& notes,
                                const juce::String& displayName)
{
    juce::XmlElement root ("PROJECT_SEVEN_TUNING");
    root.setAttribute ("version", 1);
    root.setAttribute ("name", displayName.isNotEmpty() ? displayName : file.getFileNameWithoutExtension());

    for (int i = 0; i < kNumStrings; ++i)
    {
        auto* stringElement = root.createNewChildElement ("STRING");
        stringElement->setAttribute ("index", i + 1);
        stringElement->setAttribute ("note", notes[static_cast<size_t> (i)]);
    }

    auto target = file;
    if (! target.hasFileExtension (getFileExtension()))
        target = target.withFileExtension (getFileExtension());

    return root.writeTo (target);
}

std::optional<std::array<int, kNumStrings>> PresetManager::loadTuning (const juce::File& file)
{
    if (! file.existsAsFile())
        return std::nullopt;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("PROJECT_SEVEN_TUNING"))
        return std::nullopt;

    std::array<int, kNumStrings> notes {};
    std::array<bool, kNumStrings> found {};

    for (auto* child : xml->getChildIterator())
    {
        if (! child->hasTagName ("STRING"))
            continue;

        const int index = child->getIntAttribute ("index", 0);
        if (index < 1 || index > kNumStrings)
            continue;

        const int note = child->getIntAttribute ("note", -1);
        if (note < 0 || note > 127)
            continue;

        notes[static_cast<size_t> (index - 1)] = note;
        found[static_cast<size_t> (index - 1)] = true;
    }

    for (bool value : found)
        if (! value)
            return std::nullopt;

    return notes;
}
} // namespace ps
