// Mz101 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"
#include "Settings.h"

namespace sh101
{

//
// THE MODULATOR
//
// Triangle (the 101 has no sine) and square from one oscillator; NOISE is the noise
// generator itself, lowpassed only by the modulation path.
//
// RANDOM is NOT a sample-and-hold of the noise. The service manual's CPU program has the
// CPU generate random data each time it sees the clock go high (step 6a) and write it out
// through the D/A converter (step 7) - so the engine supplies it, via setRandom().
//
// It is also the instrument's clock: the CPU watches its square (clockHigh) to step the
// arpeggiator and sequencer, and the envelope's LFO trigger takes it directly, so this
// also reports where each cycle starts.
//
class Lfo
{
public:
    struct Tick
    {
        float value = 0;
        bool  cycleStart = false;
        bool  halfCycle = false;
    };

    void setSampleRate(float r)
    {
        noiseCoef = onePoleCoef(1.0f / (2.0f * (float) pi * 400.0f), r);
    }

    void reset() { phase = 0; held = 0; noiseLp = 0; pendingStart = false; }

    // The CPU's Clock Reset: start a cycle on the very next sample.
    void restart() { phase = 0; pendingStart = true; }

    // The square, as the CPU's clock input sees it.
    bool clockHigh() const { return phase < 0.5f; }

    // The CPU's random data, for the RANDOM waveform.
    void setRandom(float v) { held = v; }

    // Free running: advance by an increment, as the hardware oscillator does.
    Tick process(float increment, int wave, Random& rng)
    {
        Tick t;

        if (pendingStart)
        {
            pendingStart = false;
            t.cycleStart = true;
        }
        else
        {
            const float was = phase;
            phase += increment;

            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                t.cycleStart = true;
            }
            else if (was < 0.5f && phase >= 0.5f)
            {
                t.halfCycle = true;
            }
        }

        return output(t, wave, rng);
    }

    // Synced: put at a phase worked out from the host's timeline. Going backwards - the
    // cycle wrapping, or the host jumping back - is a new cycle.
    Tick processAt(float target, int wave, Random& rng)
    {
        Tick t;
        pendingStart = false;

        if (target < phase)                          t.cycleStart = true;
        else if (phase < 0.5f && target >= 0.5f)     t.halfCycle = true;

        phase = target;
        return output(t, wave, rng);
    }

private:
    Tick output(Tick t, int wave, Random& rng)
    {
        noiseLp += (rng.noise() - noiseLp) * noiseCoef;

        switch (wave)
        {
            case LfoWave::Square: t.value = phase < 0.5f ? 1.0f : -1.0f; break;
            case LfoWave::Random: t.value = held; break;
            case LfoWave::Noise:  t.value = clamp(noiseLp * 6.0f, -1.0f, 1.0f); break;
            default:              t.value = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase; break;
        }

        return t;
    }

    float phase = 0;
    float held = 0;
    float noiseLp = 0;
    float noiseCoef = 0.05f;
    bool  pendingStart = false;
};

} // namespace sh101
