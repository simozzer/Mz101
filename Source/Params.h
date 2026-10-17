// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SH101/Settings.h"

#include <vector>

namespace params
{

//
// One row per front-panel control: its parameter ID, the name a host shows, and the field
// of sh101::Settings it drives. The layout, the reader and the presets all work from this,
// so a control cannot exist in one and be missing from another.
//
// IDs are saved in every host project. Never rename one.
//
struct Spec
{
    const char* id;
    const char* name;
    int cc;                                  // the MIDI CC that moves it
    const char* hint;                        // what it does, for the hover hint
    float sh101::Settings::* f = nullptr;    // a slider, 0..10 unless min/max say otherwise
    int   sh101::Settings::* i = nullptr;    // a switch: choices, bottom position first
    bool  sh101::Settings::* b = nullptr;    // a button
    juce::StringArray choices = {};
    float min = 0.0f, max = 10.0f;
};

const std::vector<Spec>& specs();
int indexOf(const juce::String& id);   // -1 if none

// The hover hint: name, what it does, and the CC it answers to now - which MIDI learn may
// have changed from the default in the table (cc < 0 for none).
juce::String hintFor(int index, int cc, bool learning);

// CCs that can be learned: not bank select (0, 32), not the bender's MOD (1), not the
// channel mode messages (120 up).
inline bool learnable(int cc) { return cc >= 2 && cc < 120 && cc != 32; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Reads every parameter into Settings, from the audio thread, without a lookup by name.
class Reader
{
public:
    explicit Reader(juce::AudioProcessorValueTreeState& state);
    sh101::Settings read() const;

private:
    std::vector<std::atomic<float>*> values;
};

} // namespace params
