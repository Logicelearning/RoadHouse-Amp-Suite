#pragma once
#include "Biquad.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace roadhouse
{
constexpr int kNumAmps = 5;
constexpr int kNumCabs = 5;

struct AmpParams
{
    int    amp      = 0;
    float  inputDb  = 0.0f;
    float  gain     = 4.0f;   // 0..10
    float  bass     = 5.0f;   // 0..10 (5 = flat)
    float  mid      = 5.0f;
    float  treble   = 5.0f;
    float  presence = 5.0f;
    float  master   = 5.0f;
    bool   bright   = false;
    bool   deep     = false;
    float  outputDb = 0.0f;
};

// Everything that makes one amp sound like itself.
struct Voicing
{
    const char* name;
    // preamp
    double gMin, gMax;          // gain-knob range (linear), mapped exponentially
    double bias1, bias2;        // tube asymmetry per gain stage (even harmonics)
    int    stages;              // 1 or 2 cascaded gain stages
    double stage2Gain;
    double inHP, interHP, interLP;
    // tone stack
    double bassHz, bassRange, bassBase;
    double midHz, midQ, midRange, midBase;
    double trebHz, trebRange;
    double brightHz, brightDb;
    double deepHz, deepDb;
    // power amp
    double presHz, presRange;
    double powerDrive, sag;
    double outTrimDb;
};

inline const Voicing& getVoicing (int i)
{
    static const Voicing v[kNumAmps] =
    {
        // name                                  gMin  gMax  b1    b2    st  s2g  inHP interHP interLP
        { "Plexi Clean (Marshall-style)",        0.8,  9.0,  0.06, 0.00, 1,  1.0, 70,  90,  9000,
          110, 9.0, 0.0,   650, 0.8, 8.0,  2.0,   3200, 10.0,  4500, 4.5,   90, 4.0,   4500, 6.0,  0.9, 0.18, -4.5 },

        { "Blackface Drive (Fender-style)",      1.0, 16.0,  0.08, 0.05, 2,  1.6, 60, 110,  8000,
           90, 10.0, 0.0,  500, 0.7, 6.0, -4.0,   3800, 10.0,  5200, 5.0,   80, 3.5,   5500, 4.0,  1.1, 0.25, -4.0 },

        { "Tweed Overdrive (Fender-style)",      1.5, 24.0,  0.18, 0.10, 2,  2.4, 70, 130,  5800,
          100, 8.0, 0.0,   800, 0.8, 7.0,  3.0,   2800,  8.0,  3800, 2.5,   80, 4.0,   3500, 4.0,  1.4, 0.50, -5.5 },

        { "Steel String Clean (Dumble-style)",   0.5,  6.0,  0.04, 0.00, 1,  1.0, 55,  80,  8000,
          100, 9.0, 0.0,   750, 0.6, 7.0,  3.0,   2800,  8.0,  4000, 3.0,   80, 5.5,   3500, 5.0,  0.7, 0.10, -4.5 },

        { "Overdrive Special (Dumble-style)",    1.5, 36.0,  0.12, 0.08, 2,  3.0, 70, 140,  6000,
          120, 9.0, 0.0,   900, 0.7, 7.0,  5.0,   2500,  8.0,  3500, 2.0,   85, 5.0,   3000, 4.0,  1.2, 0.40, -6.0 },
    };
    return v[std::clamp (i, 0, kNumAmps - 1)];
}

inline const char* getAmpName (int i) { return getVoicing (i).name; }

inline double tube (double x, double bias) noexcept
{
    return std::tanh (x + bias) - std::tanh (bias);
}

//==============================================================================
// Preamp -> tone stack -> presence -> power amp (with sag). Runs at the
// oversampled rate. Two channels.
class AmpCore
{
public:
    void prepare (double osRate)
    {
        fs = osRate;
        smoothCoef = 1.0 - std::exp (-1.0 / (0.02 * fs));
        sagAtk     = 1.0 - std::exp (-1.0 / (0.005 * fs));
        sagRel     = 1.0 - std::exp (-1.0 / (0.12 * fs));
        const double R = std::exp (-2.0 * 3.14159265358979323846 * 8.0 / fs);
        for (auto& c : ch) { c.dc1.R = c.dc2.R = R; }
        reset();
        setParams (AmpParams{});
        preGain = preTarget; inLin = inTarget; pd = pdTarget; outLin = outTarget;
    }

    void reset()
    {
        for (auto& c : ch)
        {
            c.inHP.reset(); c.bright.reset(); c.interHP.reset(); c.interLP.reset();
            c.bass.reset(); c.mid.reset(); c.treb.reset(); c.deep.reset(); c.pres.reset();
            c.dc1 = {}; c.dc2 = {};
            c.dc1.R = c.dc2.R = std::exp (-2.0 * 3.14159265358979323846 * 8.0 / fs);
            c.sag = 0.0;
        }
    }

    void setParams (const AmpParams& p)
    {
        v = &getVoicing (p.amp);
        const double gainN = std::clamp ((double) p.gain, 0.0, 10.0) / 10.0;

        preTarget = v->gMin * std::pow (v->gMax / v->gMin, gainN);
        inTarget  = std::pow (10.0, p.inputDb / 20.0);
        pdTarget  = v->powerDrive * (0.6 + 0.28 * std::clamp ((double) p.master, 0.0, 10.0));
        outTarget = std::pow (10.0, (v->outTrimDb + p.outputDb) / 20.0);
        bright = p.bright;
        deep   = p.deep;

        const double brightDb = v->brightDb * (1.0 - 0.8 * gainN); // bright cap matters less when gain is up
        const double bassDb   = (p.bass   - 5.0) / 5.0 * v->bassRange + v->bassBase;
        const double midDb    = (p.mid    - 5.0) / 5.0 * v->midRange  + v->midBase;
        const double trebDb   = (p.treble - 5.0) / 5.0 * v->trebRange;
        const double presDb   = (p.presence - 5.0) / 5.0 * v->presRange;

        for (auto& c : ch)
        {
            c.inHP.highPass (fs, v->inHP, 0.707);
            c.bright.highShelf (fs, v->brightHz, 0.707, brightDb);
            c.interHP.highPass (fs, v->interHP, 0.707);
            c.interLP.lowPass (fs, v->interLP, 0.707);
            c.bass.lowShelf (fs, v->bassHz, 0.6, bassDb);
            c.mid.peak (fs, v->midHz, v->midQ, midDb);
            c.treb.highShelf (fs, v->trebHz, 0.6, trebDb);
            c.deep.lowShelf (fs, v->deepHz, 0.6, v->deepDb);
            c.pres.highShelf (fs, v->presHz, 0.7, presDb);
        }
    }

    void process (float* L, float* R, int n)
    {
        float* data[2] = { L, R };
        const double normPd = 1.0; // normalisation is computed per-sample below

        for (int i = 0; i < n; ++i)
        {
            preGain += (preTarget - preGain) * smoothCoef;
            inLin   += (inTarget  - inLin)   * smoothCoef;
            pd      += (pdTarget  - pd)      * smoothCoef;
            outLin  += (outTarget - outLin)  * smoothCoef;
            const double tanhPd = std::tanh (pd) * normPd;

            for (int k = 0; k < 2; ++k)
            {
                auto& c = ch[(size_t) k];
                double x = (double) data[k][i] * inLin;

                // --- preamp ---
                x = c.inHP.process (x);
                if (bright) x = c.bright.process (x);
                x *= preGain;
                x = tube (x, v->bias1);
                x = c.dc1.process (x);
                x = c.interHP.process (x);
                x = c.interLP.process (x);
                if (v->stages == 2)
                {
                    x *= v->stage2Gain;
                    x = tube (x, v->bias2);
                    x = c.dc2.process (x);
                }

                // --- tone stack ---
                x = c.bass.process (x);
                x = c.mid.process (x);
                x = c.treb.process (x);
                if (deep) x = c.deep.process (x);
                x = c.pres.process (x);

                // --- power amp with supply sag ---
                const double a = std::abs (x);
                c.sag += (a > c.sag ? sagAtk : sagRel) * (a - c.sag);
                const double d = pd / (1.0 + v->sag * 1.5 * c.sag);
                double y = std::tanh (x * d) / tanhPd;

                y *= outLin;
                if (! std::isfinite (y)) y = 0.0;
                data[k][i] = (float) y;
            }
        }
    }

private:
    struct DC
    {
        double x1 = 0.0, y1 = 0.0, R = 0.999;
        inline double process (double x) noexcept
        {
            const double y = x - x1 + R * y1;
            x1 = x; y1 = y;
            return y;
        }
    };

    struct Chan
    {
        Biquad inHP, bright, interHP, interLP, bass, mid, treb, deep, pres;
        DC dc1, dc2;
        double sag = 0.0;
    };

    std::array<Chan, 2> ch;
    const Voicing* v = &getVoicing (0);
    double fs = 48000.0, smoothCoef = 0.001, sagAtk = 0.01, sagRel = 0.0005;
    double preGain = 1.0, inLin = 1.0, pd = 1.0, outLin = 1.0;
    double preTarget = 1.0, inTarget = 1.0, pdTarget = 1.0, outTarget = 1.0;
    bool bright = false, deep = false;
};

//==============================================================================
// Speaker cabinet approximation built from filters (no IR files needed).
class CabSim
{
public:
    void prepare (double sampleRate) { fs = sampleRate; setCab (cab); reset(); }

    void reset()
    {
        for (auto& c : ch) for (auto& b : c) b.reset();
    }

    void setCab (int index)
    {
        cab = std::clamp (index, 0, kNumCabs - 1);
        count = 0;
        auto add = [this] (Type t, double f, double q, double g = 0.0)
        {
            for (auto& c : ch)
            {
                auto& b = c[(size_t) count];
                switch (t)
                {
                    case Type::HP:   b.highPass (fs, f, q); break;
                    case Type::LP:   b.lowPass (fs, f, q); break;
                    case Type::Peak: b.peak (fs, f, q, g); break;
                }
            }
            ++count;
        };

        switch (cab)
        {
            case 0: // Off: just tame the very top and the sub lows
                add (Type::HP, 60, 0.707); add (Type::LP, 10000, 0.707); add (Type::LP, 12000, 0.707);
                break;
            case 1: // 1x12 open back
                add (Type::HP, 85, 0.707);
                add (Type::Peak, 115, 1.3, 2.5); add (Type::Peak, 450, 1.0, -2.0);
                add (Type::Peak, 2400, 1.2, 3.5);
                add (Type::LP, 5500, 0.5412); add (Type::LP, 5500, 1.3066);
                break;
            case 2: // 2x12 open back, brighter
                add (Type::HP, 80, 0.707);
                add (Type::Peak, 105, 1.2, 2.5); add (Type::Peak, 700, 0.8, -1.5);
                add (Type::Peak, 3200, 1.0, 4.0);
                add (Type::LP, 6000, 0.5412); add (Type::LP, 6000, 1.3066);
                break;
            case 3: // 4x12 closed back
                add (Type::HP, 75, 0.707);
                add (Type::Peak, 95, 1.5, 4.0); add (Type::Peak, 400, 0.9, -2.5);
                add (Type::Peak, 1800, 1.1, 3.0); add (Type::Peak, 3500, 1.5, 1.5);
                add (Type::LP, 4800, 0.5412); add (Type::LP, 4800, 1.3066);
                break;
            case 4: // 4x12 vintage
            default:
                add (Type::HP, 80, 0.707);
                add (Type::Peak, 110, 1.4, 3.0); add (Type::Peak, 600, 0.8, -2.0);
                add (Type::Peak, 2200, 1.0, 4.5); add (Type::Peak, 4200, 2.0, -2.0);
                add (Type::LP, 5200, 0.5412); add (Type::LP, 5200, 1.3066);
                break;
        }
    }

    int getCab() const { return cab; }

    void process (float* L, float* R, int n)
    {
        float* data[2] = { L, R };
        for (int k = 0; k < 2; ++k)
        {
            auto& c = ch[(size_t) k];
            for (int i = 0; i < n; ++i)
            {
                double x = (double) data[k][i];
                for (int s = 0; s < count; ++s) x = c[(size_t) s].process (x);
                data[k][i] = (float) x;
            }
        }
    }

private:
    enum class Type { HP, LP, Peak };
    std::array<std::array<Biquad, 8>, 2> ch;
    int count = 0, cab = 3;
    double fs = 48000.0;
};
} // namespace roadhouse
