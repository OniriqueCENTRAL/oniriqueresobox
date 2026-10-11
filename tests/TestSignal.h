// Signals for the offline tests and the snapshot tool: no dependency.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace rbtest
{

/** Deterministic white noise in -1..1. */
struct Noise
{
    explicit Noise (uint32_t seed = 0x1234567u) : state (seed) {}

    float next() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (int32_t) state / 2147483648.0f;
    }

    uint32_t state;
};

inline std::vector<float> makeNoise (double seconds, double sampleRate, float amplitude, uint32_t seed = 1)
{
    Noise noise (seed);
    std::vector<float> out ((size_t) (seconds * sampleRate));

    for (auto& v : out)
        v = amplitude * noise.next();

    return out;
}

struct Resonance { double hz, q; float gain; };

/** Noise with a gentle roll-off, the way a voice or a cymbal falls away, and a
    few ringing peaks on top of it: what a resonance suppressor is for. */
inline std::vector<float> makeResonantNoise (double seconds, double sampleRate,
                                             const std::vector<Resonance>& resonances, uint32_t seed = 7)
{
    Noise noise (seed);
    std::vector<float> out ((size_t) (seconds * sampleRate));

    struct Filter { double b0, a1, a2, z1 = 0.0, z2 = 0.0; float gain; };
    std::vector<Filter> filters;

    for (const auto& r : resonances)
    {
        const double w = 2.0 * M_PI * r.hz / sampleRate;
        const double alpha = std::sin (w) / (2.0 * r.q);
        const double a0 = 1.0 + alpha;

        // Band-pass with unit gain at its centre.
        filters.push_back ({ alpha / a0, -2.0 * std::cos (w) / a0, (1.0 - alpha) / a0, 0.0, 0.0, r.gain });
    }

    double low = 0.0;

    for (auto& v : out)
    {
        const double white = noise.next();
        low += 0.12 * (white - low);

        double sum = 0.25 * white + 0.9 * low;

        for (auto& f : filters)
        {
            // Transposed direct form II, numerator b0 * (1 - z^-2).
            const double y = f.b0 * white + f.z1;
            f.z1 = f.z2 - f.a1 * y;
            f.z2 = -f.b0 * white - f.a2 * y;
            sum += f.gain * y;
        }

        v = (float) (0.25 * sum);
    }

    return out;
}

/** The level of one frequency in a stretch of signal, in dB. */
inline double toneLevelDb (const float* x, size_t count, double hz, double sampleRate)
{
    double re = 0.0, im = 0.0;

    for (size_t i = 0; i < count; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) count);
        const double phase = 2.0 * M_PI * hz * (double) i / sampleRate;
        re += w * x[i] * std::cos (phase);
        im -= w * x[i] * std::sin (phase);
    }

    return 20.0 * std::log10 (4.0 * std::sqrt (re * re + im * im) / (double) count + 1.0e-12);
}

inline double rmsDb (const float* x, size_t count)
{
    double sum = 0.0;

    for (size_t i = 0; i < count; ++i)
        sum += (double) x[i] * x[i];

    return 10.0 * std::log10 (sum / (double) std::max<size_t> (1, count) + 1.0e-20);
}

} // namespace rbtest
