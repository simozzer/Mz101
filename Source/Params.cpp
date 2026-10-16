#include "Params.h"

namespace params
{

using S = sh101::Settings;

const std::vector<Spec>& specs()
{
    //
    // CCs follow General MIDI 2's sound controllers where one fits - cutoff 74, resonance 71,
    // attack 73, decay 75, release 72, vibrato rate and depth 76 and 77, portamento 5 and 65,
    // volume 7 - and HOLD is the sustain pedal, 64, as the 101 has a HOLD pedal jack. The rest
    // use CCs MIDI leaves undefined: 14-31 and 102-119. CC 1 is the bender's MOD lever.
    //
    static const std::vector<Spec> table = {
        // MODULATOR
        { "lfoRate",   "Mod Rate",        76, "Speed of the modulator (LFO): 0.1 to 30 Hz, or with SYNC on HOST a note length from 4 bars to 1/32. With CLOCK on INT it is also the arpeggio and sequencer's tempo: one step per cycle.",
          &S::lfoRate },
        { "lfoSync",   "Mod Sync",       108, "FREE: the modulator runs at RATE in hertz, as on the original. HOST: it locks to your DAW's tempo and timeline, and RATE picks a note length. In the standalone app HOST falls back to FREE.",
          nullptr, &S::lfoSync, nullptr, { "Free", "Host" } },
        { "lfoWave",   "Mod Waveform",    14, "The modulator's shape. RANDOM is a new random level on each clock; NOISE is the noise generator.",
          nullptr, &S::lfoWave, nullptr, { "Triangle", "Square", "Random", "Noise" } },
        { "clockSource", "Clock",         106, "What steps the arpeggio and sequencer: the modulator (INT), or your DAW's tempo and transport (HOST) - the EXT CLK jack. In the standalone app HOST falls back to INT.",
          nullptr, &S::clockSource, nullptr, { "Int", "Host" } },
        { "clockDiv",  "Clock Division",  107, "With the clock on HOST: the length of each step, from quarter notes to 32nds (T = triplets).",
          nullptr, &S::clockDiv, nullptr, { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" } },

        // VCO
        { "vcoMod",    "VCO Mod",         77, "How much the modulator moves the oscillator's pitch: vibrato, trills, sirens.",
          &S::vcoMod },
        { "range",     "VCO Range",       15, "Oscillator octave, as organ footage: 16' is lowest, 2' highest.",
          nullptr, &S::range, nullptr, { "16'", "8'", "4'", "2'" } },
        { "pw",        "Pulse Width",     16, "Pulse width - or how far PWM moves it. 0 is a square (50%); 10 narrows it to nothing.",
          &S::pw },
        { "pwmSource", "PWM Source",      17, "What sets the pulse width: the modulator (LFO), the PW slider alone (MAN), or the envelope (ENV).",
          nullptr, &S::pwmSource, nullptr, { "LFO", "Man", "Env" } },

        // SOURCE MIXER
        { "pulse",     "Pulse Level",     18, "Level of the pulse wave into the filter.",
          &S::pulse },
        { "saw",       "Saw Level",       19, "Level of the sawtooth into the filter.",
          &S::saw },
        { "sub",       "Sub Level",       20, "Level of the sub oscillator, locked to the main one below it.",
          &S::sub },
        { "subType",   "Sub Waveform",    21, "Sub oscillator: square one octave down, square two down, or narrow pulse two down.",
          nullptr, &S::subType, nullptr, { "-1 Oct", "-2 Oct", "-2 Pulse" } },
        { "noise",     "Noise Level",     22, "Level of white noise into the filter.",
          &S::noise },

        // VCF
        { "cutoff",    "VCF Freq",        74, "Filter cutoff, 10 Hz to 20 kHz. Lower is darker.",
          &S::cutoff },
        { "res",       "VCF Res",         71, "Filter resonance. Emphasises the cutoff; near the top the filter whistles on its own.",
          &S::res },
        { "vcfEnv",    "VCF Env",         23, "How far the envelope opens the filter on each note.",
          &S::vcfEnv },
        { "vcfMod",    "VCF Mod",         24, "How much the modulator moves the filter cutoff.",
          &S::vcfMod },
        { "vcfKybd",   "VCF Kybd",        25, "How much the cutoff follows the keyboard. 10 tracks it exactly - play a whistling filter in tune.",
          &S::vcfKybd },

        // VCA
        { "vcaMode",   "VCA Mode",        26, "What shapes the volume: the envelope (ENV), or simply on while a key is held (GATE).",
          nullptr, &S::vcaMode, nullptr, { "Env", "Gate" } },

        // ENV
        { "attack",    "Env Attack",      73, "Envelope attack time, 1.5 ms to 4 s.",
          &S::attack },
        { "decay",     "Env Decay",       75, "Envelope decay time down to the sustain level, 2 ms to 10 s.",
          &S::decay },
        { "sustain",   "Env Sustain",     79, "Level the envelope holds at while a key stays down.",
          &S::sustain },
        { "release",   "Env Release",     72, "Envelope release time after the key is let go, 2 ms to 10 s.",
          &S::release },
        { "envTrig",   "Env Trigger",     27, "When the envelope restarts: every new key (G+T), only after all keys are up (GATE), or on every modulator cycle (LFO).",
          nullptr, &S::envTrig, nullptr, { "Gate+Trig", "Gate", "LFO" } },

        // PORTAMENTO
        { "portaMode", "Portamento",      65, "Glide between notes: never, only when keys overlap (AUTO), or always.",
          nullptr, &S::portaMode, nullptr, { "Off", "Auto", "On" } },
        { "portaTime", "Portamento Time",  5, "Glide time, up to 5 s.",
          &S::portaTime },

        // BENDER
        { "bendVco",   "Bender VCO",      28, "How far the pitch wheel bends the oscillator, up to an octave.",
          &S::bendVco },
        { "bendVcf",   "Bender VCF",      29, "How far the pitch wheel moves the filter cutoff.",
          &S::bendVcf },
        { "modDepth",  "Bender Mod",      30, "How much vibrato the mod wheel (CC 1) brings in.",
          &S::modDepth },

        // ARPEGGIO / SEQUENCER / TRANSPOSE
        { "arpMode",   "Arpeggio",       102, "Arpeggiator: plays the held keys in turn, at the modulator's rate.",
          nullptr, &S::arpMode, nullptr, { "Off", "Up", "Up&Down", "Down" } },
        { "seqMode",   "Sequencer",      103, "PLAY runs the sequence while a key is held (the key transposes it). LOAD records a new one.",
          nullptr, &S::seqMode, nullptr, { "Off", "Play", "Load" } },
        { "arpHold",   "Hold",            64, "Keeps the arpeggio or sequence running after the keys are let go. Also the sustain pedal.",
          nullptr, nullptr, &S::arpHold },
        { "transpose", "Transpose",      104, "Moves the keyboard down an octave (L), or up one (H).",
          nullptr, &S::transpose, nullptr, { "Low", "Mid", "High" } },

        // MASTER
        { "tune",      "Tune",           105, "Fine tuning, +-50 cents.",
          &S::tune, nullptr, nullptr, {}, -50.0f, 50.0f },
        { "volume",    "Volume",           7, "Output level.",
          &S::volume },
    };

    return table;
}

int indexOf(const juce::String& id)
{
    const auto& t = specs();
    for (size_t i = 0; i < t.size(); ++i)
        if (id == t[i].id) return (int) i;
    return -1;
}

juce::String hintFor(int index, int cc, bool learning)
{
    if (index < 0 || index >= (int) specs().size()) return {};
    const auto& p = specs()[(size_t) index];

    juce::String midi;
    if (learning)          midi = "MIDI LEARN: move a knob on your controller. Right-click to cancel.";
    else if (cc < 0)       midi = "No MIDI CC (default " + juce::String(p.cc) + "). Right-click to learn one.";
    else if (cc != p.cc)   midi = "MIDI CC " + juce::String(cc) + ", learned (default " + juce::String(p.cc) + "). Right-click to change.";
    else                   midi = "MIDI CC " + juce::String(cc) + ". Right-click to learn another.";

    return juce::String(p.name).toUpperCase() + "\n" + p.hint + "\n" + midi;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const S defaults;

    for (const auto& p : specs())
    {
        const juce::ParameterID id { p.id, 1 };

        if (p.f != nullptr)
        {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, p.name, juce::NormalisableRange<float> (p.min, p.max, 0.0f), defaults.*(p.f),
                juce::AudioParameterFloatAttributes().withStringFromValueFunction(
                    [] (float v, int) { return juce::String (v, 1); })));
        }
        else if (p.i != nullptr)
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, p.name, p.choices, defaults.*(p.i)));
        }
        else
        {
            layout.add(std::make_unique<juce::AudioParameterBool>(id, p.name, defaults.*(p.b)));
        }
    }

    return layout;
}

Reader::Reader(juce::AudioProcessorValueTreeState& state)
{
    for (const auto& p : specs())
    {
        auto* v = state.getRawParameterValue(p.id);
        jassert(v != nullptr);
        values.push_back(v);
    }
}

sh101::Settings Reader::read() const
{
    S s;
    const auto& table = specs();

    for (size_t n = 0; n < table.size(); ++n)
    {
        const float v = values[n]->load(std::memory_order_relaxed);
        const auto& p = table[n];

        if (p.f != nullptr)      s.*(p.f) = v;
        else if (p.i != nullptr) s.*(p.i) = juce::roundToInt(v);
        else                     s.*(p.b) = v >= 0.5f;
    }

    return s;
}

} // namespace params
