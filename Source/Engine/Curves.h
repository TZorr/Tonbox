//
//  Curves.h
//  Tonbox
//
//  How a knob's 0..1 position becomes a frequency, a time, a level - and the
//  few nonlinear curves the sound itself is made of.
//
//  The knob scales are Tonbox's own. Frequencies, decay times and Q run
//  logarithmically, as on any synth; Attack, Mod Amount, Mod Rate, Level and
//  Drive have scales of their own design. The sound's curves further down -
//  the mix, velocity, the distortion's shape - are fitted curves.
//

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace Curves
{
    inline constexpr double ln1000 = 6.907755278982137;

    //==============================================================================
    //  Knob scales, x = 0..1

    /** Oscillator pitch, noise filter and EQ frequency: 20 Hz .. 20 kHz, logarithmic. */
    inline float frequency (float x)        { return 20.0f * std::pow (1000.0f, x); }
    inline float frequencyToX (float hz)    { return (float) (std::log (hz / 20.0) / ln1000); }

    /** Decay times: 10 ms .. 10 s (the time to -60 dB), logarithmic. */
    inline float decayMs (float x)          { return 10.0f * std::pow (1000.0f, x); }
    inline float decayToX (float ms)        { return (float) (std::log (ms / 10.0) / ln1000); }

    /** Attack times: 0 .. 10 s, exponential from zero - fine near nothing,
        about 110 ms at the middle. */
    inline constexpr float attackCurve = 9.0f;

    inline float attackMs (float x)
    {
        return x <= 0.0f ? 0.0f : 10000.0f * (std::exp (attackCurve * x) - 1.0f) / (std::exp (attackCurve) - 1.0f);
    }

    inline float attackToX (float ms)
    {
        return ms <= 0.0f ? 0.0f
                          : std::min (1.0f, std::log (1.0f + ms / 10000.0f * (std::exp (attackCurve) - 1.0f)) / attackCurve);
    }

    /** Noise filter Q: 0.1 .. 10000, logarithmic. */
    inline float filterQ (float x)          { return 0.1f * std::pow (100000.0f, x); }
    inline float filterQToX (float q)       { return (float) (std::log (q / 0.1) / std::log (100000.0)); }

    /** Pitch modulation depth: u = -1..1 becomes +-96 semitones along u^3,
        so the middle of the knob is the fine part. */
    inline float modSemitones (float u)     { return 96.0f * u * u * u; }
    inline float modSemitonesToU (float st) { return std::cbrt (std::clamp (st, -96.0f, 96.0f) / 96.0f); }

    /** Mod Rate means a different thing in each mode, all logarithmic:
        Decay  20 s .. 2 ms, with the knob's bottom holding the pitch offset;
        Sine   0.02 Hz .. 2 kHz;
        Noise  0.2 Hz .. 20 kHz. */
    inline float modDecayMs (float x)       { return x <= 0.0f ? INFINITY : 20000.0f * std::pow (10000.0f, -x); }
    inline float modSineHz (float x)        { return 0.02f * std::pow (100000.0f, x); }
    inline float modNoiseHz (float x)       { return 0.2f * std::pow (100000.0f, x); }

    inline float modDecayToX (float ms)     { return std::isinf (ms) ? 0.0f : std::clamp ((float) (-std::log10 (ms / 20000.0) / 4.0), 0.0f, 1.0f); }
    inline float modSineToX (float hz)      { return std::clamp ((float) (std::log10 (hz / 0.02) / 5.0), 0.0f, 1.0f); }
    inline float modNoiseToX (float hz)     { return std::clamp ((float) (std::log10 (hz / 0.2) / 5.0), 0.0f, 1.0f); }

    /** Level and Master in dB: -60 is off, +10 the top. */
    inline constexpr float minLevelDb = -60.0f, maxLevelDb = 10.0f;

    inline float levelGain (float db)       { return db <= minLevelDb + 0.01f ? 0.0f : std::pow (10.0f, db * 0.05f); }

    /** Drive, 0..100: exponential, off at 0, a drive of 1 at 30, 250 at 100. */
    inline constexpr float driveScale = 0.1088f, driveCurve = 7.74f;

    inline float driveGain (float amount)
    {
        return amount <= 0.0f ? 0.0f : driveScale * (std::exp (driveCurve * std::min (amount, 100.0f) / 100.0f) - 1.0f);
    }

    inline float driveToAmount (float gain)
    {
        return gain <= 0.0f ? 0.0f : std::min (100.0f, 100.0f / driveCurve * std::log (1.0f + gain / driveScale));
    }

    //==============================================================================
    //  The sound's own curves

    /** Osc / Noise mix, 0..100: each side stays at full level up to the
        middle and fades towards its end along (e^4u - 1) / (e^4 - 1). */
    inline float mixFade (float u)
    {
        static const float denominator = std::exp (4.0f) - 1.0f;
        return u <= 0.0f ? 0.0f : (u >= 1.0f ? 1.0f : (std::exp (4.0f * u) - 1.0f) / denominator);
    }

    inline float mixOscGain (float mix)     { return mix <= 50.0f ? 1.0f : mixFade ((100.0f - mix) / 50.0f); }
    inline float mixNoiseGain (float mix)   { return mix >= 50.0f ? 1.0f : mixFade (mix / 50.0f); }

    /** Velocity: e = 1 - sensitivity (1 - velocity), sensitivity 0..2. */
    inline float velocityAmount (float velocity, float sensitivity)
    {
        return std::max (0.0f, 1.0f - sensitivity * (1.0f - velocity));
    }

    /** What e does to a level: -25 dB at 0.5, -40 dB at 0.25. */
    inline float velocityGain (float e)     { return e <= 0.0f ? 0.0f : e * std::exp (4.33f * (e - 1.0f)); }

    /** What e does to the modulation depth. */
    inline float velocityModScale (float e) { return e * e; }

    //==============================================================================
    //  Distortion (the Drive knob, 0..100) - static, asymmetric, keeps its DC.
    //
    //  The shape is two cubic soft clips stacked: the first takes the signal
    //  up to half way, the second, sitting one unit higher, the rest. So the
    //  negative side flattens at -1 and the positive at +2.375. The knob sets
    //  the drive (driveGain); below a drive of 1 the shaped signal is faded in
    //  instead, so the knob leaves zero without a step.
    struct Distortion
    {
        float wet = 0.0f, dry = 1.0f, inputGain = 1.0f;
        bool active = false;

        static float cubic (float v)
        {
            v = std::clamp (v, -1.0f, 1.0f);
            return 1.5f * v - 0.5f * v * v * v;
        }

        static float shape (float u)
        {
            return u <= 0.5f ? cubic (u) : 1.375f + cubic (u - 1.0f);
        }

        void set (float amount)
        {
            const auto g = driveGain (amount);
            active = g > 0.0f;

            if (g < 1.0f)
            {
                dry = 1.0f - g;
                wet = g * 0.7225f;
                inputGain = 1.16f;
            }
            else
            {
                dry = 0.0f;
                wet = 1.0f / (2.375f - 1.06f / g);
                inputGain = g;
            }
        }

        float process (float x) const
        {
            return active ? dry * x + wet * shape (inputGain * x) : x;
        }
    };
}
