// Mz101 - Copyright (C) 2026 Simon Moscrop
// Free software under the GNU AGPL v3; see LICENSE.

#include "Engine.h"

#include <algorithm>

namespace sh101
{

namespace
{
    constexpr float middleC = 261.6255653f;

    float panel(float v) { return clamp(v * 0.1f, 0.0f, 1.0f); }   // 0..10 -> 0..1

    // An audio-taper ("A") pot used as a variable resistor: 10% of its resistance at half
    // travel, the rest packed into the top half.
    float audioTaper(float x)
    {
        constexpr float k = 4.4f;   // exp(k/2) - 1 = 10% of exp(k) - 1
        return (std::exp(k * x) - 1.0f) / (std::exp(k) - 1.0f);
    }

    // An RC time set by an audio-taper pot in series with a fixed resistor: the fixed part
    // sets the shortest time, pot plus fixed the longest.
    float taperedTime(float x, float shortest, float longest)
    {
        return shortest + (longest - shortest) * audioTaper(x);
    }
}

void Engine::prepare(double hostSampleRate)
{
    hostRate = (float) hostSampleRate;
    factor = hostRate <= 50000.0f ? 4 : (hostRate <= 100000.0f ? 2 : 1);
    rateInt = hostRate * (float) factor;

    decimator.prepare(factor, hostRate);
    env.setSampleRate(rateInt);
    lfo.setSampleRate(rateInt);

    vco.reset();
    vcf.reset();
    env.reset();
    lfo.reset();

    smoothCoef = onePoleCoef(0.004f, rateInt);    // a slider cannot move faster than a hand
    gateCoef   = onePoleCoef(0.0005f, rateInt);   // the VCA's control input, in GATE mode
    driftCoef  = onePoleCoef(1.5f, rateInt);
    jitterCoef = onePoleCoef(1.0f / (2.0f * (float) pi * 25.0f), rateInt);
    sagCoef    = onePoleCoef(0.04f, rateInt);     // the load's own recovery
    regulatorCoef = onePoleCoef(0.25f, rateInt);  // the regulator catching up
    volumeCoef = onePoleCoef(0.01f, hostRate);

    // full-wave rectified 50 Hz mains, what is left of it after the regulators
    rippleInc = 100.0f / rateInt;

    // the mixer's coupling capacitor into the filter, and the output's
    mixCoef = 1.0f - (float) (2.0 * pi * 5.0 / rateInt);
    dcCoef  = 1.0f - (float) (2.0 * pi * 8.0 / hostRate);

    maxFc = std::min(0.35f * rateInt, 40000.0f);

    mixX1 = mixY1 = dcX1 = dcY1 = 0;
    gateLevel = sag = regulated = jitter = 0;
    gate = false;
    numHeld = arpCount = 0;
    queued = 0;
    cpuCountdown = 0;
    clockWasHigh = false;
    seqRunning = false;
    position = -1;
    primed = false;

    updateTargets();
}

//------------------------------------------------------------------------------ modes

Engine::Mode Engine::modeFor(const Settings& s) const
{
    // The CPU checks LOAD, then PLAY, then ARPEGGIO (flow chart, steps 9-11). In LOAD the
    // keyboard plays straight, so what goes in can be heard going in.
    if (s.seqMode == Seq::Load) return Mode::Keyboard;
    if (s.seqMode == Seq::Play) return Mode::Sequence;
    if (s.arpMode != Arp::Off)  return Mode::Arpeggio;
    return Mode::Keyboard;
}

void Engine::setSettings(const Settings& s)
{
    const bool wasHold = settings.arpHold;
    const bool enteringLoad = s.seqMode == Seq::Load && settings.seqMode != Seq::Load;
    const Mode next = modeFor(s);

    settings = s;

    // Pressing LOAD starts a new sequence from the first step.
    if (enteringLoad)
    {
        length = 0;
        legatoArmed = false;
    }

    if (next != mode)
    {
        enterMode(next);
    }
    else if (wasHold && ! s.arpHold && numHeld == 0)
    {
        // Releasing HOLD lets go of anything no longer under a finger.
        if (mode == Mode::Arpeggio) { arpCount = 0; gateOff(); }
        if (mode == Mode::Sequence) { seqRunning = false; position = -1; gateOff(); }
    }

    updateTargets();
}

void Engine::enterMode(Mode m)
{
    mode = m;
    arpCount = 0;
    seqRunning = false;
    position = -1;

    switch (m)
    {
        case Mode::Keyboard:
            // whatever is still held plays, as if just pressed
            if (numHeld > 0) playKey(held[numHeld - 1], false, true);
            else gateOff();
            break;

        case Mode::Arpeggio:
            rebuildArp();
            if (arpCount > 0) { arpIdx = -1; arpDir = 1; clockReset(); }
            else gateOff();
            break;

        case Mode::Sequence:
            gateOff();
            if (numHeld > 0 && length.load() > 0)
            {
                seqTranspose = held[numHeld - 1] - 60;
                seqRunning = true;
                seqResting = false;
            }
            break;
    }
}

void Engine::updateTargets()
{
    const auto& s = settings;

    // VCF: 10 Hz to 20 kHz across the slider (spec)
    tgt.cutoffLog  = std::log2(10.0f) + panel(s.cutoff) * std::log2(2000.0f);
    tgt.k          = 4.4f * panel(s.res);           // self-oscillation from about 9
    tgt.envOct     = 10.0f * panel(s.vcfEnv);
    tgt.vcfModOct  = 3.0f * panel(s.vcfMod);
    tgt.kybd       = panel(s.vcfKybd);              // spec: 0 - 100%

    // VCO and mixer
    tgt.pw    = panel(s.pw);
    tgt.pulse = panel(s.pulse);
    tgt.saw   = panel(s.saw);
    tgt.sub   = panel(s.sub);
    tgt.noise = panel(s.noise) * 0.7f;

    // modulation depths - squared, as a linear pot into a depth that is only useful at the
    // bottom of its range wants spreading out there
    const float vm = panel(s.vcoMod), md = panel(s.modDepth);
    tgt.vcoModOct   = vm * vm;                      // up to an octave either side
    tgt.modDepthOct = md * md * 0.5f;
    tgt.bendOct     = panel(s.bendVco);             // up to an octave
    tgt.bendVcfOct  = 4.0f * panel(s.bendVcf);

    const float v = panel(s.volume);
    tgt.volume = 1.4f * v * v;

    // Spec: attack 1.5 ms - 4 s, decay and release 2 ms - 10 s. The time sliders are taken
    // to be the 1M audio-taper parts in the parts list, as variable resistors - so most of
    // the range is in the top half of the travel, as on the hardware.
    env.set(taperedTime(panel(s.attack), 0.0015f, 4.0f),
            taperedTime(panel(s.decay), 0.002f, 10.0f),
            panel(s.sustain),
            taperedTime(panel(s.release), 0.002f, 10.0f));

    // spec: 0.1 - 30 Hz; or synced, a note length
    lfoInc = expLaw(panel(s.lfoRate), 0.1f, 30.0f) / rateInt;
    syncQuarters = syncDivisions[syncDivisionIndex(s.lfoRate)].quarters;

    // Spec: 0 - 5 s, on a 2M audio-taper pot (the bender board's PORTAMENTO TIME). The RC
    // takes about three time constants to arrive.
    const float tau = taperedTime(panel(s.portaTime), 0.0f, 5.0f) / 3.0f;
    portaCoef = tau < 0.0005f ? 1.0f : onePoleCoef(tau, rateInt);

    range        = s.range;
    transposeOct = (float) (s.transpose - Transpose::Mid);
    tuneOct      = s.tune / 1200.0f;

    if (! primed)
    {
        cur = tgt;
        volume = tgt.volume;
        primed = true;
    }
}

//------------------------------------------------------------------------------ the CPU

void Engine::push(Event::Type t, int note)
{
    if (note < 0 || note > 127 || queued >= (int) (sizeof queue / sizeof queue[0])) return;
    queue[queued++] = { t, (uint8_t) note };
}

void Engine::allNotesOff()
{
    queued = 0;
    numHeld = 0;
    arpCount = 0;
    seqRunning = false;
    position = -1;
    transposeArmed = false;
    gateOff();
}

// One pass of the CPU's loop: keyboard and buttons (steps 5 and 8-9), then the clock
// (step 6) and what hangs off it (10-11).
void Engine::cpuPass()
{
    for (int i = 0; i < queued; ++i)
    {
        const auto& e = queue[i];

        switch (e.type)
        {
            case Event::On:  keyDown(e.note); break;
            case Event::Off: keyUp(e.note); break;

            case Event::KeyTranspose:
                // In LOAD, the KEY TRANSPOSE button is REST (step 9).
                if (settings.seqMode == Seq::Load) { legatoArmed = false; recordStep(stepRest); }
                else transposeArmed = ! transposeArmed.load();
                break;

            case Event::Legato:
                if (settings.seqMode == Seq::Load) legatoArmed = ! legatoArmed.load();
                break;
        }
    }
    queued = 0;

    const bool high = clockHigh();
    if (high && ! clockWasHigh) clockRise();
    else if (! high && clockWasHigh) clockFall();
    clockWasHigh = high;
}

// Clock Reset (step 13): restart the modulator, and make sure this pass sees it go high.
// An external clock cannot be reset - the arpeggio waits for its next step instead.
void Engine::clockReset()
{
    // A modulator locked to the timeline belongs to the timeline: a key cannot restart it.
    if (lfoSynced()) return;

    lfo.restart();
    if (! usingHostClock()) clockWasHigh = false;
}

void Engine::setHostClock(bool available, bool playing, double ppq, double bpm)
{
    hostAvailable = available;
    hostPlaying = available && playing && bpm > 0;
    hostPpq = ppq;
    hostPpqPerTick = hostPlaying ? bpm / 60.0 / (double) rateInt : 0.0;

    if (available && bpm > 0) lastBpm = bpm;
    if (hostPlaying) lfoPpq = ppq;
    lfoPpqPerTick = lastBpm / 60.0 / (double) rateInt;
}

bool Engine::usingHostClock() const
{
    return settings.clockSource == Clock::Host && hostAvailable;
}

bool Engine::lfoSynced() const
{
    return settings.lfoSync == LfoSync::Host && hostAvailable;
}

// The clock as the CPU's input sees it: the modulator's square, or the host's beat grid
// cut into steps - high for the first half of each, low while the transport is stopped.
bool Engine::clockHigh() const
{
    if (! usingHostClock()) return lfo.clockHigh();
    if (! hostPlaying) return false;

    static constexpr double stepsPerBeat[] = { 1, 2, 3, 4, 6, 8 };
    const double x = hostPpq * stepsPerBeat[std::min(std::max(settings.clockDiv, 0), 5)];
    return x - std::floor(x) < 0.5;
}

void Engine::keyDown(int note)
{
    // KEY TRANSPOSE then a key: that key, relative to the bottom C of the 101's keyboard
    // (middle C), becomes the shift. It is not played.
    if (transposeArmed.load())
    {
        shift = note - 60;
        transposeArmed = false;
        return;
    }

    const bool othersHeld = numHeld > 0;
    removeHeld(note);
    held[numHeld++] = note;

    switch (mode)
    {
        case Mode::Arpeggio:
        {
            // With HOLD on, a fresh chord - one played after every key was let go - replaces
            // the latched one rather than adding to it.
            if (settings.arpHold && ! othersHeld) arpCount = 0;

            if (arpCount == 0) { arpIdx = -1; arpDir = 1; }
            addArp(note);

            // the CPU resets the clock on every key in ARPEGGIO
            clockReset();
            return;
        }

        case Mode::Sequence:
            // The key transposes the sequence, relative to middle C, and starts it - on the
            // next clock: there is no Clock Reset in PLAY.
            seqTranspose = note - 60;
            if (! seqRunning && length.load() > 0)
            {
                seqRunning = true;
                seqResting = false;
                position = -1;
            }
            return;

        case Mode::Keyboard:
            if (settings.seqMode == Seq::Load)
            {
                // Overlapping the previous key enters a tie, the same as LEGATO does.
                const bool tie = legatoArmed.exchange(false) || othersHeld;
                recordStep((uint32_t) note | (tie ? stepTie : 0u));
            }

            // ... and resets the clock on every key with the envelope on LFO trigger
            if (settings.envTrig == EnvTrig::Lfo) clockReset();

            playKey(note, othersHeld && gate, true);
            return;
    }
}

void Engine::keyUp(int note)
{
    removeHeld(note);

    switch (mode)
    {
        case Mode::Arpeggio:
            if (! settings.arpHold)
            {
                removeArp(note);
                if (arpCount == 0) gateOff();
            }
            return;

        case Mode::Sequence:
            if (numHeld > 0)
                seqTranspose = held[numHeld - 1] - 60;
            else if (! settings.arpHold)
            {
                seqRunning = false;
                position = -1;
                gateOff();
            }
            return;

        case Mode::Keyboard:
            if (numHeld == 0)
                gateOff();
            else if (note == key)
                playKey(held[numHeld - 1], true, false);   // back to the note still held, legato
            return;
    }
}

void Engine::clockRise()
{
    // step 6(a): random data for the modulator's RANDOM waveform
    lfo.setRandom(rng.bipolar());

    if (mode == Mode::Arpeggio)
    {
        if (arpCount > 0) playKey(arpNext(), false, true);
        return;
    }

    if (mode != Mode::Sequence || ! seqRunning) return;

    const int n = length.load();
    if (n == 0) { seqRunning = false; gateOff(); return; }

    const int p = (position.load() + 1) % n;
    position = p;

    const uint32_t step = steps[p].load();

    if (step & stepRest)
    {
        seqResting = true;
        gateOff();
        return;
    }

    // A tie only means something coming from a note: after a rest it is a new note.
    const bool tied = (step & stepTie) != 0 && ! seqResting && gate;
    seqResting = false;

    const int note = std::min(127, std::max(0, (int) (step & 0x7fu) + seqTranspose));
    playKey(note, tied, ! tied);
}

// The gate closes for the low half of the clock - except into a tied step.
void Engine::clockFall()
{
    if (mode == Mode::Arpeggio)
    {
        gateOff();
        return;
    }

    if (mode != Mode::Sequence || ! seqRunning || seqResting) return;

    const int n = length.load();
    if (n == 0) return;

    const uint32_t next = steps[(position.load() + 1) % n].load();
    const bool nextTied = (next & stepTie) != 0 && (next & stepRest) == 0;
    if (! nextTied) gateOff();
}

//------------------------------------------------------------------------------ keyboard

void Engine::playKey(int note, bool legato, bool retrigger)
{
    key = note;
    keyTarget = (float) (note - 60) / 12.0f;

    glide = settings.portaMode == Porta::On
         || (settings.portaMode == Porta::Auto && legato);

    if (! glide) keyCv = keyTarget;

    // GATE mode lets a legato note through without restarting the envelope; GATE+TRIG and
    // LFO restart it on every new key.
    const bool trig = retrigger && (! legato || settings.envTrig != EnvTrig::Gate);
    gateOn(trig);
}

void Engine::gateOn(bool retrigger)
{
    if (! gate || retrigger) triggerEnvelope();
    gate = true;
}

void Engine::gateOff()
{
    gate = false;
    env.release();
}

void Engine::triggerEnvelope()
{
    env.trigger();

    // The envelope's capacitor charging, and the VCA opening, draw a gulp of current: the
    // rail dips for a few tens of milliseconds, and pitch and cutoff dip with it.
    sag += 1.0f;
}

void Engine::removeHeld(int note)
{
    int j = 0;
    for (int i = 0; i < numHeld; ++i)
        if (held[i] != note) held[j++] = held[i];
    numHeld = j;
}

//------------------------------------------------------------------------------ arpeggio

void Engine::addArp(int note)
{
    int i = 0;
    while (i < arpCount && arpNotes[i] < note) ++i;
    if (i < arpCount && arpNotes[i] == note) return;

    for (int j = arpCount; j > i; --j) arpNotes[j] = arpNotes[j - 1];
    arpNotes[i] = note;
    ++arpCount;
}

void Engine::removeArp(int note)
{
    int j = 0;
    for (int i = 0; i < arpCount; ++i)
        if (arpNotes[i] != note) arpNotes[j++] = arpNotes[i];
    arpCount = j;
}

void Engine::rebuildArp()
{
    arpCount = 0;
    for (int i = 0; i < numHeld; ++i) addArp(held[i]);
}

int Engine::arpNext()
{
    const int n = arpCount;

    switch (settings.arpMode)
    {
        case Arp::Down:
            arpIdx = (arpIdx <= 0 || arpIdx > n ? n : arpIdx) - 1;
            break;

        case Arp::UpDown:
            if (n == 1) { arpIdx = 0; break; }
            arpIdx += arpDir;
            if (arpIdx >= n) { arpIdx = n - 2; arpDir = -1; }
            if (arpIdx < 0)  { arpIdx = 1; arpDir = 1; }
            break;

        default:
            arpIdx = (arpIdx + 1) % n;
            break;
    }

    arpIdx = std::min(std::max(arpIdx, 0), n - 1);
    return arpNotes[arpIdx];
}

//------------------------------------------------------------------------------ sequencer

void Engine::recordStep(uint32_t step)
{
    const int n = length.load();
    if (n >= maxSteps) return;
    steps[n] = step;
    length = n + 1;
}

void Engine::loadSequence(const uint32_t* data, int count)
{
    count = std::min(std::max(count, 0), maxSteps);
    length = 0;
    for (int i = 0; i < count; ++i)
        steps[i] = data[i] & (0x7fu | stepRest | stepTie);
    length = count;
}

//------------------------------------------------------------------------------ audio

float Engine::tick()
{
    hostPpq += hostPpqPerTick;

    if (--cpuCountdown <= 0)
    {
        cpuPass();
        cpuCountdown = (int) (rateInt * (0.0015f + 0.002f * rng.uniform()));
    }

    // The envelope's LFO trigger is wired from the modulator through the trigger
    // generator, not through the CPU, so it is on time.
    Lfo::Tick lt;
    if (lfoSynced())
    {
        lfoPpq += lfoPpqPerTick;
        const double cycles = lfoPpq / syncQuarters;
        lt = lfo.processAt((float) (cycles - std::floor(cycles)), settings.lfoWave, rng);
    }
    else
    {
        lt = lfo.process(lfoInc, settings.lfoWave, rng);
    }
    if (mode == Mode::Keyboard && settings.envTrig == EnvTrig::Lfo && gate && lt.cycleStart)
        triggerEnvelope();

    // portamento: an RC on the keyboard CV - fast at first, slowing as it arrives
    if (glide) keyCv += (keyTarget - keyCv) * portaCoef;
    else       keyCv = keyTarget;

    auto smooth = [this] (float& v, float t) { v += (t - v) * smoothCoef; };
    smooth(cur.cutoffLog, tgt.cutoffLog);
    smooth(cur.k, tgt.k);
    smooth(cur.pw, tgt.pw);
    smooth(cur.pulse, tgt.pulse);
    smooth(cur.saw, tgt.saw);
    smooth(cur.sub, tgt.sub);
    smooth(cur.noise, tgt.noise);

    const float e = env.process();
    gateLevel += ((gate ? 1.0f : 0.0f) - gateLevel) * gateCoef;

    //---------------------------------------------------------------- the analog
    // Thermal drift: a couple of cents, wandering over seconds. Enough that two notes are
    // never quite the same; not enough to sound out of tune.
    if (--driftCountdown <= 0)
    {
        driftCountdown = (int) (rateInt * (0.3f + 0.9f * rng.uniform()));
        driftTarget = rng.noise() * 0.0012f;
    }
    drift += (driftTarget - drift) * driftCoef;

    // And faster, much smaller: the expo converter's own noise, a fraction of a cent.
    jitter += (rng.noise() * 0.004f - jitter) * jitterCoef;

    // The rail: sags on envelope triggers and on the oscillator's edges switching, and
    // carries a trace of mains ripple that grows as it sags. The regulator corrects a STEADY
    // load - so `regulated`, a slow copy of the sag, is taken off it: only changes in load
    // move the rail. Without that a high note, all edges, would sit permanently flat.
    sag -= sag * sagCoef;
    regulated += (sag - regulated) * regulatorCoef;
    const float dip = sag - regulated;

    ripplePhase += rippleInc;
    if (ripplePhase >= 1.0f) ripplePhase -= 1.0f;
    const float ripple = std::sin(2.0f * (float) pi * ripplePhase) * (1.0f + std::fabs(dip));
    const float rail = dip + 0.05f * ripple;

    //---------------------------------------------------------------- VCO
    // The RANGE data the CPU writes: 1 V at 16' to 4 V at 2', plus any KEY TRANSPOSE shift,
    // within the D/A converter's 0 - 5 V. 8' (2 V) is the reference.
    const float rangeV = clamp((float) (range + 1) + (float) shift.load(std::memory_order_relaxed) / 12.0f, 0.0f, 5.0f);

    const float keyed = keyCv + transposeOct;
    const float lfoPitch = lt.value * (tgt.vcoModOct + modLever * tgt.modDepthOct);
    const float pitch = keyed + (rangeV - 2.0f) + tuneOct + bend * tgt.bendOct + lfoPitch
                      + drift + jitter - 0.0015f * rail;
    const float dt = std::min(middleC * std::exp2(pitch) / rateInt, 0.45f);

    float pwmAmount = 1.0f;
    if (settings.pwmSource == PwmSource::Lfo)      pwmAmount = 0.5f * (lt.value + 1.0f);
    else if (settings.pwmSource == PwmSource::Env) pwmAmount = e;

    // spec: 50% down to 0% - at the top the pulse vanishes
    const float width = 0.5f * (1.0f - cur.pw * pwmAmount);

    Vco::Levels levels;
    levels.pulse = cur.pulse;
    levels.saw = cur.saw;
    levels.sub = cur.sub;
    levels.subType = settings.subType;

    //---------------------------------------------------------------- MIXER
    const float mixed = vco.process(dt, width, levels) + cur.noise * rng.noise();
    sag += 0.01f * vco.lastEdgeEnergy();

    const float coupled = mixed - mixX1 + mixCoef * mixY1;
    mixX1 = mixed;
    mixY1 = coupled;

    //---------------------------------------------------------------- VCF
    const float fLog = cur.cutoffLog
                     + e * tgt.envOct
                     + lt.value * tgt.vcfModOct
                     + keyed * tgt.kybd
                     + bend * tgt.bendVcfOct
                     - 0.03f * rail;

    const float fc = clamp(std::exp2(fLog), 5.0f, maxFc);
    const float g = std::tan((float) pi * fc / rateInt);

    // A trace of thermal noise at the input, so the filter can start to self-oscillate
    // from silence as the real one does.
    const float filtered = vcf.process(0.5f * coupled + 1.0e-5f * rng.noise(), g, cur.k);

    //---------------------------------------------------------------- VCA (BA662A)
    const float amp = settings.vcaMode == VcaMode::Gate ? gateLevel : e;
    return softClip(filtered * amp);
}

void Engine::render(float* out, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        for (int r = 0; r < factor; ++r)
            decimator.push(tick());

        const float y = decimator.output();

        const float hp = y - dcX1 + dcCoef * dcY1;
        dcX1 = y;
        dcY1 = hp;

        volume += (tgt.volume - volume) * volumeCoef;
        out[i] = hp * volume;
    }
}

} // namespace sh101
