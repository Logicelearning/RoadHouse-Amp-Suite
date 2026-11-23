#include "PluginEditor.h"

namespace
{
const AmpTheme kThemes[] =
{
    // top,        bottom,      plate,       text,        accent,      knob
    { juce::Colour (0xff1b1b1b), juce::Colour (0xff0b0b0b), juce::Colour (0xffcaa64a), juce::Colour (0xfff3e6bf), juce::Colour (0xffd9b44a), juce::Colour (0xff2a2a2a) }, // Plexi: black + gold
    { juce::Colour (0xff23262b), juce::Colour (0xff0d0f12), juce::Colour (0xffb9c0c8), juce::Colour (0xffe6eaee), juce::Colour (0xffc8d0d8), juce::Colour (0xff1a1c20) }, // Blackface: black + silver
    { juce::Colour (0xffb28a4e), juce::Colour (0xff7a5a30), juce::Colour (0xffe8d7a8), juce::Colour (0xff2b1d0e), juce::Colour (0xff3a2410), juce::Colour (0xff3a2a18) }, // Tweed
    { juce::Colour (0xffd9c9a0), juce::Colour (0xffb39d6b), juce::Colour (0xff6b4a22), juce::Colour (0xff2e2110), juce::Colour (0xff8a4b14), juce::Colour (0xff3b2a14) }, // Dumble clean: sand
    { juce::Colour (0xff3a2c20), juce::Colour (0xff1b130c), juce::Colour (0xffc7925a), juce::Colour (0xfff0dcc0), juce::Colour (0xffe8a860), juce::Colour (0xff2a1e14) }, // ODS: dark amber
};
}

void RoadLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = startAngle + pos * (endAngle - startAngle);

    // tick marks
    g.setColour (theme.text.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float r1 = r + 3.0f, r2 = r + (i % 5 == 0 ? 8.0f : 5.5f);
        g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1,
                    c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, 1.2f);
    }

    // shadow + body
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - r, c.y - r + 3.0f, r * 2.0f, r * 2.0f);
    g.setGradientFill (juce::ColourGradient (theme.knob.brighter (0.5f), c.x - r * 0.6f, c.y - r * 0.8f,
                                             theme.knob.darker (0.6f), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    g.setColour (theme.accent);
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);

    // pointer
    juce::Path p;
    p.addRoundedRectangle (-2.0f, -r + 4.0f, 4.0f, r * 0.5f, 1.5f);
    g.setColour (theme.accent.brighter (0.4f));
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

RoadhouseEditor::Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id,
                             const juce::String& name, const juce::String& suffix, int decimals)
{
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    addAndMakeVisible (label);

    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.setTextValueSuffix (suffix);
    slider.setDoubleClickReturnValue (true, s.getParameter (id)->convertFrom0to1 (s.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
}

void RoadhouseEditor::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

RoadhouseEditor::RoadhouseEditor (RoadhouseProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      input (p.apvts, "inputDb", "INPUT", " dB"), gain (p.apvts, "gain", "GAIN"),
      bass (p.apvts, "bass", "BASS"), mid (p.apvts, "mid", "MID"), treble (p.apvts, "treble", "TREBLE"),
      presence (p.apvts, "presence", "PRESENCE"), master (p.apvts, "master", "MASTER"),
      reverb (p.apvts, "reverbMix", "REVERB", {}, 2), rsize (p.apvts, "reverbSize", "SIZE", {}, 2),
      gate (p.apvts, "gate", "GATE", " dB"), output (p.apvts, "outputDb", "OUTPUT", " dB")
{
    setLookAndFeel (&lnf);

    ampBox.addItemList (p.apvts.getParameter ("amp")->getAllValueStrings(), 1);
    cabBox.addItemList (p.apvts.getParameter ("cab")->getAllValueStrings(), 1);
    for (int i = 0; i < p.getNumPrograms(); ++i) presetBox.addItem (p.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };

    brightBtn.setClickingTogglesState (true);
    deepBtn.setClickingTogglesState (true);

    ampAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "amp", ampBox);
    cabAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "cab", cabBox);
    brightAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "bright", brightBtn);
    deepAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "deep", deepBtn);
    ampBox.onChange = [this] { applyTheme(); };

    for (auto* c : std::initializer_list<juce::Component*> { &ampBox, &cabBox, &presetBox, &brightBtn, &deepBtn,
                                                             &input, &gain, &bass, &mid, &treble, &presence,
                                                             &master, &reverb, &rsize, &gate, &output })
        addAndMakeVisible (c);

    applyTheme();
    setSize (900, 460);
}

RoadhouseEditor::~RoadhouseEditor() { setLookAndFeel (nullptr); }

void RoadhouseEditor::applyTheme()
{
    const int idx = juce::jlimit (0, (int) std::size (kThemes) - 1, (int) *proc.apvts.getRawParameterValue ("amp"));
    lnf.theme = kThemes[idx];
    const auto& t = lnf.theme;

    for (auto* s : { &input, &gain, &bass, &mid, &treble, &presence, &master, &reverb, &rsize, &gate, &output })
    {
        s->label.setColour (juce::Label::textColourId, t.text);
        s->slider.setColour (juce::Slider::textBoxTextColourId, t.text);
        s->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }
    for (auto* cb : { &ampBox, &cabBox, &presetBox })
    {
        cb->setColour (juce::ComboBox::backgroundColourId, t.knob);
        cb->setColour (juce::ComboBox::textColourId, t.text.interpolatedWith (juce::Colours::white, 0.5f));
        cb->setColour (juce::ComboBox::outlineColourId, t.accent);
        cb->setColour (juce::ComboBox::arrowColourId, t.accent);
    }
    for (auto* b : { &brightBtn, &deepBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, t.knob);
        b->setColour (juce::TextButton::buttonOnColourId, t.accent);
        b->setColour (juce::TextButton::textColourOffId, t.text.interpolatedWith (juce::Colours::white, 0.5f));
        b->setColour (juce::TextButton::textColourOnId, t.knob);
    }
    repaint();
}

void RoadhouseEditor::paint (juce::Graphics& g)
{
    const auto& t = lnf.theme;
    g.setGradientFill (juce::ColourGradient (t.top, 0, 0, t.bottom, 0, (float) getHeight(), false));
    g.fillAll();

    // front panel plate
    auto panel = getLocalBounds().toFloat().reduced (16.0f).withTrimmedTop (56.0f);
    g.setColour (t.plate.withAlpha (0.16f));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (t.accent.withAlpha (0.8f));
    g.drawRoundedRectangle (panel, 10.0f, 1.5f);

    // title
    g.setColour (t.text);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("ROADHOUSE", 24, 10, 260, 36, juce::Justification::centredLeft);
    g.setColour (t.accent);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("70s ROCK & BLUES AMP SUITE", 26, 40, 260, 14, juce::Justification::centredLeft);

    // section captions
    g.setColour (t.text.withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    g.drawText ("PREAMP / TONE", 36, 74, 200, 14, juce::Justification::centredLeft);
    g.drawText ("SPEAKER / SPACE / OUT", 36, 262, 260, 14, juce::Justification::centredLeft);
}

void RoadhouseEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56).reduced (16, 12);
    ampBox.setBounds (header.removeFromRight (290));
    header.removeFromRight (10);
    presetBox.setBounds (header.removeFromRight (180));

    auto panel = r.reduced (28, 12).withTrimmedTop (8);
    auto row1 = panel.removeFromTop (182).withTrimmedTop (18);
    const int kw = row1.getWidth() / 7;
    for (auto* k : { &input, &gain, &bass, &mid, &treble, &presence, &master })
        k->setBounds (row1.removeFromLeft (kw));

    panel.removeFromTop (22);
    auto row2 = panel.removeFromTop (160);

    auto toggles = row2.removeFromLeft (kw + 10).reduced (6, 20);
    brightBtn.setBounds (toggles.removeFromTop (34));
    toggles.removeFromTop (12);
    deepBtn.setBounds (toggles.removeFromTop (34));

    auto cabArea = row2.removeFromLeft (kw * 2 - 10).reduced (4, 0);
    cabArea.removeFromTop (30);
    cabBox.setBounds (cabArea.removeFromTop (30));

    for (auto* k : { &reverb, &rsize, &gate, &output })
        k->setBounds (row2.removeFromLeft (kw).withTrimmedTop (0));
}
