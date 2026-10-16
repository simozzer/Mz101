#pragma once

#include "Dsp.h"
#include "Settings.h"

namespace sh101
{

//
// THE VCO
//
// One sawtooth core (a CEM3340-style exponential current into a capacitor, dumped by a
// comparator at the top of each cycle) from which everything else is derived in hardware:
//
//   saw     the core itself
//   pulse   a comparator between the saw and the PW voltage - so pulse width is a
//           threshold on the ramp, and moves continuously under PWM
//   sub     flip-flops clocked by the core's reset: one divides by two (square, one octave
//           down), a second divides that again (square, two down), and the two ANDed give
//           the 25% pulse two octaves down
//
// Because the sub is clocked by the core, it is phase-locked to it - no beating, ever, and
// its edges land exactly on the saw's reset. That is why the 101's sub sounds welded to the
// main oscillator rather than layered under it.
//
// Every edge is band-limited with a polyBLEP, placed at its exact fractional position. The
// correction straddles the edge - half before it, half after - so the output runs one sample
// late: `pending` holds the previous sample until the next one says whether an edge landed
// between them.
//
class Vco
{
public:
    struct Levels
    {
        float pulse = 0, saw = 0, sub = 0;
        int   subType = SubType::OctDown1;
    };

    void reset()
    {
        phase = 0;
        pulseHigh = true;
        q1 = q2 = false;
        pending = 0;
    }

    // dt is frequency / sample rate; width is the pulse's duty, 0.05 to 0.5.
    float process(float dt, float width, const Levels& L)
    {
        before = after = 0;
        edgeEnergy = 0;

        const float oldSub = subValue(L.subType);

        // If PWM moved the threshold across the ramp since the last sample, the comparator
        // has already flipped. Put the edge in the middle of the interval - it is a tiny
        // step in any case, since PWM is slow next to the ramp.
        if (pulseHigh != (phase < width))
        {
            pulseHigh = ! pulseHigh;
            edge(L.pulse * (pulseHigh ? 2.0f : -2.0f), 0.5f);
        }

        float p = phase + dt;

        // falling edge of the pulse before the reset
        if (pulseHigh && phase < width && p >= width)
        {
            pulseHigh = false;
            edge(-2.0f * L.pulse, (p - width) / dt);
        }

        if (p >= 1.0f)
        {
            p -= 1.0f;
            const float d = p / dt;   // samples since the reset

            edge(-2.0f * L.saw, d);

            if (! pulseHigh)
            {
                pulseHigh = true;
                edge(2.0f * L.pulse, d);
            }

            // the dividers
            const bool q1Fell = q1;
            q1 = ! q1;
            if (q1Fell) q2 = ! q2;

            edge(L.sub * (subValue(L.subType) - oldSub), d);

            // and the pulse may fall again in the same sample, at very high pitches
            if (p >= width)
            {
                pulseHigh = false;
                edge(-2.0f * L.pulse, (p - width) / dt);
            }
        }

        phase = p;

        const float naive = L.saw * (2.0f * phase - 1.0f)
                          + L.pulse * (pulseHigh ? 1.0f : -1.0f)
                          + L.sub * subValue(L.subType);

        const float out = pending + before;
        pending = naive + after;
        return out;
    }

    // How much the outputs jumped this sample, all edges together - the current the mixer
    // and the sub's flip-flops pull from the rail when they switch.
    float lastEdgeEnergy() const { return edgeEnergy; }

private:
    // A step of height h, d samples (0..1) before now: the polyBLEP residual, split between
    // the previous sample and this one.
    void edge(float h, float d)
    {
        edgeEnergy += std::fabs(h);
        before += 0.5f * h * d * d;
        after  -= 0.5f * h * (1.0f - d) * (1.0f - d);
    }

    float subValue(int type) const
    {
        switch (type)
        {
            case SubType::OctDown2: return q2 ? 1.0f : -1.0f;
            case SubType::Pulse2:   return (q1 && q2) ? 1.0f : -1.0f;
            default:                return q1 ? 1.0f : -1.0f;
        }
    }

    float phase = 0;
    bool  pulseHigh = true;
    bool  q1 = false, q2 = false;
    float pending = 0;
    float before = 0, after = 0;
    float edgeEnergy = 0;
};

} // namespace sh101
