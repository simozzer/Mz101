#pragma once

namespace sh101
{

namespace LfoWave   { enum { Triangle, Square, Random, Noise }; }
namespace Range     { enum { Feet16, Feet8, Feet4, Feet2 }; }
namespace PwmSource { enum { Lfo, Manual, Env }; }
namespace SubType   { enum { OctDown1, OctDown2, Pulse2 }; }
namespace VcaMode   { enum { Env, Gate }; }
namespace EnvTrig   { enum { GateTrig, Gate, Lfo }; }
namespace Porta     { enum { Off, Auto, On }; }
namespace Arp       { enum { Off, Up, UpDown, Down }; }
namespace Transpose { enum { Low, Mid, High }; }
namespace Seq       { enum { Off, Play, Load }; }
namespace Clock     { enum { Internal, Host }; }
namespace LfoSync   { enum { Free, Host }; }
namespace ClockDiv  { enum { Quarter, Eighth, EighthT, Sixteenth, SixteenthT, ThirtySecond }; }

//
// The front panel. Every slider is in panel units, 0 to 10, as printed beside it on the
// original; every switch is its position, counted from the bottom. The engine turns these
// into volts, hertz and seconds itself, so the plugin layer never has to know the curves.
//
struct Settings
{
    // MODULATOR
    float lfoRate   = 5.0f;
    int   lfoWave   = LfoWave::Triangle;

    // Not on the original: lock the modulator to the host's timeline, RATE then choosing a
    // note length (Sync.h) rather than hertz
    int   lfoSync   = LfoSync::Free;

    // What clocks the arpeggio and sequencer: the modulator, or - in place of the EXT CLK IN
    // jack - the host's beat grid, at this division
    int   clockSource = Clock::Internal;
    int   clockDiv    = ClockDiv::Sixteenth;

    // VCO
    float vcoMod    = 0.0f;
    int   range     = Range::Feet8;
    float pw        = 0.0f;
    int   pwmSource = PwmSource::Manual;

    // SOURCE MIXER
    float pulse     = 0.0f;
    float saw       = 10.0f;
    float sub       = 0.0f;
    int   subType   = SubType::OctDown1;
    float noise     = 0.0f;

    // VCF
    float cutoff    = 6.0f;
    float res       = 0.0f;
    float vcfEnv    = 4.0f;
    float vcfMod    = 0.0f;
    float vcfKybd   = 5.0f;

    // VCA
    int   vcaMode   = VcaMode::Env;

    // ENV
    float attack    = 0.0f;
    float decay     = 4.0f;
    float sustain   = 5.0f;
    float release   = 2.0f;
    int   envTrig   = EnvTrig::GateTrig;

    // PORTAMENTO
    int   portaMode = Porta::Off;
    float portaTime = 3.0f;

    // BENDER - how far the lever moves the VCO and the VCF, and how much LFO pushing it
    // forward adds to the VCO
    float bendVco   = 2.0f;
    float bendVcf   = 0.0f;
    float modDepth  = 3.0f;

    // ARPEGGIO / SEQUENCER / KEY TRANSPOSE. HOLD latches whichever of the two is running.
    int   arpMode   = Arp::Off;
    int   seqMode   = Seq::Off;
    bool  arpHold   = false;
    int   transpose = Transpose::Mid;

    // MASTER
    float tune      = 0.0f;     // cents, -50 to +50
    float volume    = 7.0f;
};

} // namespace sh101
