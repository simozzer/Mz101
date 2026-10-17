// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Presets.h"

#include "Params.h"

namespace presets
{

const std::vector<Preset>& all()
{
    static const std::vector<Preset> list = {
        { "Init", {} },

        { "Rubber Bass", {
            { "range", 0 }, { "saw", 0 }, { "pulse", 10 }, { "sub", 10 },
            { "cutoff", 3.5f }, { "res", 3 }, { "vcfEnv", 5 }, { "vcfKybd", 5 },
            { "decay", 3.5f }, { "sustain", 0 }, { "release", 1.5f } } },

        { "Acid Line", {
            { "saw", 10 }, { "cutoff", 2.5f }, { "res", 8 }, { "vcfEnv", 6 }, { "vcfKybd", 3 },
            { "decay", 3 }, { "sustain", 0 }, { "release", 1 },
            { "portaMode", 1 }, { "portaTime", 2.5f } } },

        { "Sub Thump", {
            { "saw", 3 }, { "sub", 10 }, { "subType", 2 }, { "cutoff", 2 }, { "vcfEnv", 3 },
            { "decay", 5 }, { "sustain", 6 }, { "release", 2 } } },

        { "PWM Lead", {
            { "range", 2 }, { "saw", 0 }, { "pulse", 10 }, { "pw", 6 }, { "pwmSource", 0 }, { "lfoRate", 4 },
            { "cutoff", 5.5f }, { "res", 4 }, { "vcfEnv", 3 }, { "sustain", 7 }, { "release", 4 },
            { "portaMode", 1 }, { "portaTime", 3 } } },

        { "Arp Bleeps", {
            { "arpMode", 1 }, { "arpHold", 1 }, { "lfoRate", 6.5f },
            { "range", 2 }, { "saw", 0 }, { "pulse", 10 }, { "pw", 3 },
            { "cutoff", 4 }, { "res", 6 }, { "vcfEnv", 6 }, { "decay", 2.5f }, { "sustain", 0 } } },

        { "Noise Snare", {
            { "saw", 0 }, { "noise", 10 }, { "cutoff", 7 }, { "res", 2 }, { "vcfEnv", 2 }, { "vcfKybd", 0 },
            { "decay", 2.5f }, { "sustain", 0 }, { "release", 2.5f } } },

        { "Siren", {
            { "vcoMod", 6 }, { "lfoRate", 3 }, { "saw", 10 }, { "cutoff", 7 }, { "vcfEnv", 0 },
            { "sustain", 10 }, { "vcaMode", 1 } } },

        { "Random Filter", {
            { "lfoWave", 2 }, { "lfoRate", 6 }, { "saw", 0 }, { "pulse", 10 },
            { "cutoff", 4 }, { "res", 7 }, { "vcfMod", 5 }, { "vcfEnv", 0 }, { "vcaMode", 1 } } },

        { "Resonance Whistle", {
            { "saw", 0 }, { "res", 10 }, { "cutoff", 5 }, { "vcfKybd", 10 }, { "vcfEnv", 0 },
            { "vcaMode", 1 }, { "portaMode", 2 }, { "portaTime", 4 } } },
    };

    return list;
}

void apply(const Preset& p, juce::AudioProcessorValueTreeState& state)
{
    for (const auto& spec : params::specs())
        if (auto* param = state.getParameter(spec.id))
            param->setValueNotifyingHost(param->getDefaultValue());

    for (const auto& [id, value] : p.values)
        if (auto* param = state.getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1((float) value));
}

} // namespace presets
