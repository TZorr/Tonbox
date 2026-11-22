//
//  SendEffects.h
//  Tonbox
//
//  FMBox's three send effects - reverb, delay, mod - fed by the engine's send
//  buses and added to its mix:
//
//      send bus (mono) -> centred stereo (-3 dB a side) -> effect slot -> return level -> mix
//
//  The slots are FMBox's EffectBlocks (from Kitbox and Rackbox): each holds one
//  category of the catalog, Type picks the effect within it, A and B are its
//  two knobs. The delay always follows the host tempo. The returns follow the
//  engine's master gain, so Master turns the whole mix down.
//
//  Kept apart from ToneEngine so the engine stays free of JUCE and builds on
//  its own.
//

#pragma once

#include "Fx/EffectBlock.h"
#include "ToneEngine.h"
#include "ParameterIds.h"

#include <array>
#include <vector>

class SendEffects
{
public:
    struct SlotSettings
    {
        int type = 0;
        float a = 0.5f, b = 0.5f;
        float gain = 1.0f;   // return level, linear
    };

    SendEffects();

    /** Allocates: the effects build their delay lines here. */
    void prepare (double sampleRate, int maxBlockSize);

    void setSlot (int slot, const SlotSettings& settings) { slots[(size_t) slot] = settings; }
    void setTempo (double bpm) { tempo = bpm; }

    /** After engine.render() on the same stretch (at most the prepared block):
        runs the effects on the engine's send buses and adds them in. */
    void process (const ToneEngine& engine, float* left, float* right, int n);

private:
    std::array<EffectBlock, TbParams::Fx::numSlots> blocks;
    std::array<SlotSettings, TbParams::Fx::numSlots> slots;
    std::array<float, TbParams::Fx::numSlots> returnGain {};
    std::vector<float> fxLeft, fxRight;
    double tempo = 120.0;
    float lastMaster = 1.0f;
    int maxBlock = 512;
};
