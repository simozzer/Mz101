//
// The engine, checked on its own: pitch, the sub dividers, filter self-oscillation and
// tracking, the envelope letting go, the arpeggiator's order, and that no setting of any
// control makes it blow up. Links no JUCE.
//
//     DspCheck                 run the checks
//     DspCheck --wav <dir>     and also write a few demo phrases there to listen to
//

#define _CRT_SECURE_NO_WARNINGS   // fopen, for the demo renders

#include "Engine.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace sh101;

namespace
{

constexpr double rate = 48000.0;
int failures = 0;

void check(bool ok, const char* what, const std::string& detail)
{
    std::printf("  %s  %-44s %s\n", ok ? "ok  " : "FAIL", what, detail.c_str());
    if (! ok) ++failures;
}

std::string fmt(const char* f, double a, double b = 0)
{
    char buf[128];
    std::snprintf(buf, sizeof buf, f, a, b);
    return buf;
}

std::vector<float> render(Engine& e, double seconds)
{
    std::vector<float> v((size_t) (seconds * rate));
    e.render(v.data(), (int) v.size());
    return v;
}

// Frequency from rising zero crossings, interpolated, over the second half of the buffer.
double frequency(const std::vector<float>& v)
{
    double first = -1, last = -1;
    int count = 0;

    for (size_t i = v.size() / 2 + 1; i < v.size(); ++i)
    {
        if (v[i - 1] < 0 && v[i] >= 0)
        {
            const double t = (double) (i - 1) + v[i - 1] / (double) (v[i - 1] - v[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }

    return count > 0 ? count * rate / (last - first) : 0.0;
}

double peak(const std::vector<float>& v, size_t from = 0)
{
    double p = 0;
    for (size_t i = from; i < v.size(); ++i) p = std::max(p, (double) std::fabs(v[i]));
    return p;
}

double rms(const std::vector<float>& v, size_t from = 0)
{
    double s = 0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return std::sqrt(s / (double) std::max<size_t>(1, v.size() - from));
}

// An open, plain patch: one source, filter wide open, no envelope on the filter, held.
Settings plain()
{
    Settings s;
    s.saw = 10; s.pulse = 0; s.sub = 0; s.noise = 0;
    s.cutoff = 10; s.res = 0; s.vcfEnv = 0; s.vcfKybd = 0;
    s.attack = 0; s.decay = 0; s.sustain = 10; s.release = 0;
    return s;
}

double pitchOf(const Settings& s, int note)
{
    Engine e;
    e.prepare(rate);
    e.setSettings(s);
    e.noteOn(note);
    return frequency(render(e, 1.0));
}

void pitchChecks()
{
    std::printf("\nthe VCO\n");

    struct Case { const char* what; double expect; Settings s; int note; };
    std::vector<Case> cases;

    auto add = [&] (const char* what, double expect, int note, auto tweak)
    {
        Settings s = plain();
        tweak(s);
        cases.push_back({ what, expect, s, note });
    };

    add("saw, A4 at 8'",               440.0, 69, [] (Settings&) {});
    add("saw, A4 at 16'",              220.0, 69, [] (Settings& s) { s.range = Range::Feet16; });
    add("saw, A4 at 4'",               880.0, 69, [] (Settings& s) { s.range = Range::Feet4; });
    add("saw, A4 at 2'",              1760.0, 69, [] (Settings& s) { s.range = Range::Feet2; });
    add("pulse 50%, A4",               440.0, 69, [] (Settings& s) { s.saw = 0; s.pulse = 10; });
    add("pulse 5% (PW 9), A4",         440.0, 69, [] (Settings& s) { s.saw = 0; s.pulse = 10; s.pw = 9; });
    add("sub -1 oct square, A4",       220.0, 69, [] (Settings& s) { s.saw = 0; s.sub = 10; });
    add("sub -2 oct square, A4",       110.0, 69, [] (Settings& s) { s.saw = 0; s.sub = 10; s.subType = SubType::OctDown2; });
    add("sub -2 oct pulse, A4",        110.0, 69, [] (Settings& s) { s.saw = 0; s.sub = 10; s.subType = SubType::Pulse2; });
    add("transpose high, A4",          880.0, 69, [] (Settings& s) { s.transpose = Transpose::High; });
    add("tune +50 cents, A4",  440.0 * std::pow(2.0, 50.0 / 1200.0), 69, [] (Settings& s) { s.tune = 50; });
    add("saw, C8 at 4' (8.4 kHz)",  4186.0 * 2, 108, [] (Settings& s) { s.range = Range::Feet4; });

    for (auto& c : cases)
    {
        const double f = pitchOf(c.s, c.note);
        const double cents = 1200.0 * std::log2(f / c.expect);
        check(std::fabs(cents) < 6.0, c.what, fmt("%.2f Hz (%+.1f cents)", f, cents));
    }
}

void filterChecks()
{
    std::printf("\nthe VCF\n");

    // Self-oscillation, from nothing but the filter's own noise, tracking the keyboard.
    auto osc = [] (int note, double& f, double& level)
    {
        Settings s = plain();
        s.saw = 0; s.res = 10; s.cutoff = 5; s.vcfKybd = 10;
        s.vcaMode = VcaMode::Gate;
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(note);
        const auto v = render(e, 1.5);
        f = frequency(v);
        level = rms(v, v.size() / 2);
    };

    double f1, l1, f2, l2;
    osc(48, f1, l1);
    osc(60, f2, l2);

    check(l1 > 0.02 && l2 > 0.02, "self-oscillates at full resonance", fmt("rms %.3f, %.3f", l1, l2));
    check(f1 > 0 && std::fabs(f2 / f1 - 2.0) < 0.1, "KYBD 10 tracks an octave as an octave", fmt("%.1f Hz -> %.1f Hz", f1, f2));

    // Closing the filter takes the level down.
    auto level = [] (float cutoff)
    {
        Settings s = plain();
        s.cutoff = cutoff;
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(57);
        const auto v = render(e, 0.5);
        return rms(v, v.size() / 2);
    };

    const double open = level(10), shut = level(1);
    check(shut < open * 0.1, "closing the filter takes the saw away", fmt("rms %.3f open, %.4f at 1", open, shut));

    // Resonance thins the bottom: a low sub well under the cutoff loses level as resonance
    // goes up - less than a plain four-pole would, because of the phase splitter's forward
    // half, but it does lose it.
    auto bass = [] (float res)
    {
        Settings s = plain();
        s.saw = 0; s.sub = 10; s.subType = SubType::OctDown2;
        s.cutoff = 6; s.res = res;
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(36);
        const auto v = render(e, 1.0);
        return rms(v, v.size() / 2);
    };

    const double dry = bass(0), wet = bass(8);
    const double lossDb = 20 * std::log10(wet / dry);
    check(lossDb < -1.5 && lossDb > -10.0, "resonance thins the bass", fmt("%.1f dB at RES 8", lossDb));
}

void envelopeChecks()
{
    std::printf("\nthe envelope and VCA\n");

    Settings s = plain();
    Engine e;
    e.prepare(rate);
    e.setSettings(s);
    e.noteOn(57);
    render(e, 0.3);
    e.noteOff(57);
    const auto tail = render(e, 0.1);
    check(peak(tail, (size_t) (0.05 * rate)) < 1.0e-3, "release at 0 lets go within 50 ms", fmt("peak after 50 ms %.2e", peak(tail, (size_t) (0.05 * rate))));

    // attack at 10 is four seconds to the top: one second in, still well short
    s.attack = 10;
    e.prepare(rate);
    e.setSettings(s);
    e.noteOn(57);
    const auto rise = render(e, 4.5);
    const double at1 = rms(std::vector<float>(rise.begin() + (size_t) (0.9 * rate), rise.begin() + (size_t) (1.0 * rate)));
    const double at4 = rms(std::vector<float>(rise.begin() + (size_t) (4.3 * rate), rise.begin() + (size_t) (4.4 * rate)));
    check(at1 < at4 * 0.5 && at4 > 0.05, "attack at 10 takes seconds", fmt("rms %.3f at 1 s, %.3f at 4.3 s", at1, at4));

    // the default patch should sit at a sensible level
    Settings d;
    e.prepare(rate);
    e.setSettings(d);
    e.noteOn(48);
    const auto v = render(e, 1.0);
    const double p = peak(v);
    check(p > 0.1 && p < 1.0, "default patch peaks below full scale", fmt("peak %.2f (%.1f dBFS)", p, 20 * std::log10(p)));
}

void arpChecks()
{
    std::printf("\nthe arpeggiator\n");

    auto run = [] (int mode)
    {
        Settings s = plain();
        s.arpMode = mode;
        s.lfoRate = 8;
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(60); e.noteOn(67); e.noteOn(64);

        std::string seq;
        int last = -1;
        float buf[16];
        bool wasGate = false;
        for (int i = 0; i < (int) (rate * 1.2) / 16; ++i)
        {
            e.render(buf, 16);
            if (e.gateIsOn() && ! wasGate)
            {
                if (seq.size() < 40) seq += std::to_string(e.currentKey()) + " ";
                last = e.currentKey();
            }
            wasGate = e.gateIsOn();
        }
        (void) last;
        return seq;
    };

    const auto up = run(Arp::Up), ud = run(Arp::UpDown), down = run(Arp::Down);
    check(up.rfind("60 64 67 60 64 67", 0) == 0, "UP", up);
    check(ud.rfind("60 64 67 64 60 64 67", 0) == 0, "UP & DOWN", ud);
    check(down.rfind("67 64 60 67 64 60", 0) == 0, "DOWN", down);
}

void sequencerChecks()
{
    std::printf("\nthe sequencer\n");

    Settings s = plain();
    s.lfoRate = 8;
    s.seqMode = Seq::Load;

    Engine e;
    e.prepare(rate);
    e.setSettings(s);

    float buf[64];
    auto wait = [&] { for (int k = 0; k < 8; ++k) e.render(buf, 64); };   // ~10 ms: past a CPU pass
    auto press = [&] (int n) { e.noteOn(n); wait(); e.noteOff(n); wait(); };

    // C, rest, E, then G tied from E (LEGATO), then C held while D is pressed (overlap ties)
    press(60);
    e.keyTranspose();   // REST, in LOAD
    press(64);
    e.legato();
    press(67);
    e.noteOn(60); wait(); e.noteOn(62); wait(); e.noteOff(60); e.noteOff(62); wait();

    std::string loaded;
    for (int i = 0; i < e.seqLength(); ++i)
    {
        const uint32_t st = e.seqStep(i);
        loaded += (st & Engine::stepRest) ? std::string("rest") : std::to_string(st & 0x7f);
        if (st & Engine::stepTie) loaded += "~";
        loaded += " ";
    }
    check(loaded == "60 rest 64 67~ 60 62~ ", "LOAD records notes, rests and ties", loaded);

    // Play it, transposed up a fourth by holding F.
    s.seqMode = Seq::Play;
    e.setSettings(s);
    e.noteOn(65);

    std::string played;
    int lastPos = -2, triggers = 0;
    bool wasGate = false;
    for (int i = 0; i < (int) (rate * 1.0) / 16 && lastPos < 5; ++i)
    {
        e.render(buf, 16);
        const int p = e.seqPosition();
        if (p != lastPos)
        {
            lastPos = p;
            if (p >= 0) played += (e.gateIsOn() ? std::to_string(e.currentKey()) : std::string("rest")) + " ";
        }
        if (e.gateIsOn() && ! wasGate) ++triggers;
        wasGate = e.gateIsOn();
    }
    check(played == "65 rest 69 72 65 67 ", "PLAY transposes by the key held", played);
    check(triggers == 3, "ties hold the gate open into the next note", fmt("%.0f gate openings for 5 notes, 2 tied", triggers));

    e.noteOff(65);
    wait();
    check(! e.gateIsOn(), "letting go stops it (HOLD off)", "");
}

// What the service manual adds: PW reaching 0%, the CPU's loop, and KEY TRANSPOSE.
void serviceManualChecks()
{
    std::printf("\nfrom the service manual\n");

    auto pulseLevel = [] (float pw)
    {
        Settings s = plain();
        s.saw = 0; s.pulse = 10; s.pw = pw;
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(57);
        const auto v = render(e, 0.5);
        return rms(v, v.size() / 2);
    };
    const double half = pulseLevel(0), none = pulseLevel(10);
    check(none < half * 0.02, "PW 10 is 0%: the pulse vanishes", fmt("rms %.3f at 50%%, %.4f at 0%%", half, none));

    // KEY TRANSPOSE, then G: up a fifth from middle C, and G itself is not played
    {
        Settings s = plain();
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.keyTranspose();
        e.noteOn(67);
        render(e, 0.01);
        const bool silentKey = ! e.gateIsOn();
        e.noteOff(67);
        e.noteOn(69);
        const double f = frequency(render(e, 1.0));
        const double expect = 440.0 * std::pow(2.0, 7.0 / 12.0);
        check(silentKey && std::fabs(1200.0 * std::log2(f / expect)) < 6.0, "KEY TRANSPOSE + G shifts A4 up a fifth",
              fmt("%.2f Hz, expected %.2f", f, expect));
    }

    // The CPU's loop: a key reaches the gate 0 - 3.5 ms late, and not always equally late.
    {
        Settings s = plain();
        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        double lo = 1e9, hi = 0;
        float one;
        for (int trial = 0; trial < 40; ++trial)
        {
            e.noteOn(60);
            int n = 0;
            while (! e.gateIsOn() && n < 1000) { e.render(&one, 1); ++n; }
            const double ms = 1000.0 * n / rate;
            lo = std::min(lo, ms);
            hi = std::max(hi, ms);
            e.noteOff(60);
            for (int k = 0; k < 400; ++k) e.render(&one, 1);
        }
        check(hi <= 3.6 && hi - lo > 0.5, "keys land within one CPU pass, unevenly", fmt("%.2f - %.2f ms", lo, hi));
    }
}

// CLOCK on HOST: steps follow the host's tempo and transport, not the modulator.
void hostClockChecks()
{
    std::printf("\nthe host clock\n");

    // Counts arpeggio steps over `seconds`, the host feeding its position every 512 samples
    // the way a DAW does.
    auto steps = [] (bool available, bool playing, double bpm, int div, double seconds, double* firstMs = nullptr, float lfoRate = 2)
    {
        Settings s = plain();
        s.arpMode = Arp::Up;
        s.lfoRate = lfoRate;           // slow by default, so it cannot be what is clocking
        s.clockSource = Clock::Host;
        s.clockDiv = div;

        Engine e;
        e.prepare(rate);
        e.setSettings(s);
        e.noteOn(60); e.noteOn(64);

        const int block = 512;
        float buf[block];
        int count = 0, n = 0;
        bool was = false;
        double ppq = 0;

        for (int b = 0; b < (int) (seconds * rate) / block; ++b)
        {
            e.setHostClock(available, playing, ppq, bpm);
            for (int i = 0; i < block; i += 16)
            {
                e.render(buf, 16);
                n += 16;
                if (e.gateIsOn() && ! was)
                {
                    if (count == 0 && firstMs) *firstMs = 1000.0 * n / rate;
                    ++count;
                }
                was = e.gateIsOn();
            }
            if (playing) ppq += bpm / 60.0 * block / rate;
        }
        return count;
    };

    double first = 0;
    const int sixteenths = steps(true, true, 120, ClockDiv::Sixteenth, 2.0, &first);
    check(sixteenths >= 15 && sixteenths <= 16, "120 BPM in 16ths: 8 steps a second", fmt("%.0f steps in 2 s", sixteenths));
    check(first < 4.0, "the first step lands on the downbeat", fmt("%.2f ms after the transport starts", first));

    const int triplets = steps(true, true, 90, ClockDiv::EighthT, 2.0);
    check(triplets >= 8 && triplets <= 9, "90 BPM in 1/8 triplets: 4.5 steps a second", fmt("%.0f steps in 2 s", triplets));

    const int stopped = steps(true, false, 120, ClockDiv::Sixteenth, 1.0);
    check(stopped == 0, "transport stopped: no steps", fmt("%.0f steps", stopped));

    // SYNC on HOST, CLOCK on INT: the modulator itself is on the grid, and the arpeggio
    // steps on the modulator - so it lands on the grid too. RATE picks 1/16 here.
    auto synced = [] (bool playing, double seconds, double* firstMs)
    {
        Settings s = plain();
        s.arpMode = Arp::Up;
        s.clockSource = Clock::Internal;
        s.lfoSync = LfoSync::Host;
        s.lfoRate = 10.0f * 13.0f / (float) (numSyncDivisions - 1);   // "1/16"

        Engine e;
        e.prepare(rate);
        e.setSettings(s);

        const int block = 512;
        float buf[block];
        int count = 0, n = 0;
        bool was = false;
        double ppq = 0;

        for (int b = 0; b < (int) (seconds * rate) / block; ++b)
        {
            e.setHostClock(true, playing, ppq, 120);
            if (b == 3) { e.noteOn(60); e.noteOn(64); }   // ~32 ms in: mid-step
            for (int i = 0; i < block; i += 16)
            {
                e.render(buf, 16);
                n += 16;
                if (e.gateIsOn() && ! was)
                {
                    if (count == 0 && firstMs) *firstMs = 1000.0 * n / rate;
                    ++count;
                }
                was = e.gateIsOn();
            }
            if (playing) ppq += 120.0 / 60.0 * block / rate;
        }
        return count;
    };

    double syncedFirst = 0;
    const int lockedSteps = synced(true, 2.0, &syncedFirst);
    check(sh101::syncDivisions[13].quarters == 0.25 && lockedSteps >= 14 && lockedSteps <= 16,
          "SYNC HOST at 1/16, 120 BPM: 8 cycles a second", fmt("%.0f steps in 2 s", lockedSteps));
    check(std::fabs(syncedFirst - 125.0) < 4.0, "a key mid-step waits for the grid", fmt("key at 32 ms, first step at %.2f ms (grid: 125)", syncedFirst));

    const int freewheel = synced(false, 2.0, nullptr);
    check(freewheel >= 15 && freewheel <= 17, "transport stopped: it keeps going at the last tempo", fmt("%.0f steps in 2 s", freewheel));

    // no host timeline at all (the standalone app): the modulator clocks it after all
    const int standalone = steps(false, false, 120, ClockDiv::Sixteenth, 2.0, nullptr, 8.0f);
    const double lfoHz = 0.1 * std::pow(300.0, 0.8);
    check(std::abs(standalone - 2.0 * lfoHz) <= 1.5, "no host: falls back to the modulator", fmt("%.0f steps, LFO %.2f Hz", standalone, lfoHz));
}

void fuzz()
{
    std::printf("\nevery control, anywhere\n");

    Random r(1234);
    double worst = 0;
    bool finite = true;

    for (int run = 0; run < 150; ++run)
    {
        Settings s;
        auto pick = [&] { return r.uniform() < 0.15f ? (r.uniform() < 0.5f ? 0.0f : 10.0f) : r.uniform() * 10.0f; };

        s.lfoRate = pick(); s.lfoWave = (int) (r.uniform() * 4);
        s.vcoMod = pick(); s.range = (int) (r.uniform() * 4); s.pw = pick(); s.pwmSource = (int) (r.uniform() * 3);
        s.pulse = pick(); s.saw = pick(); s.sub = pick(); s.subType = (int) (r.uniform() * 3); s.noise = pick();
        s.cutoff = pick(); s.res = pick(); s.vcfEnv = pick(); s.vcfMod = pick(); s.vcfKybd = pick();
        s.vcaMode = (int) (r.uniform() * 2);
        s.attack = pick(); s.decay = pick(); s.sustain = pick(); s.release = pick(); s.envTrig = (int) (r.uniform() * 3);
        s.portaMode = (int) (r.uniform() * 3); s.portaTime = pick();
        s.bendVco = pick(); s.bendVcf = pick(); s.modDepth = pick();
        s.arpMode = (int) (r.uniform() * 4); s.arpHold = r.uniform() < 0.5f;
        s.seqMode = r.uniform() < 0.7f ? Seq::Off : (r.uniform() < 0.5f ? Seq::Play : Seq::Load);
        s.transpose = (int) (r.uniform() * 3); s.volume = 10;

        Engine e;
        e.prepare(r.uniform() < 0.5f ? 44100.0 : 96000.0);
        e.setSettings(s);
        e.setPitchBend(r.bipolar());
        e.setModLever(r.uniform());

        std::vector<float> v(4096);
        for (int k = 0; k < 6; ++k)
        {
            const int note = 24 + (int) (r.uniform() * 84);
            e.noteOn(note);
            e.render(v.data(), (int) v.size());
            for (float x : v) { if (! std::isfinite(x)) finite = false; worst = std::max(worst, (double) std::fabs(x)); }
            if (r.uniform() < 0.6f) e.noteOff(note);
        }
    }

    check(finite, "150 random patches, always finite", "");
    check(worst < 2.0, "and never runs away", fmt("worst peak %.2f at full volume", worst));
}

void speed()
{
    std::printf("\ncost\n");

    Settings s;
    s.res = 7; s.sub = 6; s.pulse = 5; s.noise = 2;
    Engine e;
    e.prepare(44100.0);
    e.setSettings(s);
    e.noteOn(48);

    std::vector<float> v(44100 * 10);
    const auto t0 = std::chrono::steady_clock::now();
    e.render(v.data(), (int) v.size());
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    check(secs < 10.0, "10 s of audio at 44.1 kHz (4x inside)", fmt("%.0f ms - %.0fx realtime", secs * 1000, 10.0 / secs));
}

//------------------------------------------------------------------------------ demo renders

void writeWav(const std::string& path, const std::vector<float>& v)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (! f) { std::printf("  could not write %s\n", path.c_str()); return; }

    auto u32 = [&] (uint32_t x) { std::fwrite(&x, 4, 1, f); };
    auto u16 = [&] (uint16_t x) { std::fwrite(&x, 2, 1, f); };

    const uint32_t bytes = (uint32_t) v.size() * 2;
    std::fwrite("RIFF", 1, 4, f); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(1); u32((uint32_t) rate); u32((uint32_t) rate * 2); u16(2); u16(16);
    std::fwrite("data", 1, 4, f); u32(bytes);
    for (float x : v) { const int16_t s = (int16_t) std::lround(clamp(x, -1.0f, 1.0f) * 32767.0f); std::fwrite(&s, 2, 1, f); }
    std::fclose(f);
    std::printf("  wrote %s\n", path.c_str());
}

void phrase(const std::string& path, const Settings& s, const std::vector<int>& notes, double step, double gate)
{
    Engine e;
    e.prepare(rate);
    e.setSettings(s);
    std::vector<float> out;

    for (int n : notes)
    {
        if (n >= 0) e.noteOn(n);
        auto on = render(e, step * gate);
        if (n >= 0) e.noteOff(n);
        auto off = render(e, step * (1 - gate));
        out.insert(out.end(), on.begin(), on.end());
        out.insert(out.end(), off.begin(), off.end());
    }
    auto tail = render(e, 1.0);
    out.insert(out.end(), tail.begin(), tail.end());
    writeWav(path, out);
}

void demos(const std::string& dir)
{
    std::printf("\ndemos\n");
    const std::vector<int> bass = { 36, 36, 48, 36, 39, 36, 46, 43, 36, 36, 48, 36, 41, 39, 34, 36 };

    Settings acid;
    acid.saw = 10; acid.cutoff = 2.5f; acid.res = 8; acid.vcfEnv = 6; acid.vcfKybd = 3;
    acid.decay = 3; acid.sustain = 0; acid.release = 1;
    acid.portaMode = Porta::Auto; acid.portaTime = 2.5f;
    phrase(dir + "/acid.wav", acid, bass, 0.14, 0.6);

    Settings rubber;
    rubber.saw = 0; rubber.pulse = 10; rubber.sub = 10; rubber.range = Range::Feet16;
    rubber.cutoff = 3.5f; rubber.res = 3; rubber.vcfEnv = 5; rubber.vcfKybd = 5;
    rubber.decay = 3.5f; rubber.sustain = 0; rubber.release = 1.5f;
    phrase(dir + "/rubber-bass.wav", rubber, bass, 0.14, 0.6);

    Settings lead;
    lead.saw = 0; lead.pulse = 10; lead.pw = 6; lead.pwmSource = PwmSource::Lfo; lead.lfoRate = 4;
    lead.cutoff = 5.5f; lead.res = 4; lead.vcfEnv = 3; lead.sustain = 7; lead.release = 4;
    lead.portaMode = Porta::Auto; lead.portaTime = 3; lead.range = Range::Feet4;
    phrase(dir + "/pwm-lead.wav", lead, { 60, 63, 67, 70, 72, 70, 67, 63 }, 0.35, 0.95);

    Settings sweep = plain();
    sweep.saw = 0; sweep.res = 10; sweep.cutoff = 3; sweep.vcfEnv = 7; sweep.vcfKybd = 10;
    sweep.attack = 7; sweep.decay = 8; sweep.sustain = 0; sweep.vcaMode = VcaMode::Gate;
    phrase(dir + "/self-oscillation.wav", sweep, { 48 }, 4.0, 0.9);
}

} // namespace

int main(int argc, char** argv)
{
    std::printf("Mz101 engine check\n");

    pitchChecks();
    filterChecks();
    envelopeChecks();
    arpChecks();
    sequencerChecks();
    serviceManualChecks();
    hostClockChecks();
    fuzz();
    speed();

    if (argc > 2 && std::strcmp(argv[1], "--wav") == 0)
        demos(argv[2]);

    std::printf("\n%s\n", failures == 0 ? "all checks passed" : (std::to_string(failures) + " FAILED").c_str());
    return failures == 0 ? 0 : 1;
}
