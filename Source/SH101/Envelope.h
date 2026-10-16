// Mz101 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"

namespace sh101
{

//
// THE ENVELOPE
//
// An analog ADSR: a capacitor charged and discharged through resistors, so every segment
// is an exponential approach rather than a line.
//
// The attack charges toward a voltage ABOVE the peak and is cut off by a comparator when it
// gets there, so it arrives still moving fast - the attack is closer to a straight line than
// an RC curve, and the turn into the decay is sharp. That corner is a good part of why the
// 101's envelopes sound snappy. Decay and release are plain RC towards the sustain level
// and towards zero.
//
// A retrigger starts the attack from wherever the capacitor is, not from zero. There is
// nothing in the circuit to discharge it first.
//
class Envelope
{
public:
    void setSampleRate(float r) { rate = r; }

    // Times as printed in the spec: attack to the peak, decay and release to within a few
    // percent of where they are heading.
    void set(float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds)
    {
        aCoef = onePoleCoef(attackSeconds / attackLog, rate);
        dCoef = onePoleCoef(decaySeconds / 3.0f, rate);
        rCoef = onePoleCoef(releaseSeconds / 3.0f, rate);
        sustain = sustainLevel;
    }

    void trigger() { stage = Attack; }
    void release() { if (stage != Idle) stage = Release; }
    void reset()   { stage = Idle; level = 0; }

    float value() const { return level; }

    float process()
    {
        switch (stage)
        {
            case Attack:
                level += (overshoot - level) * aCoef;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;

            case Decay:
                level += (sustain - level) * dCoef;
                break;

            case Release:
                level -= level * rCoef;
                if (level < 1.0e-6f) { level = 0; stage = Idle; }
                break;

            default:
                break;
        }

        return level;
    }

private:
    enum Stage { Idle, Attack, Decay, Release };

    // The attack's target, as a multiple of the peak; attackLog is how many time constants
    // it takes to get from 0 to 1 heading there.
    static constexpr float overshoot = 1.3f;
    static constexpr float attackLog = 1.4663371f;   // ln(1.3 / 0.3)

    float rate = 44100;
    Stage stage = Idle;
    float level = 0;
    float sustain = 0.5f;
    float aCoef = 1, dCoef = 1, rCoef = 1;
};

} // namespace sh101
