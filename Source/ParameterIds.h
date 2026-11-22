//
//  ParameterIds.h
//  Tonbox
//
//  Every parameter the host can see.
//
//  Eight identical channels, the same suffixes eight times over with the
//  channel number in front: "c3_pitch". The ids are the identity a saved
//  session is restored by, so once a session exists none of these strings may
//  change - rename the display name instead.
//
//  The knobs' 0..1 positions follow Tonbox's own scales in Engine/Curves.h.
//  0.4.0 gave four knobs new scales and, with them, new ids (vol, rate, drive,
//  out), so a value saved on an older scale is never read on the new one.
//

#pragma once

#include <juce_core/juce_core.h>

#include "Engine/Curves.h"

namespace TbParams
{
    constexpr int stateVersion = 1;
    constexpr int numChannels = 8;

    //==============================================================================
    namespace Ch
    {
        inline constexpr const char* wave        = "wave";     // Sine, Triangle, Saw
        inline constexpr const char* pitch       = "pitch";    // Hz
        inline constexpr const char* oscAttack   = "oatk";     // ms
        inline constexpr const char* oscDecay    = "odec";     // ms
        inline constexpr const char* modMode     = "mmode";    // Decay, Sine, Noise
        inline constexpr const char* modRate     = "rate";     // 0..1, ms or Hz by mode (Curves)
        inline constexpr const char* modAmount   = "mamt";     // semitones
        inline constexpr const char* filterMode  = "fmode";    // LP, BP, HP
        inline constexpr const char* filterFreq  = "ffreq";    // Hz
        inline constexpr const char* filterQ     = "fq";
        inline constexpr const char* stereo      = "stereo";
        inline constexpr const char* envMode     = "nmode";    // Exp, Lin, Mod
        inline constexpr const char* noiseAttack = "natk";     // ms
        inline constexpr const char* noiseDecay  = "ndec";     // ms
        inline constexpr const char* mix         = "mix";      // 0 osc .. 100 noise
        inline constexpr const char* distortion  = "drive";    // 0..100, Curves::driveGain
        inline constexpr const char* eqFreq      = "eqf";      // Hz
        inline constexpr const char* eqGain      = "eqg";      // dB
        inline constexpr const char* level       = "vol";      // dB, -60 (off) .. +10
        inline constexpr const char* pan         = "pan";      // -100..100
        inline constexpr const char* choke       = "choke";
        inline constexpr const char* oscVel      = "ovel";     // percent
        inline constexpr const char* noiseVel    = "nvel";
        inline constexpr const char* modVel      = "mvel";
        inline constexpr const char* note        = "note";     // the MIDI note that plays it

        //  Added in 0.2.0 (version hint 2): the sends into FMBox's three
        //  effects, 0..1, post-level and pre-pan. Not in all[], which is the
        //  synth's own parameters in the order the processor indexes them.
        inline constexpr const char* sends[] = { "rev", "dly", "mod" };

        /** Every parameter of a channel, in a fixed order the processor indexes. */
        inline constexpr const char* all[] = { wave, pitch, oscAttack, oscDecay, modMode, modRate, modAmount,
                                               filterMode, filterFreq, filterQ, stereo, envMode, noiseAttack,
                                               noiseDecay, mix, distortion, eqFreq, eqGain, level, pan, choke,
                                               oscVel, noiseVel, modVel, note };
        constexpr int count = 25;

        enum Index { iWave, iPitch, iOscAttack, iOscDecay, iModMode, iModRate, iModAmount,
                     iFilterMode, iFilterFreq, iFilterQ, iStereo, iEnvMode, iNoiseAttack,
                     iNoiseDecay, iMix, iDistortion, iEqFreq, iEqGain, iLevel, iPan, iChoke,
                     iOscVel, iNoiseVel, iModVel, iNote };
    }

    inline const juce::StringArray waveNames    { "Sine", "Tri", "Saw" };
    inline const juce::StringArray modModeNames { "Decay", "Sine", "Noise" };
    inline const juce::StringArray filterNames  { "LP", "BP", "HP" };
    inline const juce::StringArray envModeNames { "Exp", "Lin", "Mod" };

    /** "c3_pitch" for channel index 2. */
    inline juce::String channelId (int channel, const char* suffix)
    {
        return "c" + juce::String (channel + 1) + "_" + suffix;
    }

    inline constexpr const char* master = "out";   // dB, -60 (off) .. +10

    //==============================================================================
    //  Names (0.3.0): not parameters - nothing to automate - but properties of
    //  the state tree, so a session or a .tonbox preset carries them like the knobs.
    inline constexpr const char* kitNameId = "kitName";

    /** "name3" for channel index 2. */
    inline juce::Identifier channelNameId (int channel)
    {
        return juce::Identifier ("name" + juce::String (channel + 1));
    }

    inline constexpr const char* defaultKitName = "Tonbox Default";

    /** "sound3" for channel index 2 (0.7.0): the full path of the sound file
        the channel's sound was last loaded from or saved to, so the panel's
        sound browser can step on from there. Empty: none. */
    inline juce::Identifier channelSoundId (int channel)
    {
        return juce::Identifier ("sound" + juce::String (channel + 1));
    }

    /** A single sound on disk: the XML a copied pad puts on the clipboard. */
    inline constexpr const char* soundExtension = ".tonboxsound";

    //==============================================================================
    //  The three send effects, as in FMBox (and Kitbox). Each slot holds one
    //  category of Rackbox's catalog and Type picks the effect within it; A and
    //  B are that effect's two knobs, 0..1, named and printed by EffectCatalog.
    namespace Fx
    {
        constexpr int numSlots = 3;
        enum Slot { reverb, delay, mod };

        struct Ids { const char* type; const char* a; const char* b; const char* level; };

        inline constexpr Ids ids[numSlots] {
            { "rev_type", "rev_a", "rev_b", "rev_level" },
            { "dly_type", "dly_a", "dly_b", "dly_level" },
            { "mod_type", "mod_a", "mod_b", "mod_level" }
        };

        inline constexpr const char* titles[numSlots] { "Reverb", "Delay", "Mod" };

        /** Where each slot sits in EffectCatalog (Modulation 0, Reverb 1, Delay 2). */
        inline constexpr int categories[numSlots] { 1, 2, 0 };

        /** FMBox's defaults: a plate, a tape delay at 1/8 and a phaser. */
        inline constexpr int   defaultTypes[numSlots] { 0, 1, 1 };
        inline constexpr float defaultA[numSlots]     { 0.5f, 5.0f / 13.0f, 0.392f };
        inline constexpr float defaultB[numSlots]     { 0.4f, 0.37f, 0.7f };

        /** Return levels in dB, -60 is off. */
        constexpr float minLevelDb = -60.0f, maxLevelDb = 6.0f;

        inline float levelToGain (float db)
        {
            return db <= minLevelDb + 0.01f ? 0.0f : std::pow (10.0f, db * 0.05f);
        }
    }

    /** "C1" for 36 - Logic's convention, middle C = C3. */
    inline juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    //==============================================================================
    //  The kit Tonbox opens with - its own, built on these eight channels. The
    //  notes are C1..G1, one per channel.
    struct ChannelDefaults
    {
        const char* role;
        int wave; float pitchHz, oscAttackMs, oscDecayMs;
        int modMode; float modRate, modAmountSt;   // Mod Rate in ms (Decay) or Hz (Sine, Noise)
        int filterMode; float filterHz, filterQ; bool stereo;
        int envMode; float noiseAttackMs, noiseDecayMs;
        float mix, drive, eqHz, eqGainDb, levelDb, pan;
        bool choke;
        float oscVel, noiseVel, modVel;   // percent
        int note;
        float reverb = 0.0f, delay = 0.0f, mod = 0.0f;   // sends, 0..1
    };

    // Mod Rate is a time in Decay mode (ms) and a frequency otherwise (Hz).
    inline constexpr ChannelDefaults kit[numChannels] = {
        //  role      wave pitch  atk   dec    mod rate    amt    flt  freq    Q     st     env natk  ndec   mix  drive eqHz   eqDb  lvl   pan  choke  ovel nvel mvel note  rev  dly
        { "Kick",     0,   52.0f, 0.0f, 520.0f, 0, 39.9f, 28.0f, 0, 3000.0f, 0.72f, false, 0, 0.0f, 25.0f,  12.0f, 34.2f,  90.0f, 3.0f,  0.0f,   0.0f, false, 40.0f, 40.0f, 30.0f, 36 },
        { "Snare",    1,  185.0f, 0.0f, 180.0f, 0, 31.9f,  9.0f, 1, 2200.0f, 0.9f,  true,  0, 0.0f, 230.0f, 62.0f, 20.5f, 4500.0f, 3.0f, -1.0f,   0.0f, false, 50.0f, 50.0f,  0.0f, 37, 0.15f },
        { "Clap",     0,  900.0f, 0.0f,  60.0f, 0, 353.0f,  0.0f, 1, 1250.0f, 1.6f,  true,  2, 28.0f, 260.0f, 100.0f, 0.0f, 1000.0f, 0.0f, -2.0f,   0.0f, false,  0.0f, 50.0f,  0.0f, 38, 0.20f },
        { "Closed Hat", 1, 5400.0f, 0.0f, 45.0f, 2, 7166.0f,  3.0f, 2, 7600.0f, 1.2f,  true,  0, 0.0f, 55.0f,  80.0f,  0.0f, 1000.0f, 0.0f, -4.0f,  15.0f, true,  30.0f, 30.0f,  0.0f, 39 },
        { "Open Hat", 1, 5400.0f, 0.0f, 300.0f, 2, 7166.0f,  3.0f, 2, 7600.0f, 1.2f,  true,  0, 0.0f, 420.0f, 80.0f,  0.0f, 1000.0f, 0.0f, -5.0f,  15.0f, true,  30.0f, 30.0f,  0.0f, 40 },
        { "Tom",      0,  120.0f, 0.0f, 380.0f, 0, 124.4f, 7.0f, 0, 1500.0f, 0.72f, false, 0, 0.0f, 40.0f,   8.0f,  0.0f, 1000.0f, 0.0f, -2.0f, -20.0f, false, 40.0f, 40.0f,  0.0f, 41, 0.10f },
        { "Rim",      1, 1650.0f, 0.0f,  35.0f, 0, 353.0f,  0.0f, 1, 3500.0f, 3.0f,  false, 0, 0.0f, 18.0f,  45.0f, 59.1f, 2500.0f, 4.0f, -6.0f, -10.0f, false, 50.0f, 50.0f,  0.0f, 42, 0.0f, 0.20f },
        { "Perc",     1,  380.0f, 0.0f, 140.0f, 0, 17.0f, 12.0f, 1, 5000.0f, 2.0f,  false, 0, 0.0f, 30.0f,  25.0f, 46.3f, 1000.0f, 0.0f, -4.0f,  10.0f, false, 40.0f, 40.0f,  0.0f, 43 },
    };
}
