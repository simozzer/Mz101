#pragma once

#include <cmath>
#include <cstdint>

namespace sh101
{

constexpr double pi = 3.14159265358979323846;

inline float clamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// A linear pot feeding an exponential converter, which is what almost every time and rate
// control on the SH-101 is: equal slider travel, equal ratio.
inline float expLaw(float x, float lo, float hi) { return lo * std::pow(hi / lo, x); }

// One-pole RC coefficient for a time constant in seconds.
inline float onePoleCoef(float tauSeconds, float sampleRate)
{
    return tauSeconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (tauSeconds * sampleRate));
}

// tanh, close enough for a transistor pair and a good deal cheaper. Exact at 0, within a
// percent to |x| = 3, and saturating to +-1 beyond.
inline float softClip(float x)
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// softClip(x) / x - the small-signal gain of a saturating stage at the point it is working,
// which is what the filter linearises around.
inline float softClipGain(float x)
{
    if (std::fabs(x) < 1.0e-4f) return 1.0f;
    return softClip(x) / x;
}

class Random
{
public:
    explicit Random(uint32_t seed = 0x2545F491u) : state(seed ? seed : 1u) {}

    uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uniform() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0, 1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }                          // [-1, 1)

    // Four uniforms summed: near enough gaussian for noise that is filtered before anyone
    // hears it, and scaled to unit deviation.
    float noise() { return (uniform() + uniform() + uniform() + uniform() - 2.0f) * 1.7320508f; }

private:
    uint32_t state;
};

} // namespace sh101
