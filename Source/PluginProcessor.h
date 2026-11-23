#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "AmpEngine.h"

class RoadhouseProcessor : public juce::AudioProcessor
{
public:
    RoadhouseProcessor();
    ~RoadhouseProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Roadhouse"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int kOsOrder = 2;            // 2^2 = 4x oversampling
    static constexpr int kOsFactor = 1 << kOsOrder;

    juce::dsp::Oversampling<float> oversampler { 2, (size_t) kOsOrder,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    roadhouse::AmpCore amp;
    roadhouse::CabSim cab;
    juce::Reverb reverb;
    juce::AudioBuffer<float> work;
    juce::SmoothedValue<float> outSmoothed;

    // noise gate state
    double gateEnv = 0.0, gateGain = 1.0, gateAtk = 0.1, gateRel = 0.001, gateEnvRel = 0.001;
    bool gateOpen = true;

    int maxBlock = 512;
    int currentProgram = 0;
    double sr = 48000.0;

    std::atomic<float>* pAmp, *pInput, *pGain, *pBass, *pMid, *pTreble, *pPres, *pMaster,
                       *pBright, *pDeep, *pCab, *pRvbMix, *pRvbSize, *pGate, *pOutput;

    void processChunk (juce::AudioBuffer<float>& io, int offset, int len, int numIn, int numOut);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoadhouseProcessor)
};
