#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

struct AmpTheme
{
    juce::Colour top, bottom, plate, text, accent, knob;
};

class RoadLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AmpTheme theme;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class RoadhouseEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoadhouseEditor (RoadhouseProcessor&);
    ~RoadhouseEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob : public juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& id, const juce::String& name,
              const juce::String& suffix = {}, int decimals = 1);
        void resized() override;
        juce::Slider slider { juce::Slider::RotaryVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void applyTheme();

    RoadhouseProcessor& proc;
    RoadLookAndFeel lnf;

    juce::ComboBox ampBox, cabBox, presetBox;
    juce::TextButton brightBtn { "Bright" }, deepBtn { "Deep" };
    Knob input, gain, bass, mid, treble, presence, master, reverb, rsize, gate, output;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ampAtt, cabAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> brightAtt, deepAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoadhouseEditor)
};
