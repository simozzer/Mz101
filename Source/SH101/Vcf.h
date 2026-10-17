// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"

namespace sh101
{

//
// THE VCF: an IR3109 with the 101's resonance circuit round it
//
// The IR3109 is four OTAs, each wired as a buffered integrator with its output fed back to
// its own inverting input - four identical one-pole lowpasses, sharing one exponential
// current source for cutoff. An OTA's output current is tanh of its differential input, so
// each stage is
//
//      dy/dt = wc * tanh(in - y)
//
// which is NOT the Moog ladder's tanh(in) - tanh(y): the OTA saturates on the DIFFERENCE,
// so it softens most when the input moves fast against a slow output - bright edges into a
// low cutoff - and leaves gentle material almost clean.
//
// The chip has no resonance of its own. On the 101 a transistor phase splitter takes the
// fourth stage's output: the inverted side goes back to the input through the resonance
// pot, and the non-inverted side is added to the OUTPUT. So turning up resonance also
// raises the level going on to the VCA, which is why a 101 keeps more of its bottom end at
// high resonance than a plain four-pole would. A pair of diodes to ground limits the
// feedback, which is what sets the amplitude it self-oscillates at.
//
// Solved as four trapezoidal (TPT) one-poles with the feedback loop solved instantaneously -
// no unit delay in the loop, so cutoff and resonance stay where they are put right up to the
// top of the range. The nonlinearities are linearised around the previous sample's operating
// point, which at 4x oversampling is indistinguishable from iterating to convergence.
//
class Vcf
{
public:
    void reset()
    {
        for (auto& v : s) v = 0;
        for (auto& v : diff) v = 0;
        y4 = 0;
    }

    // g = tan(pi * fc / fs); k = feedback, self-oscillation from about 4.
    float process(float x, float g, float k)
    {
        float G[4], S[4];

        for (int i = 0; i < 4; ++i)
        {
            const float gi = g * softClipGain(diff[i]);
            G[i] = gi / (1.0f + gi);
            S[i] = s[i] / (1.0f + gi);
        }

        // the whole cascade as y4 = Gt * u + St
        const float Gt = G[0] * G[1] * G[2] * G[3];
        const float St = ((S[0] * G[1] + S[1]) * G[2] + S[2]) * G[3] + S[3];

        // solve the loop u = x - k * diode(y4) for this sample
        const float dg = diodeGain(y4);
        const float est = (Gt * x + St) / (1.0f + k * dg * Gt);
        float u = x - k * diode(est);

        for (int i = 0; i < 4; ++i)
        {
            const float y = G[i] * u + S[i];
            diff[i] = u - y;
            s[i] = 2.0f * y - s[i];
            u = y;
        }

        y4 = u;

        // the phase splitter's forward half
        return y4 + forward * k * diode(y4);
    }

private:
    // The clipping diodes: soft, knee at about the level the feedback runs at near
    // self-oscillation.
    static float diode(float y)     { return softClip(y * 1.6f) * (1.0f / 1.6f); }
    static float diodeGain(float y) { return softClipGain(y * 1.6f); }

    static constexpr float forward = 0.5f;

    float s[4] {};
    float diff[4] {};
    float y4 = 0;
};

} // namespace sh101
