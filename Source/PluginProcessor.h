// Mz101 - Created by Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>

#include "Params.h"
#include "SH101/Engine.h"

class Mz101Processor final : public juce::AudioProcessor
{
public:
    Mz101Processor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // The on-screen keyboard plays through this, alongside whatever MIDI the host sends.
    juce::MidiKeyboardState keyboardState;

    // The KEY TRANSPOSE (REST in LOAD) and LEGATO buttons, from the message thread. Counted
    // rather than flagged so two quick presses between blocks are two presses.
    void pressKeyTranspose() { transposePresses.fetch_add(1); }
    void pressLegato()       { legatoPresses.fetch_add(1); }

    // For the editor's step readout; only the engine's atomic accessors are safe here.
    const sh101::Engine& engineView() const { return engine; }

    // MIDI CC assignments, by control (its index in params::specs()). Each control answers
    // to one CC or none, and each CC drives one control at most. Safe from any thread.
    int  ccFor(int index) const          { return ccOf[(size_t) index].load(); }
    void learn(int index)                { learning = index; }   // -1 cancels
    int  learningIndex() const           { return learning.load(); }
    void forgetCc(int index)             { assignCc(index, -1); }
    void restoreDefaultCc(int index)     { assignCc(index, params::specs()[(size_t) index].cc); }
    void resetAllCcs();

    // bumped on every change, so the editor can tell when to repaint
    int  midiMapVersion() const          { return mapVersion.load(); }

private:
    void handle(const juce::MidiMessage& m);
    void assignCc(int index, int cc);

    params::Reader reader;
    sh101::Engine engine;

    static constexpr size_t maxControls = 64;
    std::vector<juce::RangedAudioParameter*> controls;   // by index, as params::specs()
    std::array<std::atomic<int>, maxControls> ccOf {};
    std::atomic<int> learning { -1 }, mapVersion { 0 };
    int currentProgram = 0;
    std::atomic<int> transposePresses { 0 }, legatoPresses { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mz101Processor)
};
