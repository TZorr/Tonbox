//
//  ToneEngine.h
//  Tonbox
//
//  Eight drum channels, one stereo mix. Every channel is the same small
//  synthesiser:
//
//      oscillator (sine / triangle / saw, pitch modulated) -> envelope --+
//                                                                        +-> mix -> distortion -> EQ -> level -> pan
//      noise -> 2-pole filter (LP / BP / HP) -> envelope ----------------+
//
//  Each channel also feeds three mono send buses (reverb, delay, mod),
//  post-level and pre-pan as in FMBox; the effects themselves live outside
//  the engine (SendEffects), so the engine stays free of JUCE.
//
//  Each channel is monophonic: a new hit replaces the one still ringing,
//  which is faded over 2 ms rather than cut (two copies of the voice, as in
//  FMBox). Channels with Choke on silence each other over 10 ms.
//

#pragma once

#include "Curves.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class ToneEngine
{
public:
    static constexpr int numChannels = 8;
    static constexpr int numSends = 3;

    enum Wave { sine, triangle, saw };
    enum ModMode { modDecay, modSine, modNoise };
    enum FilterMode { lowPass, bandPass, highPass };
    enum EnvMode { envExp, envLin, envMod };

    /** One channel's knobs, in the units the panel shows - except Mod Rate
        and Drive, which stay knob positions: Mod Rate means ms or Hz depending
        on the mode, and Drive is mapped by Curves::driveGain. */
    struct ChannelSettings
    {
        int wave = sine;
        float pitchHz = 100.0f;
        float oscAttackMs = 0.0f, oscDecayMs = 300.0f;

        int modMode = modDecay;
        float modRate = 0.5f;          // 0..1
        float modAmount = 0.0f;        // semitones, +-96

        int filterMode = lowPass;
        float filterHz = 20000.0f, filterQ = 0.72f;
        bool stereo = false;

        int envMode = envExp;
        float noiseAttackMs = 0.0f, noiseDecayMs = 300.0f;

        float mix = 0.0f;              // 0 osc .. 100 noise
        float distortion = 0.0f;       // the Drive knob, 0..100
        float eqHz = 600.0f, eqGainDb = 0.0f;

        float levelDb = 0.0f;          // -60 (off) .. +10
        float pan = 0.0f;              // -100..100
        bool choke = false;

        float oscVelocity = 0.0f, noiseVelocity = 0.0f, modVelocity = 0.0f;   // 0..2 (0..200 %)

        std::array<float, numSends> sends {};   // 0..1: reverb, delay, mod

        // Exact on purpose: it only tells a moved knob from an unmoved one.
       #pragma clang diagnostic push
       #pragma clang diagnostic ignored "-Wfloat-equal"
        bool operator== (const ChannelSettings&) const = default;
       #pragma clang diagnostic pop
    };

    ToneEngine();

    void prepare (double sampleRate, int maxBlockSize);

    /** Once per block, before render. Knobs act on hits still ringing. */
    void setChannel (int channel, const ChannelSettings& settings);
    void setMaster (float db);   // -60 (off) .. +10

    /** Audio thread, between render calls: the hit starts on the next sample.
        Velocity 0..1. */
    void trigger (int channel, float velocity);

    /** Overwrites both channels, and the send buses with the same stretch.
        Any length up to the prepared block size; longer is split. */
    void render (float* left, float* right, int numSamples);

    /** The mono send bus a send effect reads after render(): numSamples
        valid samples, valid until the next render(). */
    const float* getSendBus (int send) const noexcept { return sendBuses[(size_t) send].data(); }

    int getHitCount (int channel) const noexcept       { return channels[(size_t) channel].hits.load(); }
    float getLastVelocity (int channel) const noexcept { return channels[(size_t) channel].velocity.load(); }
    int getLastChannel() const noexcept                { return lastChannel.load(); }

    /** The master gain as it stands after the last render - the send
        effects' returns follow it, so Master turns the whole mix down. */
    float getMasterGain() const noexcept               { return master; }
    bool isSounding (int channel) const noexcept;

    /** A full-scale oscillator at Level 0 dB leaves a centred channel at
        0.3058 per side, as measured; this is its hard-panned peak. */
    static constexpr float outputScale = 0.4078f;

private:
    /** Everything a voice needs that follows from the knobs and the sample
        rate - recomputed only when a knob moved. */
    /** A 2-pole state-variable filter's settings, with the spread of its
        resting state for unit white noise (a Cholesky factor). Double: at a
        Q of 10000 float rounding is not negligible. */
    struct SvfCoefficients
    {
        int mode = 0;
        double k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        double l11 = 0.0, l21 = 0.0, l22 = 0.0;
        double power = 1.0;   // output variance for unit white noise in

        void set (int filterMode, double hz, double q, double sampleRate);
    };

    struct Derived
    {
        ChannelSettings s;
        float pitchHz = 100.0f;
        float oscAttackStep = 0.0f, oscDecayStep = 1.0f;
        float modDecayStep = 1.0f, modRateHz = 0.0f;
        float noiseAttackStep = 0.0f, noiseDecayStep = 1.0f;     // multipliers (Exp) or increments (Lin)
        float burstSamples = 0.0f, burstStep = 1.0f;
        SvfCoefficients noiseFilter, modFilter;
        float noiseNorm = 1.0f, modNorm = 1.0f;   // Cholesky factor of the filter's resting state
        float oscMix = 1.0f, noiseMix = 0.0f;
        Curves::Distortion distortion;
        bool eqActive = false;
        float eqB0 = 1.0f, eqB1 = 0.0f, eqB2 = 0.0f, eqA1 = 0.0f, eqA2 = 0.0f;
    };

    struct Rng
    {
        std::uint32_t state = 0x9e3779b9u;

        float next()   // uniform -1..1
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (float) (std::int32_t) state * (1.0f / 2147483648.0f);
        }
    };

    struct Svf
    {
        double ic1 = 0.0, ic2 = 0.0;
        float process (float x, const SvfCoefficients& c);

        /** A state drawn from the filter's stationary distribution for
            white noise of the given variance, as if the noise had been
            running all along. */
        void settle (const SvfCoefficients& c, double variance, Rng& rng);
    };

    struct Voice
    {
        bool active = false;
        float fade = 1.0f, fadeStep = 0.0f;

        // Oscillator
        double phase = 0.125;
        float oscLevel = 0.0f;
        bool oscAttacking = false;
        float oscGain = 1.0f, noiseGain = 1.0f, modScale = 1.0f;

        // Pitch modulation
        float modEnv = 1.0f;
        double modPhase = 0.0;

        // Noise
        Rng rngL, rngR;
        Svf svfL, svfR, modSvf;
        Rng rngMod;
        int noiseStage = 0;        // 0 attack, 1 decay, 2 off
        float noiseLevel = 0.0f;
        float burstCount = 0.0f;   // samples into the current burst
        int burstsLeft = 0;

        void start (const Derived& d, float velocity, std::uint32_t seed, float rate);
        void process (const Derived& d, float rate, float& left, float& right);
    };

    struct Channel
    {
        std::array<Voice, 2> voices;
        int current = 0;
        Derived d;
        bool dirty = true;
        float gainL = 0.0f, gainR = 0.0f, targetL = 0.0f, targetR = 0.0f;
        std::array<float, numSends> send {}, sendTarget {};
        float eqL1 = 0.0f, eqL2 = 0.0f, eqR1 = 0.0f, eqR2 = 0.0f;
        std::atomic<int> hits { 0 };
        std::atomic<float> velocity { 0.0f };
    };

    void derive (Channel& channel);
    void fadeOut (Voice& voice, float seconds);
    void renderChunk (float* left, float* right, int n);
    void renderChannel (Channel& channel, float* left, float* right, int n);

    std::array<Channel, numChannels> channels;
    std::array<std::vector<float>, numSends> sendBuses;
    int maxBlock = 512;
    std::atomic<int> lastChannel { -1 };
    float rate = 48000.0f, glide = 0.0f;
    float master = 1.0f, masterTarget = 1.0f;
    std::uint32_t seedCounter = 1;
};
