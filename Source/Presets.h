#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <utility>
#include <vector>

namespace presets
{

// A factory patch: only the controls that differ from the panel's defaults. Choices are the
// switch position as an index, buttons 0 or 1.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, double>> values;
};

const std::vector<Preset>& all();

// Every parameter to its default, then the preset's own values on top.
void apply(const Preset& p, juce::AudioProcessorValueTreeState& state);

} // namespace presets
