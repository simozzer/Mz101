#pragma once

#include "Decimator.h"
#include "Dsp.h"
#include "Envelope.h"
#include "Lfo.h"
#include "Settings.h"
#include "Sync.h"
#include "Vcf.h"
#include "Vco.h"

#include <atomic>
#include <cstdint>

namespace sh101
{

//
// One SH-101: the CPU's key assigner, arpeggiator and sequencer, then modulator, VCO,
// mixer, VCF, VCA and envelope, oversampled.
//
// THE CPU
//
// Keys, buttons and the clock do not reach the analog circuits directly. An 80C49 runs a
// loop (service manual, "CPU PROGRAM", steps 3-13) every 1.5 to 3.5 ms: read the keyboard,
// check the clock, read the buttons, then write the KCV, gate and LED outputs. So a key
// pressed lands up to a few milliseconds late, and an arpeggio or sequence step lands on the
// first pass after the clock edge rather than on the edge. That small, uneven latency is part
// of how the 101 feels, so it is modelled: notes and buttons are queued, and acted on only
// when the simulated CPU next comes round.
//
// Monophonic with last-note priority. No velocity - the 101 has none.
//
// Everything is called from the audio thread, between render() calls, except the
// read-side accessors and loadSequence(), which are safe from any thread.
//
class Engine
{
public:
    static constexpr int maxSteps = 100;

    // A sequencer step, packed: note in the low seven bits, then REST, then TIE - this step
    // is entered legato from the one before, with no new envelope.
    static constexpr uint32_t stepRest = 1u << 7;
    static constexpr uint32_t stepTie  = 1u << 8;

    void prepare(double hostSampleRate);
    void setSettings(const Settings& s);

    // the keyboard and the function buttons - queued for the CPU
    void noteOn(int note)   { push(Event::On, note); }
    void noteOff(int note)  { push(Event::Off, note); }
    void keyTranspose()     { push(Event::KeyTranspose, 0); }   // REST while in LOAD
    void legato()           { push(Event::Legato, 0); }
    void allNotesOff();

    // The EXT CLK input, fed from the host: where it is on the timeline (in quarter notes)
    // at the start of the next render, its tempo, and whether the transport is running.
    // available = false when there is no host timeline at all (the standalone app); the
    // CLOCK switch's HOST position then falls back to the modulator.
    void setHostClock(bool available, bool playing, double ppq, double bpm);

    void setPitchBend(float v) { bend = clamp(v, -1.0f, 1.0f); }   // the lever, side to side
    void setModLever(float v)  { modLever = clamp(v, 0.0f, 1.0f); } // the lever, pushed

    // any thread
    int      seqLength() const          { return length.load(); }
    int      seqPosition() const        { return position.load(); }
    bool     seqLegatoArmed() const     { return legatoArmed.load(); }
    bool     keyTransposeArmed() const  { return transposeArmed.load(); }
    int      keyShift() const           { return shift.load(); }
    uint32_t seqStep(int i) const       { return steps[i].load(); }
    void     loadSequence(const uint32_t* data, int count);

    void render(float* out, int numSamples);

    int  currentKey() const    { return key; }
    bool gateIsOn() const      { return gate; }
    int  oversampling() const  { return factor; }

private:
    enum class Mode { Keyboard, Arpeggio, Sequence };

    struct Event
    {
        enum Type : uint8_t { On, Off, KeyTranspose, Legato } type;
        uint8_t note;
    };

    struct Targets
    {
        float cutoffLog = 0, k = 0, pw = 0;
        float pulse = 0, saw = 0, sub = 0, noise = 0;
        float envOct = 0, vcfModOct = 0, kybd = 0;
        float vcoModOct = 0, modDepthOct = 0, bendOct = 0, bendVcfOct = 0;
        float volume = 0;
    };

    Mode modeFor(const Settings& s) const;
    void enterMode(Mode m);
    void updateTargets();

    // the CPU
    void push(Event::Type t, int note);
    void cpuPass();
    void keyDown(int note);
    void keyUp(int note);
    void clockRise();
    void clockFall();
    void clockReset();
    bool usingHostClock() const;
    bool lfoSynced() const;
    bool clockHigh() const;

    float tick();

    void playKey(int note, bool legato, bool retrigger);
    void gateOn(bool retrigger);
    void gateOff();
    void triggerEnvelope();

    void removeHeld(int note);
    void addArp(int note);
    void removeArp(int note);
    void rebuildArp();
    int  arpNext();

    void recordStep(uint32_t step);

    Settings settings;
    Mode mode = Mode::Keyboard;
    Targets tgt, cur;
    bool primed = false;

    float hostRate = 44100, rateInt = 176400;
    int   factor = 4;

    Vco vco;
    Vcf vcf;
    Envelope env;
    Lfo lfo;
    Decimator decimator;
    Random rng;

    // the CPU's inbox and its loop
    Event queue[256] {};
    int   queued = 0;
    int   cpuCountdown = 0;
    bool  clockWasHigh = false;

    // the host clock
    bool   hostAvailable = false, hostPlaying = false;
    double hostPpq = 0, hostPpqPerTick = 0;

    // The synced modulator's own copy of the timeline. It follows the host while the
    // transport runs, and keeps going at the last tempo while it is stopped, so the sound
    // does not freeze.
    double lfoPpq = 0, lfoPpqPerTick = 0, lastBpm = 120;
    double syncQuarters = 1;

    // keyboard
    int  held[128] {};
    int  numHeld = 0;
    int  key = 60;
    bool gate = false;
    bool glide = false;
    float keyTarget = 0, keyCv = 0;

    // KEY TRANSPOSE: a shift added to the RANGE data, so it moves the VCO but not the
    // filter's keyboard tracking
    std::atomic<bool> transposeArmed { false };
    std::atomic<int>  shift { 0 };

    // arpeggio: the notes it cycles through, low to high
    int arpNotes[128] {};
    int arpCount = 0, arpIdx = -1, arpDir = 1;

    // sequencer
    std::atomic<uint32_t> steps[maxSteps] {};
    std::atomic<int> length { 0 }, position { -1 };
    std::atomic<bool> legatoArmed { false };
    bool seqRunning = false;
    bool seqResting = false;
    int  seqTranspose = 0;

    // controllers
    float bend = 0, modLever = 0;

    // derived per block
    float lfoInc = 0, portaCoef = 1, smoothCoef = 1, gateCoef = 1;
    int   range = Range::Feet8;
    float transposeOct = 0, tuneOct = 0, maxFc = 20000;

    // the analog around the edges
    float gateLevel = 0;
    float drift = 0, driftTarget = 0, driftCoef = 0;
    int   driftCountdown = 0;
    float jitter = 0, jitterCoef = 0;
    float sag = 0, sagCoef = 0, regulated = 0, regulatorCoef = 0;
    float ripplePhase = 0, rippleInc = 0;

    float mixX1 = 0, mixY1 = 0, mixCoef = 0.9995f;
    float dcX1 = 0, dcY1 = 0, dcCoef = 0.999f;
    float volume = 0, volumeCoef = 0.01f;
};

} // namespace sh101
