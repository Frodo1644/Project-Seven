#include "PluginEditor.h"

#include <cmath>

#include "params/PresetManager.h"

namespace
{
const juce::Colour kBackground   { 0xff101113 };
const juce::Colour kBackgroundTop{ 0xff15171b };
const juce::Colour kPanel        { 0xff202328 };
const juce::Colour kPanelRaised  { 0xff262a30 };
const juce::Colour kBorder       { 0xff343840 };
const juce::Colour kText         { 0xffe5e7ea };
const juce::Colour kTextDim      { 0xff9298a2 };
const juce::Colour kAccent       { 0xffb52132 };
const juce::Colour kHighlight    { 0xffe03b4b };
const juce::Colour kGold         { 0xffc58b4a };

juce::Font uiFont (float height, int style = juce::Font::plain, float kerning = 0.0f)
{
    auto font = juce::Font (juce::FontOptions ("Segoe UI", height, style));
    return font.withExtraKerningFactor (kerning);
}

juce::String percentText (double value)
{
    return juce::String (juce::roundToInt (value * 100.0)) + "%";
}
} // namespace

//==============================================================================
class SevenLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SevenLookAndFeel()
    {
        setColourScheme ({ kBackground, kPanel, kPanelRaised, kBorder, kText,
                           kTextDim, kHighlight, kGold, kText });

        setColour (juce::ResizableWindow::backgroundColourId, kBackground);
        setColour (juce::Label::textColourId, kText);
        setColour (juce::TextButton::buttonColourId, kPanel);
        setColour (juce::TextButton::buttonOnColourId, kAccent);
        setColour (juce::TextButton::textColourOffId, kTextDim);
        setColour (juce::TextButton::textColourOnId, kText);

        setColour (juce::ComboBox::backgroundColourId, kPanel);
        setColour (juce::ComboBox::outlineColourId, kBorder);
        setColour (juce::ComboBox::textColourId, kText);
        setColour (juce::ComboBox::arrowColourId, kTextDim);
        setColour (juce::ComboBox::focusedOutlineColourId, kAccent);

        setColour (juce::PopupMenu::backgroundColourId, kPanel);
        setColour (juce::PopupMenu::textColourId, kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
        setColour (juce::PopupMenu::highlightedTextColourId, kText);
        setColour (juce::PopupMenu::headerTextColourId, kGold);

        setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
        setColour (juce::Slider::thumbColourId, kHighlight);
        setColour (juce::Slider::trackColourId, kAccent);
        setColour (juce::Slider::textBoxTextColourId, kText);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.45f));

        setColour (juce::ScrollBar::thumbColourId, kBorder.brighter (0.2f));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        if (label.getFont().getHeight() > 0.0f)
            return label.getFont();
        return uiFont (11.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (uiFont (10.5f, juce::Font::bold, 0.02f));
        label->setJustificationType (juce::Justification::centred);
        return label;
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        auto base = button.getToggleState() ? kAccent : kPanel;

        if (shouldDrawButtonAsDown)
            base = base.darker (0.18f);
        else if (shouldDrawButtonAsHighlighted)
            base = base.brighter (0.08f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, 2.0f);

        g.setColour (button.getToggleState() ? kHighlight.withAlpha (0.8f) : kBorder);
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override
    {
        auto colour = button.getToggleState() ? kText
                                              : (shouldDrawButtonAsHighlighted ? kText : kTextDim);
        if (shouldDrawButtonAsDown)
            colour = colour.darker (0.1f);

        g.setColour (colour);
        g.setFont (uiFont (9.5f, juce::Font::bold, 0.10f));
        g.drawText (button.getButtonText().toUpperCase(),
                    button.getLocalBounds().reduced (4, 0),
                    juce::Justification::centred, false);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                              static_cast<float> (width),
                                              static_cast<float> (height)).reduced (0.5f);
        g.setColour (kPanel);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (box.hasKeyboardFocus (false) ? kAccent : kBorder);
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

        const float centreY = static_cast<float> (height) * 0.5f;
        const float arrowX = static_cast<float> (width) - 15.0f;

        juce::Path arrow;
        arrow.startNewSubPath (arrowX - 4.0f, centreY - 2.0f);
        arrow.lineTo (arrowX, centreY + 2.5f);
        arrow.lineTo (arrowX + 4.0f, centreY - 2.0f);
        g.setColour (kTextDim);
        g.strokePath (arrow, juce::PathStrokeType (1.2f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setFont (uiFont (10.5f, juce::Font::bold, 0.06f));
        label.setBounds (10, 0, box.getWidth() - 28, box.getHeight());
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 3.0f;
        if (radius <= 2.0f)
            return;

        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                     rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (kBorder);
        g.strokePath (backgroundArc, juce::PathStrokeType (2.0f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                    rotaryStartAngle, angle, true);
            g.setColour (slider.isEnabled()
                             ? slider.findColour (juce::Slider::rotarySliderFillColourId)
                             : kBorder);
            g.strokePath (valueArc, juce::PathStrokeType (2.6f,
                                                          juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
        }

        const auto thumbColour = slider.findColour (juce::Slider::thumbColourId);
        const auto inner = centre.getPointOnCircumference (radius * 0.48f, angle);
        const auto outer = centre.getPointOnCircumference (radius * 0.92f, angle);
        g.setColour (thumbColour);
        g.drawLine ({ inner, outer }, 2.0f);

        g.setColour (kPanelRaised);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (centre));
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos);

        const auto trackColour = kBorder;
        const auto fillColour = slider.isEnabled()
                                    ? slider.findColour (juce::Slider::trackColourId)
                                    : kBorder.brighter (0.1f);
        const auto thumbColour = slider.isEnabled()
                                     ? slider.findColour (juce::Slider::thumbColourId)
                                     : kTextDim;

        if (style == juce::Slider::LinearVertical)
        {
            const float centreX = static_cast<float> (x) + static_cast<float> (width) * 0.5f;
            const float top = static_cast<float> (y) + 3.0f;
            const float bottom = static_cast<float> (y + height) - 3.0f;

            g.setColour (trackColour);
            g.drawLine (centreX, top, centreX, bottom, 2.0f);

            if (sliderPos < bottom)
            {
                g.setColour (fillColour);
                g.drawLine (centreX, sliderPos, centreX, bottom, 2.0f);
            }

            auto thumb = juce::Rectangle<float> (centreX - 8.0f, sliderPos - 1.5f, 16.0f, 3.0f);
            g.setColour (thumbColour);
            g.fillRoundedRectangle (thumb, 1.0f);
        }
        else
        {
            const float centreY = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
            const float left = static_cast<float> (x) + 3.0f;
            const float right = static_cast<float> (x + width) - 3.0f;

            g.setColour (trackColour);
            g.drawLine (left, centreY, right, centreY, 2.0f);

            if (sliderPos > left)
            {
                g.setColour (fillColour);
                g.drawLine (left, centreY, sliderPos, centreY, 2.0f);
            }

            auto thumb = juce::Rectangle<float> (sliderPos - 1.5f, centreY - 7.0f, 3.0f, 14.0f);
            g.setColour (thumbColour);
            g.fillRoundedRectangle (thumb, 1.0f);
        }
    }
};

//==============================================================================
class ProjectSevenAudioProcessorEditor::PanelComponent final : public juce::Component
{
public:
    explicit PanelComponent (juce::String panelTitle)
        : title (std::move (panelTitle))
    {
        setInterceptsMouseClicks (false, true);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (kPanel);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        auto titleArea = getLocalBounds().reduced (12, 0).removeFromTop (30);
        g.setColour (kAccent);
        g.fillRect (titleArea.getX(), titleArea.getCentreY() - 2, 3, 4);

        g.setColour (kTextDim);
        g.setFont (uiFont (9.5f, juce::Font::bold, 0.14f));
        g.drawText (title.toUpperCase(),
                    titleArea.withTrimmedLeft (9).withTrimmedRight (4),
                    juce::Justification::centredLeft, false);
    }

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().reduced (12).withTrimmedTop (28).withTrimmedBottom (10);
    }

private:
    juce::String title;
};

//==============================================================================
class ProjectSevenAudioProcessorEditor::StringColumn final : public juce::Component,
                                                              private juce::Slider::Listener
{
public:
    StringColumn (ProjectSevenAudioProcessor& p, int indexIn, juce::LookAndFeel* lf)
        : processor (p), index (indexIn)
    {
        setLookAndFeel (lf);

        numberLabel.setText ("STRING " + juce::String (ps::kNumStrings - index),
                             juce::dontSendNotification);
        numberLabel.setFont (uiFont (8.0f, juce::Font::bold, 0.12f));
        numberLabel.setColour (juce::Label::textColourId, kTextDim);
        numberLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (numberLabel);

        noteLabel.setFont (uiFont (20.0f, juce::Font::bold, 0.01f));
        noteLabel.setColour (juce::Label::textColourId, index == 0 ? kGold : kText);
        noteLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (noteLabel);

        frequencyLabel.setFont (uiFont (8.5f, juce::Font::plain, 0.04f));
        frequencyLabel.setColour (juce::Label::textColourId, kTextDim);
        frequencyLabel.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (frequencyLabel);

        pitchSlider.setSliderStyle (juce::Slider::LinearVertical);
        pitchSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        pitchSlider.setRange (0.0, 127.0, 1.0);
        pitchSlider.setDoubleClickReturnValue (true, static_cast<double> (defaultNote()));
        pitchSlider.setColour (juce::Slider::trackColourId, index == 0 ? kGold : kAccent);
        pitchSlider.setColour (juce::Slider::thumbColourId, kHighlight);
        pitchSlider.setTooltip ("String " + juce::String (ps::kNumStrings - index)
                                + " tuning: drag, mouse-wheel, or use +/-");
        addAndMakeVisible (pitchSlider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.getValueTreeState(), ps::ParamID::stringParam (index), pitchSlider);

        pitchSlider.addListener (this);
        updateReadout();

        downButton.setButtonText ("-");
        upButton.setButtonText ("+");
        downButton.setTooltip ("Lower this string by one semitone");
        upButton.setTooltip ("Raise this string by one semitone");

        downButton.onClick = [this]
        {
            pitchSlider.setValue (pitchSlider.getValue() - 1.0, juce::sendNotificationSync);
        };
        upButton.onClick = [this]
        {
            pitchSlider.setValue (pitchSlider.getValue() + 1.0, juce::sendNotificationSync);
        };

        addAndMakeVisible (downButton);
        addAndMakeVisible (upButton);
    }

    ~StringColumn() override
    {
        pitchSlider.removeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (kPanelRaised.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 2.0f);

        g.setColour (index == 0 ? kGold.withAlpha (0.55f) : kBorder);
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

        g.setColour (index == 0 ? kGold : kAccent);
        g.fillRect (8, 0, getWidth() - 16, 2);

        // Semitone rail: a tick every octave behind the tuning slider.
        const auto track = pitchSlider.getBounds().toFloat();
        const float centreX = track.getCentreX();
        const float top = track.getY() + 4.0f;
        const float bottom = track.getBottom() - 4.0f;

        for (int semitone = 0; semitone <= 120; semitone += 12)
        {
            const float y = juce::jmap (static_cast<float> (semitone), 0.0f, 127.0f,
                                        bottom, top);
            g.setColour (semitone % 24 == 0 ? kTextDim.withAlpha (0.30f)
                                            : kBorder.withAlpha (0.85f));
            const float halfWidth = semitone % 24 == 0 ? 8.0f : 5.0f;
            g.drawLine (centreX - halfWidth, y, centreX + halfWidth, y, 1.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (7, 9);
        numberLabel.setBounds (area.removeFromTop (13));
        noteLabel.setBounds (area.removeFromTop (25));
        frequencyLabel.setBounds (area.removeFromTop (13));
        area.removeFromTop (4);

        auto buttons = area.removeFromBottom (22);
        downButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (1));
        upButton.setBounds (buttons.reduced (1));

        pitchSlider.setBounds (area.reduced (2, 3));
    }

private:
    int defaultNote() const
    {
        return ps::kTuningPresets[0].notes[static_cast<size_t> (index)];
    }

    void sliderValueChanged (juce::Slider*) override
    {
        updateReadout();
    }

    void updateReadout()
    {
        const int note = juce::jlimit (0, 127, juce::roundToInt (pitchSlider.getValue()));
        const double frequency = ps::midiNoteToFrequency (note);

        noteLabel.setText (ps::midiNoteName (note), juce::dontSendNotification);
        frequencyLabel.setText (juce::String (frequency, frequency < 100.0 ? 2 : 1) + " HZ",
                                juce::dontSendNotification);
    }

    ProjectSevenAudioProcessor& processor;
    const int index;
    juce::Label numberLabel, noteLabel, frequencyLabel;
    juce::Slider pitchSlider;
    juce::TextButton downButton, upButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
ProjectSevenAudioProcessorEditor::ProjectSevenAudioProcessorEditor (ProjectSevenAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lookAndFeel = std::make_unique<SevenLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());
    setOpaque (true);

    titleLabel.setText ("PROJECT SEVEN", juce::dontSendNotification);
    titleLabel.setFont (uiFont (26.0f, juce::Font::bold, 0.02f));
    titleLabel.setColour (juce::Label::textColourId, kText);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("PHYSICAL MODELLING SEVEN-STRING  /  CLEAN DI", juce::dontSendNotification);
    subtitleLabel.setFont (uiFont (8.5f, juce::Font::bold, 0.16f));
    subtitleLabel.setColour (juce::Label::textColourId, kTextDim);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    presetCaption.setText ("TUNING", juce::dontSendNotification);
    presetCaption.setFont (uiFont (8.0f, juce::Font::bold, 0.12f));
    presetCaption.setColour (juce::Label::textColourId, kTextDim);
    presetCaption.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (presetCaption);

    presetBox.addItemList ({ "STANDARD B", "DROP A", "DROP G", "DROP F", "CUSTOM" }, 1);
    presetBox.setTooltip ("Built-in tuning presets; editing any string selects Custom");
    addAndMakeVisible (presetBox);
    presetAttachment = std::make_unique<ComboBoxAttachment> (
        processor.getValueTreeState(), ps::ParamID::tuningPreset, presetBox);

    saveButton.setTooltip ("Save the current seven string notes as a .ps7t tuning");
    loadButton.setTooltip ("Load a .ps7t tuning");
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);

    saveButton.onClick = [this] { chooseSaveFile(); };
    loadButton.onClick = [this] { chooseLoadFile(); };

    stringsPanel = std::make_unique<PanelComponent> ("String Set / Tuning");
    articulationPanel = std::make_unique<PanelComponent> ("Articulation");
    pickupPanel = std::make_unique<PanelComponent> ("Plectrum & Pickup");
    voicePanel = std::make_unique<PanelComponent> ("Voice");

    addAndMakeVisible (*stringsPanel);
    addAndMakeVisible (*articulationPanel);
    addAndMakeVisible (*pickupPanel);
    addAndMakeVisible (*voicePanel);

    for (int i = 0; i < ps::kNumStrings; ++i)
    {
        stringColumns[static_cast<size_t> (i)] = std::make_unique<StringColumn> (
            processor, i, lookAndFeel.get());
        stringsPanel->addAndMakeVisible (*stringColumns[static_cast<size_t> (i)]);
    }

    static const char* articulationNames[] = { "OPEN", "PALM", "NAT. HARM", "PINCH" };
    static const char* articulationTips[] = {
        "Open string (keyswitch C0)",
        "Palm muted (keyswitch C#0)",
        "Natural harmonic (keyswitch D0)",
        "Pinch / artificial harmonic (keyswitch D#0)"
    };

    for (int i = 0; i < 4; ++i)
    {
        auto button = std::make_unique<juce::TextButton> (articulationNames[i]);
        button->setClickingTogglesState (false);
        button->setTooltip (articulationTips[i]);
        button->onClick = [this, i]
        {
            if (auto* parameter = processor.getValueTreeState().getParameter (ps::ParamID::articulation))
                parameter->setValueNotifyingHost (
                    parameter->convertTo0to1 (static_cast<float> (i)));
        };
        articulationPanel->addAndMakeVisible (*button);
        articulationButtons[static_cast<size_t> (i)] = std::move (button);
    }

    palmMuteCaption.setText ("PALM MUTE", juce::dontSendNotification);
    palmMuteCaption.setFont (uiFont (8.0f, juce::Font::bold, 0.12f));
    palmMuteCaption.setColour (juce::Label::textColourId, kTextDim);
    articulationPanel->addAndMakeVisible (palmMuteCaption);

    palmMuteSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    palmMuteSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    palmMuteSlider.textFromValueFunction = [] (double value) { return percentText (value); };
    palmMuteSlider.setTooltip ("Continuous palm-mute damping, independent of the articulation");
    articulationPanel->addAndMakeVisible (palmMuteSlider);
    palmMuteAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::palmMute, palmMuteSlider);

    keyswitchLabel.setText ("C0 OPEN   C#0 PALM   D0 NAT   D#0 PINCH", juce::dontSendNotification);
    keyswitchLabel.setFont (uiFont (8.0f, juce::Font::plain, 0.03f));
    keyswitchLabel.setColour (juce::Label::textColourId, kTextDim);
    keyswitchLabel.setJustificationType (juce::Justification::centredLeft);
    articulationPanel->addAndMakeVisible (keyswitchLabel);

    auto addKnob = [this] (juce::Slider& slider, juce::Label& caption,
                           const juce::String& text, PanelComponent& panel,
                           const juce::String& tip)
    {
        caption.setText (text, juce::dontSendNotification);
        caption.setFont (uiFont (8.0f, juce::Font::bold, 0.10f));
        caption.setColour (juce::Label::textColourId, kTextDim);
        caption.setJustificationType (juce::Justification::centred);
        panel.addAndMakeVisible (caption);

        slider.setTooltip (tip);
        panel.addAndMakeVisible (slider);
    };

    pickupPanel->addAndMakeVisible (pickPositionCaption);
    pickupPanel->addAndMakeVisible (pickIntensityCaption);
    pickupPanel->addAndMakeVisible (pickupPositionCaption);

    configureKnob (pickPositionSlider);
    configureKnob (pickIntensitySlider);
    configureKnob (pickupPositionSlider);

    pickPositionSlider.textFromValueFunction = [] (double value) { return percentText (value); };
    pickIntensitySlider.textFromValueFunction = [] (double value) { return percentText (value); };
    pickupPositionSlider.textFromValueFunction = [] (double value) { return percentText (value); };

    addKnob (pickPositionSlider, pickPositionCaption, "PICK POSITION", *pickupPanel,
             "Distance of the pluck from the bridge");
    addKnob (pickIntensitySlider, pickIntensityCaption, "PICK FORCE", *pickupPanel,
             "Hardness of the plectrum excitation");
    addKnob (pickupPositionSlider, pickupPositionCaption, "PICKUP POSITION", *pickupPanel,
             "Magnetic pickup position along the string");

    pickPositionAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::pickPosition, pickPositionSlider);
    pickIntensityAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::pickIntensity, pickIntensitySlider);
    pickupPositionAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::pickupPosition, pickupPositionSlider);

    voicePanel->addAndMakeVisible (polyphonyCaption);
    voicePanel->addAndMakeVisible (bendRangeCaption);
    voicePanel->addAndMakeVisible (outputLevelCaption);

    configureKnob (polyphonySlider);
    configureKnob (bendRangeSlider);
    configureKnob (outputLevelSlider);

    polyphonySlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value));
    };
    bendRangeSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (juce::roundToInt (value)) + " ST";
    };
    outputLevelSlider.textFromValueFunction = [] (double value)
    {
        return juce::String (value, 1) + " DB";
    };

    addKnob (polyphonySlider, polyphonyCaption, "POLYPHONY", *voicePanel,
             "Maximum simultaneous voices");
    addKnob (bendRangeSlider, bendRangeCaption, "BEND RANGE", *voicePanel,
             "Pitch-bend range in semitones");
    addKnob (outputLevelSlider, outputLevelCaption, "OUTPUT", *voicePanel,
             "Clean DI output level");

    polyphonyAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::polyphony, polyphonySlider);
    bendRangeAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::bendRange, bendRangeSlider);
    outputLevelAttachment = std::make_unique<SliderAttachment> (
        processor.getValueTreeState(), ps::ParamID::outputLevel, outputLevelSlider);

    voicesLabel.setFont (uiFont (9.0f, juce::Font::bold, 0.10f));
    voicesLabel.setColour (juce::Label::textColourId, kText);
    voicesLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (voicesLabel);

    mappingLabel.setFont (uiFont (9.0f, juce::Font::plain, 0.04f));
    mappingLabel.setColour (juce::Label::textColourId, kTextDim);
    mappingLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mappingLabel);

    footerLegendLabel.setText ("C0 / C#0 / D0 / D#0  ARTICULATION KEYS",
                               juce::dontSendNotification);
    footerLegendLabel.setFont (uiFont (8.0f, juce::Font::plain, 0.05f));
    footerLegendLabel.setColour (juce::Label::textColourId, kTextDim.withAlpha (0.75f));
    footerLegendLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (footerLegendLabel);

    setSize (1000, 650);
    setResizable (true, true);
    setResizeLimits (900, 585, 1400, 910);
    getConstrainer()->setFixedAspectRatio (1000.0 / 650.0);

    startTimerHz (30);
}

ProjectSevenAudioProcessorEditor::~ProjectSevenAudioProcessorEditor()
{
    stopTimer();
}

void ProjectSevenAudioProcessorEditor::configureKnob (juce::Slider& slider, const juce::String&, int)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 17);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    slider.setColour (juce::Slider::rotarySliderOutlineColourId, kBorder);
    slider.setColour (juce::Slider::thumbColourId, kHighlight);
}

void ProjectSevenAudioProcessorEditor::layoutKnobGroup (juce::Rectangle<int> area,
                                                        juce::Label& caption,
                                                        juce::Slider& slider,
                                                        int)
{
    caption.setBounds (area.removeFromTop (13));
    slider.setBounds (area);
}

void ProjectSevenAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient background (kBackgroundTop, 0.0f, 0.0f,
                                     kBackground, 0.0f, static_cast<float> (getHeight()),
                                     false);
    g.setGradientFill (background);
    g.fillRect (getLocalBounds());

    // Fine top rule and header underline.
    g.setColour (kBorder.withAlpha (0.65f));
    g.drawHorizontalLine (75, 18.0f, static_cast<float> (getWidth() - 18));
    g.drawHorizontalLine (getHeight() - 44, 18.0f, static_cast<float> (getWidth() - 18));

    g.setColour (kAccent);
    g.fillRect (18, 16, 3, 40);

    // Restrained corner ticks keep the industrial frame without decoration.
    g.setColour (kBorder);
    g.drawLine (18.0f, 92.0f, 18.0f, 102.0f);
    g.drawLine (static_cast<float> (getWidth() - 18), 92.0f,
                static_cast<float> (getWidth() - 18), 102.0f);
}

void ProjectSevenAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (18);

    auto header = area.removeFromTop (57);
    area.removeFromTop (18);

    auto footer = area.removeFromBottom (26);
    area.removeFromBottom (18);

    auto brand = header.removeFromLeft (juce::jmin (300, header.getWidth() / 2));
    titleLabel.setBounds (brand.removeFromTop (31));
    subtitleLabel.setBounds (brand.removeFromTop (16));

    header.removeFromLeft (10);
    auto controls = header.removeFromRight (juce::jmin (420, header.getWidth()));
    auto boxRow = controls.withSizeKeepingCentre (controls.getWidth(), 30);
    loadButton.setBounds (boxRow.removeFromRight (58));
    boxRow.removeFromRight (6);
    saveButton.setBounds (boxRow.removeFromRight (58));
    boxRow.removeFromRight (12);
    presetBox.setBounds (boxRow.removeFromRight (juce::jmin (190, boxRow.getWidth() - 50)));
    presetCaption.setBounds (boxRow.removeFromRight (juce::jmin (50, boxRow.getWidth())));

    voicesLabel.setBounds (footer.removeFromLeft (200));
    footerLegendLabel.setBounds (footer.removeFromRight (juce::jmin (330, footer.getWidth())));
    mappingLabel.setBounds (footer);

    const int rightWidth = juce::jlimit (286, 370,
                                         juce::roundToInt (static_cast<float> (area.getWidth()) * 0.325f));
    auto rightColumn = area.removeFromRight (rightWidth);
    area.removeFromRight (12);

    stringsPanel->setBounds (area);

    const int articulationHeight = juce::roundToInt (static_cast<float> (rightColumn.getHeight()) * 0.365f);
    const int pickupHeight = juce::roundToInt (static_cast<float> (rightColumn.getHeight()) * 0.325f);

    articulationPanel->setBounds (rightColumn.removeFromTop (articulationHeight));
    rightColumn.removeFromTop (10);
    pickupPanel->setBounds (rightColumn.removeFromTop (pickupHeight));
    rightColumn.removeFromTop (10);
    voicePanel->setBounds (rightColumn);

    auto stringArea = stringsPanel->getContentBounds();
    const int gap = 6;
    const int columnWidth = (stringArea.getWidth() - gap * (ps::kNumStrings - 1)) / ps::kNumStrings;

    for (int i = 0; i < ps::kNumStrings; ++i)
    {
        auto column = stringArea.removeFromLeft (columnWidth);
        stringColumns[static_cast<size_t> (i)]->setBounds (column);
        if (i < ps::kNumStrings - 1)
            stringArea.removeFromLeft (gap);
    }

    auto articulation = articulationPanel->getContentBounds();
    const int buttonGap = 6;
    const int buttonHeight = juce::jlimit (24, 34,
                                           (articulation.getHeight() - 52 - buttonGap) / 2);
    auto buttonGrid = articulation.removeFromTop (buttonHeight * 2 + buttonGap);
    const int buttonWidth = (buttonGrid.getWidth() - buttonGap) / 2;

    for (int row = 0; row < 2; ++row)
    {
        auto rowArea = (row == 0) ? buttonGrid.removeFromTop (buttonHeight)
                                  : buttonGrid.removeFromTop (buttonHeight);
        articulationButtons[static_cast<size_t> (row * 2)]->setBounds (
            rowArea.removeFromLeft (buttonWidth));
        articulationButtons[static_cast<size_t> (row * 2 + 1)]->setBounds (
            rowArea.removeFromRight (buttonWidth));
    }

    articulation.removeFromTop (8);
    auto palmRow = articulation.removeFromTop (28);
    palmMuteCaption.setBounds (palmRow.removeFromLeft (76));
    palmMuteSlider.setBounds (palmRow);

    articulation.removeFromTop (4);
    keyswitchLabel.setBounds (articulation);

    auto pickup = pickupPanel->getContentBounds();
    const int pickupGap = 6;
    const int pickupWidth = (pickup.getWidth() - pickupGap * 2) / 3;

    auto pickup1 = pickup.removeFromLeft (pickupWidth);
    pickup.removeFromLeft (pickupGap);
    auto pickup2 = pickup.removeFromLeft (pickupWidth);
    pickup.removeFromLeft (pickupGap);
    auto pickup3 = pickup;

    layoutKnobGroup (pickup1, pickPositionCaption, pickPositionSlider, 17);
    layoutKnobGroup (pickup2, pickIntensityCaption, pickIntensitySlider, 17);
    layoutKnobGroup (pickup3, pickupPositionCaption, pickupPositionSlider, 17);

    auto voice = voicePanel->getContentBounds();
    const int voiceGap = 6;
    const int voiceWidth = (voice.getWidth() - voiceGap * 2) / 3;

    auto voice1 = voice.removeFromLeft (voiceWidth);
    voice.removeFromLeft (voiceGap);
    auto voice2 = voice.removeFromLeft (voiceWidth);
    voice.removeFromLeft (voiceGap);
    auto voice3 = voice;

    layoutKnobGroup (voice1, polyphonyCaption, polyphonySlider, 17);
    layoutKnobGroup (voice2, bendRangeCaption, bendRangeSlider, 17);
    layoutKnobGroup (voice3, outputLevelCaption, outputLevelSlider, 17);
}

void ProjectSevenAudioProcessorEditor::timerCallback()
{
    auto& state = processor.getValueTreeState();

    int polyphony = 12;
    if (const auto* value = state.getRawParameterValue (ps::ParamID::polyphony))
        polyphony = juce::roundToInt (value->load());

    voicesLabel.setText ("ACTIVE VOICES  " + juce::String (processor.getActiveVoiceCount())
                             + " / " + juce::String (polyphony),
                         juce::dontSendNotification);

    const int stringIndex = processor.getLastAssignedString();
    if (stringIndex >= 0)
    {
        auto text = "LAST  STRING " + juce::String (ps::kNumStrings - stringIndex)
                  + "   FRET " + juce::String (processor.getLastAssignedFret());

        const int harmonic = processor.getLastHarmonic();
        if (harmonic > 0)
            text = "LAST  STRING " + juce::String (ps::kNumStrings - stringIndex)
                 + "   HARMONIC " + juce::String (harmonic);

        mappingLabel.setText (text, juce::dontSendNotification);
    }
    else
    {
        mappingLabel.setText ("LAST  --", juce::dontSendNotification);
    }

    const int articulation = processor.getCurrentArticulation();
    for (int i = 0; i < 4; ++i)
        articulationButtons[static_cast<size_t> (i)]->setToggleState (
            i == articulation, juce::dontSendNotification);
}

void ProjectSevenAudioProcessorEditor::chooseSaveFile()
{
    auto initial = ps::PresetManager::getTuningDirectory().getChildFile ("Custom Tuning.ps7t");

    fileChooser = std::make_unique<juce::FileChooser> ("Save Project Seven tuning", initial, "*.ps7t");
    auto safeThis = juce::Component::SafePointer<ProjectSevenAudioProcessorEditor> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("ps7t"))
            file = file.withFileExtension ("ps7t");

        std::array<int, ps::kNumStrings> notes {};
        auto& state = safeThis->processor.getValueTreeState();

        for (int i = 0; i < ps::kNumStrings; ++i)
            if (const auto* value = state.getRawParameterValue (ps::ParamID::stringParam (i)))
                notes[static_cast<size_t> (i)] = juce::roundToInt (value->load());

        if (! ps::PresetManager::saveTuning (file, notes, file.getFileNameWithoutExtension()))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Tuning not saved",
                                                    "Project Seven could not write:\n"
                                                        + file.getFullPathName());
        }
    });
}

void ProjectSevenAudioProcessorEditor::chooseLoadFile()
{
    auto initial = ps::PresetManager::getTuningDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Load Project Seven tuning", initial, "*.ps7t");
    auto safeThis = juce::Component::SafePointer<ProjectSevenAudioProcessorEditor> (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        if (safeThis == nullptr)
            return;

        const auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (const auto notes = ps::PresetManager::loadTuning (file))
        {
            safeThis->applyTuning (*notes);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Tuning not loaded",
                                                    "This is not a valid Project Seven tuning file:\n"
                                                        + file.getFullPathName());
        }
    });
}

void ProjectSevenAudioProcessorEditor::applyTuning (const std::array<int, ps::kNumStrings>& notes)
{
    for (int i = 0; i < ps::kNumStrings; ++i)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (ps::ParamID::stringParam (i)))
        {
            const auto note = juce::jlimit (0, 127, notes[static_cast<size_t> (i)]);
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (static_cast<float> (note)));
        }
    }
}
