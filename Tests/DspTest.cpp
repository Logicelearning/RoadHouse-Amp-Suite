// Offline sanity test: renders a plucked-guitar-like signal through every amp and
// writes raw float32 files for analysis. Developer use only.
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <cmath>

static void setP (RoadhouseProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0; const int block = 512; const int total = (int) (sr * 2.0);

    std::vector<float> in ((size_t) total);
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        double s = 0;
        for (int h = 1; h <= 10; ++h) s += std::sin (2 * M_PI * 164.81 * h * t) / h;   // E3 pluck
        in[(size_t) i] = (float) (s * std::exp (-2.2 * t) * 0.18);
    }
    FILE* f = fopen ("/tmp/dsp_in.f32", "wb"); fwrite (in.data(), 4, in.size(), f); fclose (f);

    const float gains[] = { 2.f, 5.f, 8.f };
    for (int amp = 0; amp < roadhouse::kNumAmps; ++amp)
        for (float g : gains)
        {
            RoadhouseProcessor p;
            p.prepareToPlay (sr, block);
            setP (p, "amp", (float) amp); setP (p, "gain", g);
            setP (p, "reverbMix", 0.f);

            juce::AudioBuffer<float> buf (2, block);
            std::vector<float> out; out.reserve ((size_t) total);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < total; pos += block)
            {
                const int n = std::min (block, total - pos);
                buf.setSize (2, n, false, false, true);
                buf.clear();
                for (int i = 0; i < n; ++i) buf.setSample (0, i, in[(size_t) (pos + i)]);
                p.processBlock (buf, midi);
                for (int i = 0; i < n; ++i) out.push_back (buf.getSample (0, i));
            }
            char name[128]; snprintf (name, sizeof name, "/tmp/dsp_out_a%d_g%d.f32", amp, (int) g);
            f = fopen (name, "wb"); fwrite (out.data(), 4, out.size(), f); fclose (f);
        }

    // reverb + mono/stereo, odd block sizes, extreme settings: just check for NaN/inf
    {
        RoadhouseProcessor p; p.prepareToPlay (44100.0, 256);
        setP (p, "amp", 4.f); setP (p, "gain", 10.f); setP (p, "master", 10.f);
        setP (p, "reverbMix", 1.f); setP (p, "gate", -40.f); setP (p, "bright", 1.f); setP (p, "deep", 1.f);
        juce::AudioBuffer<float> buf (2, 333); juce::MidiBuffer midi; bool bad = false;
        juce::Random rnd (1);
        for (int b = 0; b < 200; ++b)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 333; ++i) buf.setSample (c, i, rnd.nextFloat() * 2.f - 1.f);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 333; ++i) if (! std::isfinite (buf.getSample (c, i))) bad = true;
        }
        printf ("extreme-settings finite: %s\n", bad ? "FAIL" : "ok");
    }
    // presets
    {
        RoadhouseProcessor p; p.prepareToPlay (48000.0, 512);
        for (int i = 0; i < p.getNumPrograms(); ++i) { p.setCurrentProgram (i); }
        printf ("presets applied: %d\n", p.getNumPrograms());
    }
    return 0;
}
