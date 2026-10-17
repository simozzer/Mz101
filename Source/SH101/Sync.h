// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <algorithm>
#include <cmath>

namespace sh101
{

//
// The note lengths the modulator can lock to with SYNC on HOST, slowest first, so the RATE
// slider still runs slow at the bottom and fast at the top. Lengths are in quarter notes.
//
struct SyncDivision
{
    const char* name;
    double quarters;
};

inline constexpr SyncDivision syncDivisions[] = {
    { "4 bars", 16.0 },  { "2 bars", 8.0 },     { "1 bar", 4.0 },     { "1/2.", 3.0 },
    { "1/2", 2.0 },      { "1/4.", 1.5 },       { "1/2T", 4.0 / 3 },  { "1/4", 1.0 },
    { "1/8.", 0.75 },    { "1/4T", 2.0 / 3 },   { "1/8", 0.5 },       { "1/16.", 0.375 },
    { "1/8T", 1.0 / 3 }, { "1/16", 0.25 },      { "1/16T", 1.0 / 6 }, { "1/32", 0.125 },
};

inline constexpr int numSyncDivisions = (int) (sizeof syncDivisions / sizeof syncDivisions[0]);

// The RATE slider, 0..10, as an index into syncDivisions - equal slices of its travel.
inline int syncDivisionIndex(float rate)
{
    const int i = (int) std::lround(rate / 10.0f * (float) (numSyncDivisions - 1));
    return std::min(std::max(i, 0), numSyncDivisions - 1);
}

} // namespace sh101
