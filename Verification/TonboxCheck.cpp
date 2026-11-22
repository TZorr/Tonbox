//
//  TonboxCheck.cpp
//  Tonbox
//
//  Measures the engine the plugin runs - the same object files - against its
//  specification, and writes every channel of the
//  default kit to a WAV to listen to.
//
//  - every default channel sounds, does not clip, stays finite and goes quiet
//    on its own, at 44.1, 48 and 96 kHz, with the same length at each rate;
//  - the measured constants: pitch, the -60 dB decay time, the output scale
//    and pan law, noise at the oscillator's RMS, Lin's two-thirds, the
//    distortion's square at +1.003 / -0.422, velocity's -25 dB at half;
//  - no NaN or infinity with every knob at its extremes;
//  - choke and retrigger: the choked channel is gone within 15 ms, and a
//    retrigger leaves no step;
//  - the three sends reach FMBox's effects: a reverb tail, a 1/8 echo at
//    120 BPM, a changed signal from the mod slot - and returns at -inf leave
//    the mix untouched bit for bit.
//
//  This is what keeps the engine on its specification from one build to the
//  next.
//
//  Usage:  TonboxCheck [wav-directory]
//

#include <juce_audio_formats/juce_audio_formats.h>

#include "Engine/SendEffects.h"
#include "Engine/ToneEngine.h"
#include "ParameterIds.h"

#include <cstdio>

namespace
{
    int failures = 0, checks = 0;

    void check (bool condition, const juce::String& what, const juce::String& detail = {})
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("FAIL: %s  %s\n", what.toRawUTF8(), detail.toRawUTF8());
        }
    }

    ToneEngine::ChannelSettings fromKit (int c)
    {
        const auto& k = TbParams::kit[c];
        ToneEngine::ChannelSettings s;
        s.wave = k.wave; s.pitchHz = k.pitchHz; s.oscAttackMs = k.oscAttackMs; s.oscDecayMs = k.oscDecayMs;
        s.modMode = k.modMode; s.modAmount = k.modAmountSt;
        s.modRate = k.modMode == ToneEngine::modSine ? Curves::modSineToX (k.modRate)
                  : (k.modMode == ToneEngine::modNoise ? Curves::modNoiseToX (k.modRate) : Curves::modDecayToX (k.modRate));
        s.filterMode = k.filterMode; s.filterHz = k.filterHz; s.filterQ = k.filterQ; s.stereo = k.stereo;
        s.envMode = k.envMode; s.noiseAttackMs = k.noiseAttackMs; s.noiseDecayMs = k.noiseDecayMs;
        s.mix = k.mix; s.distortion = k.drive; s.eqHz = k.eqHz; s.eqGainDb = k.eqGainDb;
        s.levelDb = k.levelDb; s.pan = k.pan; s.choke = k.choke;
        s.oscVelocity = k.oscVel * 0.01f; s.noiseVelocity = k.noiseVel * 0.01f; s.modVelocity = k.modVel * 0.01f;
        return s;
    }

    /** A plain sine: no modulation, no noise, centred, Level 0 dB. */
    ToneEngine::ChannelSettings plainSine (float hz = 1000.0f, float decayMs = 10000.0f)
    {
        ToneEngine::ChannelSettings s;
        s.pitchHz = hz;
        s.oscDecayMs = decayMs;
        return s;
    }

    struct Take
    {
        juce::AudioBuffer<float> audio;
        double rate = 48000.0;

        float peak (int ch = 0, int from = 0, int to = -1) const
        {
            to = to < 0 ? audio.getNumSamples() : to;
            return audio.getMagnitude (ch, from, to - from);
        }

        float rms (int ch, double fromMs, double toMs) const
        {
            const auto a = (int) (fromMs * rate / 1000.0), b = std::min (audio.getNumSamples(), (int) (toMs * rate / 1000.0));
            return audio.getRMSLevel (ch, a, b - a);
        }

        bool finite() const
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < audio.getNumSamples(); ++i)
                    if (! std::isfinite (audio.getSample (ch, i)))
                        return false;
            return true;
        }

        /** ms after the start at which the 1 ms RMS last exceeds peak RMS + dB. */
        double fallMs (float db) const
        {
            const auto w = (int) (rate / 1000.0);
            std::vector<float> env;
            for (int i = 0; i + w <= audio.getNumSamples(); i += w)
                env.push_back (audio.getRMSLevel (0, i, w));
            const auto top = *std::max_element (env.begin(), env.end());
            const auto limit = top * std::pow (10.0f, db / 20.0f);
            for (int i = (int) env.size() - 1; i >= 0; --i)
                if (env[(size_t) i] > limit)
                    return (double) i;
            return 0.0;
        }

        /** Frequency from rising zero crossings between two times. */
        double pitch (double fromMs, double toMs) const
        {
            const auto* x = audio.getReadPointer (0);
            const auto a = (int) (fromMs * rate / 1000.0), b = (int) (toMs * rate / 1000.0);
            double first = -1.0, last = -1.0;
            int count = 0;
            for (int i = a + 1; i < b; ++i)
                if (x[i - 1] <= 0.0f && x[i] > 0.0f)
                {
                    const auto c = (i - 1) + (double) (-x[i - 1] / (x[i] - x[i - 1]));
                    if (first < 0.0) first = c; else ++count;
                    last = c;
                }
            return count > 0 ? rate * count / (last - first) : 0.0;
        }
    };

    struct Hit { int channel; float velocity; double atMs; };

    Take render (double rate, double seconds, const std::vector<std::pair<int, ToneEngine::ChannelSettings>>& channels,
                 const std::vector<Hit>& hits)
    {
        ToneEngine engine;
        for (auto& [c, s] : channels)
            engine.setChannel (c, s);
        engine.prepare (rate, 512);

        Take take;
        take.rate = rate;
        const auto total = (int) (seconds * rate);
        take.audio.setSize (2, total);
        take.audio.clear();

        int position = 0;
        size_t next = 0;
        auto sorted = hits;
        std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.atMs < b.atMs; });

        while (position < total)
        {
            int end = total;
            if (next < sorted.size())
                end = std::min (end, (int) (sorted[next].atMs * rate / 1000.0));

            if (end > position)
            {
                engine.render (take.audio.getWritePointer (0, position), take.audio.getWritePointer (1, position), end - position);
                position = end;
            }

            while (next < sorted.size() && (int) (sorted[next].atMs * rate / 1000.0) <= position)
            {
                engine.trigger (sorted[next].channel, sorted[next].velocity);
                ++next;
            }
        }

        return take;
    }

    Take single (const ToneEngine::ChannelSettings& s, double seconds = 1.0, float velocity = 1.0f, double rate = 48000.0)
    {
        return render (rate, seconds, { { 0, s } }, { { 0, velocity, 0.0 } });
    }

    juce::String db (float gain) { return juce::String (juce::Decibels::gainToDecibels (gain, -200.0f), 2) + " dB"; }

    //==============================================================================
    void checkKit (const juce::File& wavDirectory)
    {
        for (int c = 0; c < TbParams::numChannels; ++c)
        {
            const auto name = "ch " + juce::String (c + 1) + " (" + TbParams::kit[c].role + ")";
            std::array<double, 3> fall {};

            for (int r = 0; r < 3; ++r)
            {
                const double rate = r == 0 ? 44100.0 : (r == 1 ? 48000.0 : 96000.0);
                ToneEngine engine;
                engine.setChannel (c, fromKit (c));
                engine.prepare (rate, 512);
                engine.trigger (c, 1.0f);

                Take take;
                take.rate = rate;
                take.audio.setSize (2, (int) (6.0 * rate));
                engine.render (take.audio.getWritePointer (0), take.audio.getWritePointer (1), take.audio.getNumSamples());

                const auto peak = std::max (take.peak (0), take.peak (1));
                const auto at = juce::String ((int) rate) + " Hz";
                check (peak > 0.03f, name + " sounds at " + at, db (peak));
                check (peak < 1.0f, name + " does not clip at " + at, db (peak));
                check (take.finite(), name + " stays finite at " + at);
                check (! engine.isSounding (c), name + " is over within 6 s at " + at);
                fall[(size_t) r] = take.fallMs (-40.0f);

                if (r == 1 && wavDirectory != juce::File())
                {
                    auto file = wavDirectory.getChildFile (juce::String (c + 1) + "-" + juce::String (TbParams::kit[c].role).replace (" ", "") + ".wav");
                    file.deleteFile();
                    juce::WavAudioFormat wav;
                    if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
                        if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (24)))
                            writer->writeFromAudioSampleBuffer (take.audio, 0, (int) (1.5 * rate));
                }
            }

            check (std::abs (fall[2] - fall[1]) <= 0.1 * fall[1] + 2.0, name + ": same length at 96 kHz",
                   juce::String (fall[1]) + " / " + juce::String (fall[2]) + " ms");
            check (std::abs (fall[0] - fall[1]) <= 0.1 * fall[1] + 2.0, name + ": same length at 44.1 kHz",
                   juce::String (fall[1]) + " / " + juce::String (fall[0]) + " ms");
        }
    }

    void checkSpecification()
    {
        // Pitch and the output scale.
        {
            const auto take = single (plainSine (440.0f), 0.5);
            check (std::abs (take.pitch (5, 400) - 440.0) < 0.5, "a 440 Hz sine is 440 Hz", juce::String (take.pitch (5, 400)));
            check (std::abs (take.peak (0) - 0.3058f) < 0.003f, "a centred sine at 0 dB peaks at 0.3058", juce::String (take.peak (0)));
        }

        // Decay: -60 dB at the time shown.
        for (const auto ms : { 100.0f, 316.23f, 1000.0f })
        {
            const auto take = single (plainSine (1000.0f, ms), ms * 0.0015 + 0.2);
            const auto fall = take.fallMs (-60.0f);
            check (std::abs (fall - ms) <= 0.03 * ms + 2.0, "Decay " + juce::String (ms) + " ms reaches -60 dB on time", juce::String (fall));
        }

        // Pan law: 1 - u^2.
        {
            auto s = plainSine();
            s.pan = -100.0f;
            const auto hard = single (s, 0.2);
            check (std::abs (hard.peak (0) - 0.4078f) < 0.004f && hard.peak (1) < 1.0e-6f, "hard left: 0.4078 left, nothing right",
                   juce::String (hard.peak (0)) + " / " + juce::String (hard.peak (1)));
            s.pan = 50.0f;
            const auto half = single (s, 0.2);
            check (std::abs (half.peak (1) / half.peak (0) - 0.9375f / 0.4375f) < 0.02f, "pan 50 R: 0.9375 / 0.4375");
        }

        // Noise at the oscillator's RMS.
        {
            auto s = plainSine (200.0f);
            const auto osc = single (s, 1.0).rms (0, 50, 900);
            s.mix = 100.0f;
            s.noiseDecayMs = 10000.0f;
            const auto noise = single (s, 1.0).rms (0, 50, 900);
            check (std::abs (juce::Decibels::gainToDecibels (noise / osc)) < 1.0f, "white noise is at the oscillator's RMS",
                   db (noise / osc));

            s.filterMode = ToneEngine::bandPass;
            s.filterHz = 1000.0f;
            s.filterQ = 30.0f;
            const auto narrow = single (s, 1.0).rms (0, 50, 900);
            check (std::abs (juce::Decibels::gainToDecibels (narrow / osc) - 1.7f) < 1.5f,
                   "a narrow band-pass keeps the level (plus the measured lift)", db (narrow / osc));
        }

        // Lin: the decay ends at two thirds of the time shown.
        {
            ToneEngine::ChannelSettings s;
            s.mix = 100.0f;
            s.envMode = ToneEngine::envLin;
            s.noiseDecayMs = 300.0f;
            const auto take = single (s, 0.5);
            const auto end = take.fallMs (-60.0f);
            check (std::abs (end - 200.0) < 8.0, "Lin decay ends at 2/3 of 300 ms", juce::String (end));
        }

        // Distortion at 100: a square at +1.003 / -0.422 (in oscillator units).
        {
            auto s = plainSine (200.0f);
            s.distortion = 100.0f;
            const auto take = single (s, 0.2);
            const auto* x = take.audio.getReadPointer (0);
            float top = -10.0f, bottom = 10.0f;
            for (int i = 480; i < 4800; ++i) { top = std::max (top, x[i]); bottom = std::min (bottom, x[i]); }
            check (std::abs (top / 0.3058f - 1.003f) < 0.02f && std::abs (bottom / 0.3058f + 0.422f) < 0.02f,
                   "Distortion 100 is the measured square", juce::String (top / 0.3058f) + " / " + juce::String (bottom / 0.3058f));
        }

        // Velocity: sensitivity 100 %, velocity 0.5 -> about -25 dB.
        {
            auto s = plainSine();
            s.oscVelocity = 1.0f;
            const auto full = single (s, 0.2, 1.0f).peak (0);
            const auto half = single (s, 0.2, 64.0f / 127.0f).peak (0);
            const auto drop = juce::Decibels::gainToDecibels (half / full);
            check (std::abs (drop + 24.6f) < 1.0f, "velocity 64 at 100 % sensitivity is -24.6 dB", db (half / full));
        }

        // Mod Decay: +24 st starts two octaves up.
        {
            auto s = plainSine (400.0f);
            s.modAmount = 24.0f;
            s.modRate = Curves::modDecayToX (300.0f);
            const auto take = single (s, 0.6);
            check (take.pitch (0.0, 2.0) > 1450.0, "Mod Decay +24 st starts near 1600 Hz", juce::String (take.pitch (0.0, 2.0)));
            check (std::abs (take.pitch (400.0, 590.0) - 400.0) < 2.0, "and lands on 400 Hz", juce::String (take.pitch (400.0, 590.0)));
        }
    }

    void checkExtremes()
    {
        // Every knob at both ends, a few combinations at a time.
        juce::Random random (7);

        for (int round = 0; round < 60; ++round)
        {
            ToneEngine::ChannelSettings s;
            const auto pick = [&] (float lo, float hi) { return random.nextBool() ? lo : hi; };
            s.wave = random.nextInt (3);
            s.pitchHz = pick (20.0f, 20000.0f);
            s.oscAttackMs = pick (0.0f, 10000.0f);
            s.oscDecayMs = pick (10.0f, 10000.0f);
            s.modMode = random.nextInt (3);
            s.modRate = pick (0.0f, 1.0f);
            s.modAmount = pick (-96.0f, 96.0f);
            s.filterMode = random.nextInt (3);
            s.filterHz = pick (20.0f, 20000.0f);
            s.filterQ = pick (0.1f, 10000.0f);
            s.stereo = random.nextBool();
            s.envMode = random.nextInt (3);
            s.noiseAttackMs = pick (0.0f, 10000.0f);
            s.noiseDecayMs = pick (10.0f, 10000.0f);
            s.mix = pick (0.0f, 100.0f);
            s.distortion = pick (0.0f, 100.0f);
            s.eqHz = pick (20.0f, 20000.0f);
            s.eqGainDb = pick (-40.0f, 40.0f);
            s.levelDb = pick (Curves::minLevelDb, Curves::maxLevelDb);
            s.pan = pick (-100.0f, 100.0f);
            s.oscVelocity = pick (0.0f, 2.0f);
            s.noiseVelocity = pick (0.0f, 2.0f);
            s.modVelocity = pick (0.0f, 2.0f);

            for (const auto rate : { 44100.0, 96000.0 })
            {
                const auto take = single (s, 0.4, random.nextFloat(), rate);
                check (take.finite(), "finite at extreme settings, round " + juce::String (round));
                check (take.peak (0) < 30.0f && take.peak (1) < 30.0f, "bounded at extreme settings, round " + juce::String (round),
                       juce::String (std::max (take.peak (0), take.peak (1))));
            }
        }
    }

    void checkChokeAndRetrigger()
    {
        auto open = plainSine (1000.0f);
        open.choke = true;
        auto closed = plainSine (250.0f, 50.0f);
        closed.choke = true;

        const auto take = render (48000.0, 0.4, { { 0, open }, { 1, closed } }, { { 0, 1.0f, 0.0 }, { 1, 1.0f, 200.0 } });

        // The 1 kHz part, after the choke, by correlation with a 1 kHz sine.
        const auto* x = take.audio.getReadPointer (0);
        const auto start = (int) (0.215 * 48000.0);
        double c = 0.0, s = 0.0;
        for (int i = start; i < start + 960; ++i)
        {
            c += x[i] * std::cos (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0);
            s += x[i] * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / 48000.0);
        }
        const auto left = 2.0 * std::hypot (c, s) / 960.0;
        check (left < 0.003, "a choked channel is gone 15 ms later", juce::String (left));

        // A retrigger: no step bigger than the waveform's own.
        const auto again = render (48000.0, 0.4, { { 0, plainSine (100.0f) } }, { { 0, 1.0f, 0.0 }, { 0, 1.0f, 201.3 } });
        const auto* y = again.audio.getReadPointer (0);
        float worst = 0.0f;
        for (int i = 1; i < again.audio.getNumSamples(); ++i)
            worst = std::max (worst, std::abs (y[i] - y[i - 1]));
        const auto sineStep = (float) (0.3058 * 2.0 * juce::MathConstants<double>::pi * 100.0 / 48000.0);
        check (worst < 0.3058f * 0.75f + sineStep, "a retrigger leaves no step", juce::String (worst));
    }
}

namespace
{
    /** One rim hit (channel 7 of the kit, sends cleared then set) through
        engine and effects, in 512-sample blocks as the plugin renders. */
    Take renderWithEffects (std::array<float, 3> sends, std::array<float, 3> returns, double seconds = 1.0)
    {
        constexpr double rate = 48000.0;
        constexpr int block = 512;
        auto s = fromKit (6);
        s.sends = sends;

        ToneEngine engine;
        engine.setChannel (6, s);
        engine.prepare (rate, block);

        SendEffects effects;
        for (int f = 0; f < TbParams::Fx::numSlots; ++f)
            effects.setSlot (f, { TbParams::Fx::defaultTypes[f], TbParams::Fx::defaultA[f], TbParams::Fx::defaultB[f], returns[(size_t) f] });
        effects.setTempo (120.0);
        effects.prepare (rate, block);

        engine.trigger (6, 1.0f);

        Take take;
        take.rate = rate;
        take.audio.setSize (2, (int) (seconds * rate));
        for (int i = 0; i < take.audio.getNumSamples(); i += block)
        {
            const auto n = std::min (block, take.audio.getNumSamples() - i);
            auto* l = take.audio.getWritePointer (0, i);
            auto* r = take.audio.getWritePointer (1, i);
            engine.render (l, r, n);
            effects.process (engine, l, r, n);
        }
        return take;
    }

    void checkSends()
    {
        const auto dry = renderWithEffects ({ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });

        // Sends into returns at -inf: the mix bit for bit.
        {
            const auto muted = renderWithEffects ({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f });
            bool same = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < dry.audio.getNumSamples(); ++i)
                    same = same && juce::exactlyEqual (dry.audio.getSample (ch, i), muted.audio.getSample (ch, i));
            check (same, "sends into returns at -inf leave the mix untouched");
        }

        const auto window = [] (const Take& t, double from, double to) { return t.peak (0, (int) (from * t.rate), (int) (to * t.rate)); };

        {
            const auto wet = renderWithEffects ({ 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
            check (window (wet, 0.2, 0.4) > 30.0f * window (dry, 0.2, 0.4) + 1.0e-4f, "the reverb send makes the rim ring on",
                   db (window (wet, 0.2, 0.4)));
        }

        {
            // 1/8 at 120 BPM is 250 ms after the hit.
            const auto wet = renderWithEffects ({ 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
            const auto echo = window (wet, 0.24, 0.29);
            check (echo > 0.1f * dry.peak (0) && window (dry, 0.24, 0.29) < 1.0e-3f, "the delay send echoes the rim after 1/8 at 120 BPM",
                   db (echo));
        }

        {
            const auto wet = renderWithEffects ({ 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f });
            float difference = 0.0f;
            for (int i = 0; i < wet.audio.getNumSamples(); ++i)
                difference = std::max (difference, std::abs (wet.audio.getSample (0, i) - dry.audio.getSample (0, i)));
            check (difference > 0.01f && wet.finite(), "the mod send changes the signal", juce::String (difference));
        }
    }
}

int main (int argc, char** argv)
{
    juce::File wavDirectory;
    if (argc > 1)
    {
        wavDirectory = juce::File (juce::String (argv[1]));
        wavDirectory.createDirectory();
    }

    std::printf ("Default kit at 44.1, 48 and 96 kHz...\n");
    checkKit (wavDirectory);
    std::printf ("Measured constants...\n");
    checkSpecification();
    std::printf ("Extremes...\n");
    checkExtremes();
    std::printf ("Choke and retrigger...\n");
    checkChokeAndRetrigger();
    std::printf ("Send effects...\n");
    checkSends();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
