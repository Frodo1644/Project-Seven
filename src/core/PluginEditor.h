#pragma once

#include <array>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class SevenLookAndFeel;

class ProjectSevenAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit ProjectSevenAudioProcessorEditor (ProjectSevenAudioProcessor&);
    ~ProjectSevenAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class StringColumn;
    class PanelComponent;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;

    void configureKnob (juce::Slider&, const juce::String& suffix = {}, int decimals = 0);
    void layoutKnobGroup (juce::Rectangle<int> area,
                          juce::Label& caption,
                          juce::Slider& slider,
                          int textBoxHeight);
    void chooseSaveFile();
    void chooseLoadFile();
    void applyTuning (const std::array<int, ps::kNumStrings>& notes);

    ProjectSevenAudioProcessor& processor;

    std::unique_ptr<SevenLookAndFeel> lookAndFeel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetCaption;
    juce::ComboBox presetBox;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton loadButton { "LOAD" };
    std::unique_ptr<ComboBoxAttachment> presetAttachment;

    std::unique_ptr<PanelComponent> stringsPanel;
    std::unique_ptr<PanelComponent> articulationPanel;
    std::unique_ptr<PanelComponent> pickupPanel;
    std::unique_ptr<PanelComponent> voicePanel;

    std::array<std::unique_ptr<StringColumn>, ps::kNumStrings> stringColumns;
    std::array<std::unique_ptr<juce::TextButton>, 4> articulationButtons;

    juce::Label palmMuteCaption;
    juce::Slider palmMuteSlider;
    std::unique_ptr<SliderAttachment> palmMuteAttachment;

    juce::Label pickPositionCaption, pickIntensityCaption, pickupPositionCaption;
    juce::Slider pickPositionSlider, pickIntensitySlider, pickupPositionSlider;
    std::unique_ptr<SliderAttachment> pickPositionAttachment;
    std::unique_ptr<SliderAttachment> pickIntensityAttachment;
    std::unique_ptr<SliderAttachment> pickupPositionAttachment;

    juce::Label polyphonyCaption, bendRangeCaption, outputLevelCaption;
    juce::Slider polyphonySlider, bendRangeSlider, outputLevelSlider;
    std::unique_ptr<SliderAttachment> polyphonyAttachment;
    std::unique_ptr<SliderAttachment> bendRangeAttachment;
    std::unique_ptr<SliderAttachment> outputLevelAttachment;

    juce::Label voicesLabel;
    juce::Label mappingLabel;
    juce::Label keyswitchLabel;
    juce::Label footerLegendLabel;

    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSevenAudioProcessorEditor)
};
