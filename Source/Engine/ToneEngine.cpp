//
//  ToneEngine.cpp
//  Tonbox
//

#include "ToneEngine.h"
#include "Wavetables.h"

#include <cmath>

namespace
{
    constexpr double twoPi = 6.283185307179586;
    constexpr float silence = 1.0e-5f;   // -100 dB: an envelope below this is over

    inline double wrap (double t) { return t - std::floor (t); }

    /** The two-sample polynomial step correction. */
    inline float polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            const auto x = t / dt;
            return (float) (x + x - x * x - 1.0);
        }

        if (t > 1.0 - dt)
        {
            const auto x = (t - 1.0) / dt;
            return (float) (x * x + x + x + 1.0);
        }

        return 0.0f;
    }

    /** Sine, triangle and saw at equal RMS - the measured relation: the
        triangle and saw peak sqrt (3/2) above the sine. All three start an
        eighth of a cycle in, as measured.

        The saw is a ramp with a polyBLEP step up to 1 kHz - full-band and
        free of Gibbs overshoot - and a wavetable from 2 kHz, where
        polyBLEP's aliasing would be about 20 dB too high;
        in between the two are crossfaded. The triangle's harmonics fall so
        fast that the wavetable serves at every pitch. */
    inline float oscillator (int wave, double phase, double dt, double sampleRate)
    {
        constexpr float equalRms = 1.2247449f;

        switch (wave)
        {
            case ToneEngine::triangle:
                return equalRms * Wavetables::play (Wavetables::get (Wavetables::triangle), phase, dt);

            case ToneEngine::saw:
            {
                const auto hz = dt * sampleRate;
                const auto blend = std::clamp (std::log2 (hz / 1000.0), 0.0, 1.0);   // 0 below 1 kHz, 1 above 2 kHz
                float value = 0.0f;

                if (blend < 1.0)
                    value += (float) (1.0 - blend) * ((float) (1.0 - 2.0 * phase) + polyBlep (phase, dt));

                if (blend > 0.0)
                    value += (float) blend * Wavetables::play (Wavetables::get (Wavetables::saw), phase, dt);

                return equalRms * value;
            }

            default:
                return (float) std::sin (twoPi * phase);
        }
    }

    /** Multiplier per sample that falls (or rises) 60 dB in the given time. */
    inline float sixtyDbStep (float ms, float rate, bool rising)
    {
        if (! (ms > 0.0f) || std::isinf (ms))
            return 1.0f;

        const auto perSample = (float) (Curves::ln1000 / (ms * 0.001 * rate));
        return std::exp (rising ? perSample : -perSample);
    }

    /** The 2-pole filter's noise power gain: the output variance for unit
        white noise in, from the discrete Lyapunov equation of its state
        space. Exact for any setting, which a sum over the impulse response
        is not at a Q of 10000. */
    struct NoisePower
    {
        double power = 1.0;              // output variance
        double p11 = 0.0, p12 = 0.0, p22 = 0.0;   // state covariance
    };

    NoisePower svfNoisePower (int mode, double k, double a1, double a2, double a3)
    {
        // State update x' = A x + B u.
        const auto a = 2.0 * a1 - 1.0, b = -2.0 * a2, c = 2.0 * a2, d = 1.0 - 2.0 * a3;
        const auto b1 = 2.0 * a2, b2 = 2.0 * a3;

        // P = A P A^T + B B^T, three unknowns p11, p12, p22.
        double m[3][4] = {
            { 1.0 - a * a,   -2.0 * a * b,               -b * b,       b1 * b1 },
            { -a * c,         1.0 - a * d - b * c,       -b * d,       b1 * b2 },
            { -c * c,        -2.0 * c * d,                1.0 - d * d, b2 * b2 }
        };

        for (int col = 0; col < 3; ++col)
        {
            int pivot = col;
            for (int row = col + 1; row < 3; ++row)
                if (std::abs (m[row][col]) > std::abs (m[pivot][col]))
                    pivot = row;

            for (int k2 = 0; k2 < 4; ++k2)
                std::swap (m[col][k2], m[pivot][k2]);

            if (std::abs (m[col][col]) < 1.0e-300)
                return {};

            for (int row = 0; row < 3; ++row)
            {
                if (row == col)
                    continue;

                const auto f = m[row][col] / m[col][col];
                for (int k2 = col; k2 < 4; ++k2)
                    m[row][k2] -= f * m[col][k2];
            }
        }

        const auto p11 = m[0][3] / m[0][0], p12 = m[1][3] / m[1][1], p22 = m[2][3] / m[2][2];

        // y = C x + D u for each output.
        double c1, c2, dd;

        switch (mode)
        {
            case ToneEngine::bandPass: c1 = a1;           c2 = -a2;                 dd = a2;                break;
            case ToneEngine::highPass: c1 = -k * a1 - a2; c2 = k * a2 - (1.0 - a3); dd = 1.0 - k * a2 - a3; break;
            default:                   c1 = a2;           c2 = 1.0 - a3;            dd = a3;                break;
        }

        return { std::max (1.0e-30, c1 * c1 * p11 + 2.0 * c1 * c2 * p12 + c2 * c2 * p22 + dd * dd), p11, p12, p22 };
    }

    /** The filtered noise is not quite constant-power:
        wide, low-Q filters high up come out quieter (up to 6 dB at Q 0.1 and
        10 kHz), resonant ones louder (+0.8 dB per decade of Q above 0.32, at
        most +1.8 dB). Fitted to a 210-point grid, within about 0.5 dB. */
    double levelCorrection (int mode, double hz, double q)
    {
        struct Fit { double amount, corner, power, qPower; };
        static constexpr Fit fits[] = { { 5.0,  365726.0, 0.8, 2.0 },     // LP
                                        { 2.25,  23738.0, 1.5, 3.0 },     // BP
                                        { 2.25, 176373.0, 1.0, 3.0 } };   // HP
        const auto& fit = fits[std::clamp (mode, 0, 2)];
        const auto lossDb = -fit.amount * std::log10 (1.0 + std::pow (hz / fit.corner, fit.power) * std::pow (q, -fit.qPower));
        const auto liftDb = std::min (1.8, 0.8 * std::log10 (std::max (1.0, q / 0.32)));
        return std::pow (10.0, (lossDb + liftDb) / 20.0);
    }

    /** Standard normal, from two uniforms. */
    template <typename Rng>
    float gaussian (Rng& rng)
    {
        const auto u1 = std::max (1.0e-7f, 0.5f * (rng.next() + 1.0f));
        const auto u2 = 0.5f * (rng.next() + 1.0f);
        return std::sqrt (-2.0f * std::log (u1)) * std::cos (6.2831853f * u2);
    }
}

//==============================================================================
void ToneEngine::SvfCoefficients::set (int filterMode, double hz, double q, double sampleRate)
{
    mode = filterMode;
    const auto g = std::tan (3.14159265358979 * std::min (hz, 0.49 * sampleRate) / sampleRate);
    k = 1.0 / std::max (0.01, q);
    a1 = 1.0 / (1.0 + g * (g + k));
    a2 = g * a1;
    a3 = g * a2;

    const auto noise = svfNoisePower (mode, k, a1, a2, a3);
    power = noise.power;

    const auto p11 = std::max (0.0, noise.p11), p12 = noise.p12, p22 = std::max (0.0, noise.p22);
    l11 = std::sqrt (p11);
    l21 = l11 > 0.0 ? p12 / l11 : 0.0;
    l22 = std::sqrt (std::max (0.0, p22 - l21 * l21));
}

float ToneEngine::Svf::process (float x, const SvfCoefficients& c)
{
    const auto v3 = x - ic2;
    const auto v1 = c.a1 * ic1 + c.a2 * v3;
    const auto v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;

    switch (c.mode)
    {
        case bandPass: return (float) v1;
        case highPass: return (float) (x - c.k * v1 - v2);
        default:       return (float) v2;
    }
}

void ToneEngine::Svf::settle (const SvfCoefficients& c, double variance, Rng& rng)
{
    const auto spread = std::sqrt (variance);
    const double z1 = gaussian (rng), z2 = gaussian (rng);
    ic1 = spread * c.l11 * z1;
    ic2 = spread * (c.l21 * z1 + c.l22 * z2);
}

//==============================================================================
void ToneEngine::Voice::start (const Derived& d, float velocity, std::uint32_t seed, float sampleRate)
{
    const auto& s = d.s;
    active = true;
    fade = 1.0f;
    fadeStep = 0.0f;

    oscGain   = Curves::velocityGain (Curves::velocityAmount (velocity, s.oscVelocity));
    noiseGain = Curves::velocityGain (Curves::velocityAmount (velocity, s.noiseVelocity));
    modScale  = Curves::velocityModScale (Curves::velocityAmount (velocity, s.modVelocity));

    phase = 0.125;
    oscAttacking = s.oscAttackMs > 0.0f;
    oscLevel = oscAttacking ? 0.001f : 1.0f;

    rngL.state = seed * 2654435761u + 1u;
    rngR.state = seed * 40503u + 0x6d2b79f5u;
    rngMod.state = seed * 69069u + 0x2545f491u;
    for (int i = 0; i < 4; ++i) { rngL.next(); rngR.next(); rngMod.next(); }

    modEnv = 1.0f;
    modPhase = 0.0;

    // The filters start settled: a Q of 10000 would otherwise take seconds
    // to ring up, and the noise should be at full level at once.
    // Uniform noise in, so variance 1/3.
    svfL.settle (d.noiseFilter, 1.0 / 3.0, rngL);
    svfR.settle (d.noiseFilter, 1.0 / 3.0, rngR);
    modSvf.settle (d.modFilter, 1.0 / 3.0, rngMod);

    // The noise envelope. Mod is a clap: during the attack time the level
    // jumps back to full at every burst period and falls 24 dB in between;
    // the last burst runs into the ordinary decay.
    burstCount = 0.0f;
    burstsLeft = 0;

    if (s.noiseAttackMs <= 0.0f)
    {
        noiseStage = 1;
        noiseLevel = 1.0f;
    }
    else if (s.envMode == envMod)
    {
        noiseStage = 0;
        noiseLevel = 1.0f;
        // As many bursts as fit the attack, rounded: the last burst lands on
        // the attack's end even when the period runs a hair over.
        burstsLeft = (int) std::lround (s.noiseAttackMs / (d.burstSamples * 1000.0f / sampleRate));

        if (burstsLeft <= 0)
            noiseStage = 1;
    }
    else
    {
        noiseStage = 0;
        noiseLevel = s.envMode == envLin ? 0.0f : 0.001f;
    }
}

void ToneEngine::Voice::process (const Derived& d, float sampleRate, float& left, float& right)
{
    const auto& s = d.s;

    // Pitch modulation, in semitones.
    float mod = 0.0f;

    switch (s.modMode)
    {
        case modSine:
            mod = 0.5f * s.modAmount * (float) std::cos (twoPi * modPhase);
            modPhase = wrap (modPhase + d.modRateHz / sampleRate);
            break;

        case modNoise:
            // Low-passed noise, its spread a quarter of the amount.
            mod = s.modAmount * d.modNorm * modSvf.process (rngMod.next(), d.modFilter);
            break;

        default:
            mod = s.modAmount * modEnv;
            modEnv *= d.modDecayStep;
            break;
    }

    mod *= modScale;

    const auto hz = d.pitchHz * std::exp2 (mod * (1.0f / 12.0f));
    const auto dt = std::min (0.49, (double) hz / sampleRate);

    const auto osc = oscillator (s.wave, phase, dt, sampleRate) * oscLevel * oscGain * d.oscMix;
    phase = wrap (phase + dt);

    if (oscAttacking)
    {
        oscLevel *= d.oscAttackStep;

        if (oscLevel >= 1.0f)
        {
            oscLevel = 1.0f;
            oscAttacking = false;
        }
    }
    else
    {
        oscLevel *= d.oscDecayStep;
    }

    // Noise, filtered and brought back to the oscillator's RMS.
    float noiseL = 0.0f, noiseR = 0.0f;

    if (noiseStage < 2)
    {
        const auto gain = noiseLevel * noiseGain * d.noiseMix * d.noiseNorm;
        noiseL = svfL.process (rngL.next(), d.noiseFilter) * gain;
        noiseR = s.stereo ? svfR.process (rngR.next(), d.noiseFilter) * gain : noiseL;

        switch (noiseStage)
        {
            case 0:
                if (s.envMode == envMod)
                {
                    noiseLevel *= d.burstStep;

                    if ((burstCount += 1.0f) >= d.burstSamples)
                    {
                        burstCount -= d.burstSamples;
                        noiseLevel = 1.0f;

                        if (--burstsLeft <= 0)
                            noiseStage = 1;
                    }
                }
                else if (s.envMode == envLin)
                {
                    noiseLevel += d.noiseAttackStep;

                    if (noiseLevel >= 1.0f)
                    {
                        noiseLevel = 1.0f;
                        noiseStage = 1;
                    }
                }
                else
                {
                    noiseLevel *= d.noiseAttackStep;

                    if (noiseLevel >= 1.0f)
                    {
                        noiseLevel = 1.0f;
                        noiseStage = 1;
                    }
                }
                break;

            default:
                if (s.envMode == envLin)
                {
                    noiseLevel -= d.noiseDecayStep;

                    if (noiseLevel <= 0.0f)
                    {
                        noiseLevel = 0.0f;
                        noiseStage = 2;
                    }
                }
                else
                {
                    noiseLevel *= d.noiseDecayStep;

                    if (noiseLevel < silence)
                        noiseStage = 2;
                }
                break;
        }
    }

    left = osc + noiseL;
    right = osc + noiseR;

    if (! oscAttacking && oscLevel < silence && noiseStage == 2)
        active = false;
}

//==============================================================================
ToneEngine::ToneEngine()
{
    prepare (48000.0, 512);
}

void ToneEngine::prepare (double sampleRate, int maxBlockSize)
{
    rate = (float) sampleRate;
    maxBlock = std::max (1, maxBlockSize);

    for (auto& bus : sendBuses)
        bus.assign ((size_t) maxBlock, 0.0f);

    glide = 1.0f - std::exp (-1.0f / (0.005f * rate));
    master = masterTarget;

    for (auto& channel : channels)
    {
        channel.dirty = true;
        derive (channel);
        channel.gainL = channel.targetL;
        channel.gainR = channel.targetR;
        channel.send = channel.sendTarget;
        channel.eqL1 = channel.eqL2 = channel.eqR1 = channel.eqR2 = 0.0f;

        for (auto& voice : channel.voices)
            voice.active = false;
    }
}

void ToneEngine::setChannel (int index, const ChannelSettings& settings)
{
    auto& channel = channels[(size_t) index];

    if (! (channel.d.s == settings))
    {
        channel.d.s = settings;
        channel.dirty = true;
        derive (channel);
    }
}

void ToneEngine::setMaster (float db)
{
    masterTarget = Curves::levelGain (db);
}

void ToneEngine::derive (Channel& channel)
{
    if (! channel.dirty)
        return;

    channel.dirty = false;
    auto& d = channel.d;
    const auto& s = d.s;
    const auto nyquistSafe = 0.49f * rate;

    d.pitchHz = s.pitchHz;
    d.oscAttackStep = sixtyDbStep (s.oscAttackMs, rate, true);
    d.oscDecayStep  = sixtyDbStep (s.oscDecayMs, rate, false);

    d.modDecayStep = sixtyDbStep (Curves::modDecayMs (s.modRate), rate, false);
    d.modRateHz = s.modMode == modSine ? Curves::modSineHz (s.modRate)
                                       : (s.modMode == modNoise ? Curves::modNoiseHz (s.modRate) : 0.0f);

    // Lin runs its two segments in two thirds of the time shown, as measured.
    if (s.envMode == envLin)
    {
        constexpr float linScale = 2.0f / 3.0f;
        d.noiseAttackStep = s.noiseAttackMs > 0.0f ? 1.0f / (linScale * s.noiseAttackMs * 0.001f * rate) : 1.0f;
        d.noiseDecayStep  = 1.0f / std::max (1.0f, linScale * s.noiseDecayMs * 0.001f * rate);
    }
    else
    {
        d.noiseAttackStep = sixtyDbStep (s.noiseAttackMs, rate, true);
        d.noiseDecayStep  = sixtyDbStep (s.noiseDecayMs, rate, false);
    }

    // The clap's burst period grows with the attack time - 3.42 A^0.426 ms,
    // a least-squares fit to 21, 29, 39.5 and 54.7 ms measured at 70, 153,
    // 315 and 660 ms - but there are always at least three bursts.
    {
        const auto periodMs = std::max (0.5f, std::min (3.42f * std::pow (std::max (s.noiseAttackMs, 0.0f), 0.4261f), s.noiseAttackMs / 3.0f));
        d.burstSamples = periodMs * 0.001f * rate;
        d.burstStep = std::exp (std::log (0.0630957f) / d.burstSamples);   // -24 dB per period
    }

    // The noise filter, normalised to constant output power: narrowing it
    // makes it louder where it passes, not quieter overall.
    {
        const auto hz = std::min (s.filterHz, nyquistSafe);
        d.noiseFilter.set (s.filterMode, hz, s.filterQ, rate);

        // Uniform noise has RMS 1/sqrt 3; the oscillator, 1/sqrt 2.
        d.noiseNorm = (float) (std::sqrt (1.5 / d.noiseFilter.power) * levelCorrection (s.filterMode, hz, s.filterQ));
    }

    // Noise modulation: white noise through a 2-pole low-pass at 0.7 times
    // the rate shown, its spread a quarter of the amount - measured.
    if (s.modMode == modNoise)
    {
        d.modFilter.set (lowPass, std::max (0.01, 0.7 * (double) d.modRateHz), 0.7071, rate);
        d.modNorm = (float) (0.25 / std::sqrt (d.modFilter.power / 3.0));
    }

    d.oscMix = Curves::mixOscGain (s.mix);
    d.noiseMix = Curves::mixNoiseGain (s.mix);
    d.distortion.set (s.distortion);

    // Peaking EQ: a boost is broad (Q 0.5) and pulls the whole channel down
    // by half its gain; a cut is narrow (Q 2) and leaves the rest alone.
    d.eqActive = std::abs (s.eqGainDb) > 0.01f;

    if (d.eqActive)
    {
        const auto boost = s.eqGainDb > 0.0f;
        const auto A = std::pow (10.0, s.eqGainDb / 40.0);
        const auto q = boost ? 0.5 : 2.0;
        const auto w0 = twoPi * std::min (s.eqHz, nyquistSafe) / rate;
        const auto alpha = std::sin (w0) / (2.0 * q);
        const auto cosW = std::cos (w0);
        const auto a0 = 1.0 + alpha / A;
        const auto makeUp = boost ? 1.0 / A : 1.0;

        d.eqB0 = (float) ((1.0 + alpha * A) / a0 * makeUp);
        d.eqB1 = (float) (-2.0 * cosW / a0 * makeUp);
        d.eqB2 = (float) ((1.0 - alpha * A) / a0 * makeUp);
        d.eqA1 = (float) (-2.0 * cosW / a0);
        d.eqA2 = (float) ((1.0 - alpha / A) / a0);
    }

    // Level and pan: 1 - u^2 per side, 0.75 each at the centre.
    const auto gain = outputScale * Curves::levelGain (s.levelDb);
    const auto u = (std::clamp (s.pan, -100.0f, 100.0f) + 100.0f) / 200.0f;
    channel.targetL = gain * (1.0f - u * u);
    channel.targetR = gain * (1.0f - (1.0f - u) * (1.0f - u));

    // Sends are post-level, pre-pan: a channel turned down sends less, and
    // a centred one sends at its hard-panned level, as in FMBox.
    for (size_t f = 0; f < channel.sendTarget.size(); ++f)
        channel.sendTarget[f] = gain * std::clamp (s.sends[f], 0.0f, 1.0f);
}

void ToneEngine::fadeOut (Voice& voice, float seconds)
{
    if (voice.active && voice.fadeStep == 0.0f)
        voice.fadeStep = voice.fade / std::max (1.0f, seconds * rate);
}

void ToneEngine::trigger (int index, float velocity)
{
    if (index < 0 || index >= numChannels)
        return;

    auto& channel = channels[(size_t) index];
    derive (channel);

    fadeOut (channel.voices[(size_t) channel.current], 0.002f);
    channel.current ^= 1;
    channel.voices[(size_t) channel.current].start (channel.d, std::clamp (velocity, 0.0f, 1.0f), seedCounter++, rate);

    if (channel.d.s.choke)
        for (int other = 0; other < numChannels; ++other)
            if (other != index && channels[(size_t) other].d.s.choke)
                for (auto& voice : channels[(size_t) other].voices)
                    fadeOut (voice, 0.010f);

    channel.velocity.store (velocity);
    channel.hits.fetch_add (1);
    lastChannel.store (index);
}

bool ToneEngine::isSounding (int index) const noexcept
{
    const auto& voices = channels[(size_t) index].voices;
    return voices[0].active || voices[1].active;
}

void ToneEngine::render (float* left, float* right, int numSamples)
{
    // The send buses hold one block; a longer call keeps only its last
    // stretch there - callers that use the buses stay within maxBlock.
    while (numSamples > 0)
    {
        const auto n = std::min (numSamples, maxBlock);
        renderChunk (left, right, n);
        left += n;
        right += n;
        numSamples -= n;
    }
}

void ToneEngine::renderChunk (float* left, float* right, int numSamples)
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    for (auto& bus : sendBuses)
        std::fill (bus.begin(), bus.begin() + numSamples, 0.0f);

    for (auto& channel : channels)
        renderChannel (channel, left, right, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        master += glide * (masterTarget - master);
        left[i] *= master;
        right[i] *= master;
    }
}

void ToneEngine::renderChannel (Channel& channel, float* left, float* right, int n)
{
    const auto& d = channel.d;

    if (! channel.voices[0].active && ! channel.voices[1].active)
    {
        // Nothing playing: the gains still glide, the EQ forgets its tail.
        for (int i = 0; i < n; ++i)
        {
            channel.gainL += glide * (channel.targetL - channel.gainL);
            channel.gainR += glide * (channel.targetR - channel.gainR);

            for (size_t f = 0; f < channel.send.size(); ++f)
                channel.send[f] += glide * (channel.sendTarget[f] - channel.send[f]);
        }

        channel.eqL1 = channel.eqL2 = channel.eqR1 = channel.eqR2 = 0.0f;
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        channel.gainL += glide * (channel.targetL - channel.gainL);
        channel.gainR += glide * (channel.targetR - channel.gainR);

        for (size_t f = 0; f < channel.send.size(); ++f)
            channel.send[f] += glide * (channel.sendTarget[f] - channel.send[f]);

        float l = 0.0f, r = 0.0f;

        for (auto& voice : channel.voices)
        {
            if (! voice.active)
                continue;

            float vl, vr;
            voice.process (d, rate, vl, vr);
            l += vl * voice.fade;
            r += vr * voice.fade;

            if (voice.fadeStep > 0.0f && (voice.fade -= voice.fadeStep) <= 0.0f)
                voice.active = false;
        }

        l = d.distortion.process (l);
        r = d.distortion.process (r);

        if (d.eqActive)
        {
            // Transposed direct form II.
            const auto yl = d.eqB0 * l + channel.eqL1;
            channel.eqL1 = d.eqB1 * l - d.eqA1 * yl + channel.eqL2;
            channel.eqL2 = d.eqB2 * l - d.eqA2 * yl;
            l = yl;

            const auto yr = d.eqB0 * r + channel.eqR1;
            channel.eqR1 = d.eqB1 * r - d.eqA1 * yr + channel.eqR2;
            channel.eqR2 = d.eqB2 * r - d.eqA2 * yr;
            r = yr;
        }

        left[i]  += l * channel.gainL;
        right[i] += r * channel.gainR;

        // Mono into the sends: the mid of the two sides.
        const auto mono = 0.5f * (l + r);
        for (size_t f = 0; f < sendBuses.size(); ++f)
            sendBuses[f][(size_t) i] += mono * channel.send[f];
    }
}
