// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include "Dsp.h"

#include <algorithm>
#include <vector>

namespace sh101
{

//
// Back down from the oversampled rate to the host's.
//
// The voice runs at 2x or 4x so the filter's and VCA's saturation, and the oscillator's
// audio-rate FM from the noise LFO, have room above the audio band to put their harmonics.
// This removes them: a Kaiser-windowed sinc, computed only at the output samples.
//
// The stopband begins not at the output's Nyquist but where anything left would alias down
// to above 20 kHz - a little aliasing into the last two kilohertz before Nyquist is
// inaudible, and it buys a flat response to well above 18 kHz from a filter of modest length.
//
class Decimator
{
public:
    void prepare(int oversampling, float hostRate)
    {
        factor = oversampling;
        taps = factor == 1 ? 1 : 64 * factor;

        h.assign((size_t) taps, 0.0f);
        buf.assign((size_t) taps * 2, 0.0f);
        pos = 0;

        if (factor == 1)
        {
            h[0] = 1.0f;
            return;
        }

        const double stopEdge = std::max(0.5, 1.0 - 20000.0 / hostRate) / factor;
        const double beta = 9.0;                                       // about 90 dB
        const double transition = (90.0 - 7.95) / (14.36 * taps);
        const double fc = stopEdge - transition * 0.5;
        const double mid = (taps - 1) * 0.5;

        double sum = 0;
        for (int n = 0; n < taps; ++n)
        {
            const double t = n - mid;
            const double sinc = t == 0 ? 2 * fc : std::sin(2 * pi * fc * t) / (pi * t);
            const double r = 2.0 * n / (taps - 1) - 1.0;
            const double w = besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
            h[(size_t) n] = (float) (sinc * w);
            sum += sinc * w;
        }

        for (auto& c : h) c = (float) (c / sum);
    }

    void push(float x)
    {
        buf[(size_t) pos] = x;
        buf[(size_t) (pos + taps)] = x;
        if (++pos == taps) pos = 0;
    }

    float output() const
    {
        const float* p = buf.data() + pos;
        float acc = 0;
        for (int i = 0; i < taps; ++i)
            acc += h[(size_t) i] * p[i];
        return acc;
    }

private:
    static double besselI0(double x)
    {
        double sum = 1, term = 1;
        for (int k = 1; k < 64; ++k)
        {
            const double q = x / (2.0 * k);
            term *= q * q;
            sum += term;
            if (term < 1e-14 * sum) break;
        }
        return sum;
    }

    int factor = 1, taps = 1, pos = 0;
    std::vector<float> h, buf;
};

} // namespace sh101
