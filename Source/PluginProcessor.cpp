#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Preset
{
    const char* name;
    int amp; float inDb, gain, bass, mid, treble, pres, master;
    bool bright, deep; int cab; float rvb, size, gate, out;
};

// amp: 0 Plexi Clean, 1 Blackface Drive, 2 Tweed Overdrive, 3 Dumble Clean, 4 ODS
// cab: 0 Off, 1 1x12, 2 2x12, 3 4x12 closed, 4 4x12 vintage
const Preset kPresets[] =
{
    { "Init",                      0, 0, 4.0f, 5, 5, 5, 5, 5,  false, false, 3, 0.00f, 0.5f, -90, 0 },
    { "Plexi Clean Chime",         0, 0, 3.5f, 5, 6, 6.5f, 5, 5, true,  false, 3, 0.15f, 0.4f, -90, 0 },
    { "Plexi Edge of Breakup",     0, 0, 6.5f, 5.5f, 6, 6, 5.5f, 6, false, false, 3, 0.12f, 0.4f, -80, 0 },
    { "Blackface Glass",           1, 0, 3.0f, 4.5f, 4, 6.5f, 4, 5, true,  false, 1, 0.28f, 0.5f, -90, 0 },
    { "Blackface Cranked Blues",   1, 0, 7.5f, 5, 4.5f, 6, 4, 6.5f, false, false, 1, 0.22f, 0.5f, -80, 0 },
    { "Tweed Bark",                2, 0, 6.0f, 4.5f, 6, 5.5f, 4, 6, false, false, 1, 0.12f, 0.35f, -78, 0 },
    { "Dumble Silk Clean",         3, 0, 3.5f, 5.5f, 5.5f, 5, 5, 5, false, true,  2, 0.2f, 0.5f, -90, 0 },
    { "ODS Singing Lead",          4, 0, 6.5f, 5, 6.5f, 5, 5, 6, false, false, 2, 0.22f, 0.55f, -75, 0 },
    { "ODS Smoky Rhythm",          4, 0, 4.5f, 6, 5.5f, 4.5f, 4, 5, false, true,  2, 0.15f, 0.45f, -80, 0 },
};
constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));

const juce::StringArray kCabNames { "Off", "1x12 Open Back", "2x12 Open Back", "4x12 Closed Back", "4x12 Vintage" };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout RoadhouseProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    juce::StringArray ampNames;
    for (int i = 0; i < roadhouse::kNumAmps; ++i) ampNames.add (roadhouse::getAmpName (i));

    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "amp", 1 }, "Amp", ampNames, 0));
    l.add (std::make_unique<FP> (juce::ParameterID { "inputDb", 1 }, "Input",    NR (-12.f, 12.f, 0.1f), 0.f, Attr().withLabel ("dB")));
    l.add (std::make_unique<FP> (juce::ParameterID { "gain", 1 },    "Gain",     NR (0.f, 10.f, 0.01f), 4.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "bass", 1 },    "Bass",     NR (0.f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "mid", 1 },     "Mid",      NR (0.f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "treble", 1 },  "Treble",   NR (0.f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "presence", 1 },"Presence", NR (0.f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "master", 1 },  "Master",   NR (0.f, 10.f, 0.01f), 5.f));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "bright", 1 }, "Bright", false));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "deep", 1 },   "Deep", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "cab", 1 }, "Cabinet", kCabNames, 3));
    l.add (std::make_unique<FP> (juce::ParameterID { "reverbMix", 1 },  "Reverb",      NR (0.f, 1.f, 0.01f), 0.f));
    l.add (std::make_unique<FP> (juce::ParameterID { "reverbSize", 1 }, "Reverb Size", NR (0.f, 1.f, 0.01f), 0.5f));
    l.add (std::make_unique<FP> (juce::ParameterID { "gate", 1 },    "Gate",     NR (-90.f, -30.f, 0.1f), -90.f, Attr().withLabel ("dB")));
    l.add (std::make_unique<FP> (juce::ParameterID { "outputDb", 1 },"Output",   NR (-24.f, 12.f, 0.1f), 0.f, Attr().withLabel ("dB")));
    return l;
}

RoadhouseProcessor::RoadhouseProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pAmp = get ("amp"); pInput = get ("inputDb"); pGain = get ("gain"); pBass = get ("bass");
    pMid = get ("mid"); pTreble = get ("treble"); pPres = get ("presence"); pMaster = get ("master");
    pBright = get ("bright"); pDeep = get ("deep"); pCab = get ("cab");
    pRvbMix = get ("reverbMix"); pRvbSize = get ("reverbSize"); pGate = get ("gate"); pOutput = get ("outputDb");
}

bool RoadhouseProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in  = l.getMainInputChannelSet();
    const auto out = l.getMainOutputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::mono()) return false;
    return true;
}

void RoadhouseProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (samplesPerBlock, 64);
    work.setSize (2, maxBlock);
    work.clear();

    oversampler.initProcessing ((size_t) maxBlock);
    oversampler.reset();
    setLatencySamples (juce::roundToInt (oversampler.getLatencyInSamples()));

    amp.prepare (sampleRate * kOsFactor);
    cab.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    outSmoothed.reset (sampleRate, 0.02);
    outSmoothed.setCurrentAndTargetValue (1.0f);

    gateEnv = 0.0; gateGain = 1.0; gateOpen = true;
    gateEnvRel = 1.0 - std::exp (-1.0 / (0.05  * sampleRate));
    gateAtk    = 1.0 - std::exp (-1.0 / (0.002 * sampleRate));
    gateRel    = 1.0 - std::exp (-1.0 / (0.08  * sampleRate));
}

void RoadhouseProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int off = 0; off < n; off += maxBlock)
        processChunk (buffer, off, juce::jmin (maxBlock, n - off), numIn, numOut);
}

void RoadhouseProcessor::processChunk (juce::AudioBuffer<float>& io, int off, int len, int numIn, int numOut)
{
    // ---- gather parameters ----
    roadhouse::AmpParams p;
    p.amp = (int) pAmp->load();   p.inputDb = pInput->load();
    p.gain = pGain->load();       p.bass = pBass->load();   p.mid = pMid->load();
    p.treble = pTreble->load();   p.presence = pPres->load(); p.master = pMaster->load();
    p.bright = pBright->load() > 0.5f; p.deep = pDeep->load() > 0.5f;
    p.outputDb = 0.0f; // applied below at base rate

    amp.setParams (p);
    const int cabIdx = (int) pCab->load();
    if (cabIdx != cab.getCab()) cab.setCab (cabIdx);
    outSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));

    // ---- copy input into the 2-channel work buffer (mono input feeds both) ----
    work.setSize (2, maxBlock, false, false, true);
    for (int ch = 0; ch < 2; ++ch)
        work.copyFrom (ch, 0, io, juce::jmin (ch, juce::jmax (0, numIn - 1)), off, len);

    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);

    // ---- noise gate ----
    const float gateDb = pGate->load();
    if (gateDb > -89.0f)
    {
        const double thrOpen  = juce::Decibels::decibelsToGain ((double) gateDb);
        const double thrClose = thrOpen * 0.7;
        for (int i = 0; i < len; ++i)
        {
            const double a = std::max (std::abs ((double) L[i]), std::abs ((double) R[i]));
            gateEnv += (a > gateEnv ? 1.0 : gateEnvRel) * (a - gateEnv);
            if (gateOpen  && gateEnv < thrClose) gateOpen = false;
            if (! gateOpen && gateEnv > thrOpen) gateOpen = true;
            const double target = gateOpen ? 1.0 : 0.0;
            gateGain += (target - gateGain) * (target > gateGain ? gateAtk : gateRel);
            L[i] *= (float) gateGain;
            R[i] *= (float) gateGain;
        }
    }

    // ---- amp (4x oversampled) ----
    juce::dsp::AudioBlock<float> block (work);
    auto sub = block.getSubBlock (0, (size_t) len);
    auto osBlock = oversampler.processSamplesUp (sub);
    amp.process (osBlock.getChannelPointer (0), osBlock.getChannelPointer (1), (int) osBlock.getNumSamples());
    oversampler.processSamplesDown (sub);

    // ---- cab ----
    cab.process (L, R, len);

    // ---- reverb ----
    const float mix = pRvbMix->load();
    if (mix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = 0.3f + 0.65f * pRvbSize->load();
        rp.damping    = 0.55f;
        rp.wetLevel   = mix * 0.25f;
        rp.dryLevel   = 0.5f * (1.0f - 0.4f * mix);
        rp.width      = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, len);
    }

    // ---- output level + write back ----
    for (int i = 0; i < len; ++i)
    {
        const float g = outSmoothed.getNextValue();
        L[i] *= g; R[i] *= g;
    }

    for (int ch = 0; ch < numOut; ++ch)
        io.copyFrom (ch, off, work, juce::jmin (ch, 1), 0, len);
}

int RoadhouseProcessor::getNumPrograms() { return kNumPresets; }
const juce::String RoadhouseProcessor::getProgramName (int i) { return kPresets[juce::jlimit (0, kNumPresets - 1, i)].name; }

void RoadhouseProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    currentProgram = index;
    const auto& pr = kPresets[index];
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };
    set ("amp", (float) pr.amp);   set ("inputDb", pr.inDb); set ("gain", pr.gain);
    set ("bass", pr.bass);         set ("mid", pr.mid);      set ("treble", pr.treble);
    set ("presence", pr.pres);     set ("master", pr.master);
    set ("bright", pr.bright ? 1.f : 0.f); set ("deep", pr.deep ? 1.f : 0.f);
    set ("cab", (float) pr.cab);   set ("reverbMix", pr.rvb); set ("reverbSize", pr.size);
    set ("gate", pr.gate);         set ("outputDb", pr.out);
}

void RoadhouseProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RoadhouseProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* RoadhouseProcessor::createEditor() { return new RoadhouseEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new RoadhouseProcessor(); }
