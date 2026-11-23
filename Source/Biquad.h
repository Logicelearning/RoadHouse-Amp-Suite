#pragma once
#include <cmath>
#include <algorithm>

namespace roadhouse
{
// Small RBJ-cookbook biquad (transposed direct form II, double precision).
// No allocation anywhere, so coefficients can be re-designed on the audio thread.
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    void setBypass() noexcept { b0 = 1.0; b1 = b2 = a1 = a2 = 0.0; }

    void lowPass (double fs, double f, double q) noexcept
    {
        double c, s, alpha; prep (fs, f, q, c, s, alpha);
        setRaw ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }

    void highPass (double fs, double f, double q) noexcept
    {
        double c, s, alpha; prep (fs, f, q, c, s, alpha);
        setRaw ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }

    void peak (double fs, double f, double q, double gainDb) noexcept
    {
        double c, s, alpha; prep (fs, f, q, c, s, alpha);
        const double A = std::pow (10.0, gainDb / 40.0);
        setRaw (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void lowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        double c, s, alpha; prep (fs, f, q, c, s, alpha);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double t = 2.0 * std::sqrt (A) * alpha;
        setRaw (A * ((A + 1) - (A - 1) * c + t),
                2 * A * ((A - 1) - (A + 1) * c),
                A * ((A + 1) - (A - 1) * c - t),
                (A + 1) + (A - 1) * c + t,
                -2 * ((A - 1) + (A + 1) * c),
                (A + 1) + (A - 1) * c - t);
    }

    void highShelf (double fs, double f, double q, double gainDb) noexcept
    {
        double c, s, alpha; prep (fs, f, q, c, s, alpha);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double t = 2.0 * std::sqrt (A) * alpha;
        setRaw (A * ((A + 1) + (A - 1) * c + t),
                -2 * A * ((A - 1) + (A + 1) * c),
                A * ((A + 1) + (A - 1) * c - t),
                (A + 1) - (A - 1) * c + t,
                2 * ((A - 1) - (A + 1) * c),
                (A + 1) - (A - 1) * c - t);
    }

private:
    static void prep (double fs, double f, double q, double& c, double& s, double& alpha) noexcept
    {
        f = std::min (std::max (f, 5.0), fs * 0.45);
        const double w0 = 2.0 * 3.14159265358979323846 * f / fs;
        c = std::cos (w0);
        s = std::sin (w0);
        alpha = s / (2.0 * q);
    }

    void setRaw (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / a0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }
};
} // namespace roadhouse
