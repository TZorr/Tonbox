//
//  SendEffects.cpp
//  Tonbox
//

#include "SendEffects.h"

SendEffects::SendEffects()
{
    for (int f = 0; f < TbParams::Fx::numSlots; ++f)
        slots[(size_t) f] = { TbParams::Fx::defaultTypes[f], TbParams::Fx::defaultA[f], TbParams::Fx::defaultB[f], 1.0f };

    prepare (48000.0, 512);
}

void SendEffects::prepare (double sampleRate, int maxBlockSize)
{
    maxBlock = juce::jmax (1, maxBlockSize);

    for (size_t f = 0; f < blocks.size(); ++f)
    {
        blocks[f].prepare (sampleRate, maxBlock);
        returnGain[f] = slots[f].gain;
    }

    fxLeft.assign ((size_t) maxBlock, 0.0f);
    fxRight.assign ((size_t) maxBlock, 0.0f);
}

void SendEffects::process (const ToneEngine& engine, float* left, float* right, int n)
{
    n = juce::jmin (n, maxBlock);

    // A send enters as a centred stereo signal - -3 dB per side, like a
    // centred sound - so an echo comes back at the sound's level.
    constexpr float centre = 0.70710678f;

    // The returns ramp with the master gain across the stretch, as the dry
    // mix did inside the engine.
    const auto masterEnd = engine.getMasterGain();
    const auto masterStep = (masterEnd - lastMaster) / (float) n;

    for (int f = 0; f < TbParams::Fx::numSlots; ++f)
    {
        const auto& p = slots[(size_t) f];

        EffectBlock::Settings settings;
        settings.category = TbParams::Fx::categories[f];
        settings.subtype  = EffectCatalog::clampSubtype (settings.category, p.type);
        settings.a        = p.a;
        settings.b        = p.b;
        settings.mix      = 1.0f;
        settings.sync     = f == TbParams::Fx::delay;   // the delay always follows the tempo, as in FMBox
        settings.bpm      = tempo;

        const auto* bus = engine.getSendBus (f);

        for (int i = 0; i < n; ++i)
            fxLeft[(size_t) i] = fxRight[(size_t) i] = bus[i] * centre;

        blocks[(size_t) f].process (fxLeft.data(), fxRight.data(), n, settings);

        const auto start = returnGain[(size_t) f];
        const auto step  = (p.gain - start) / (float) n;
        auto gain = start;
        auto master = lastMaster;

        for (int i = 0; i < n; ++i)
        {
            gain += step;
            master += masterStep;
            left[i]  += gain * master * fxLeft[(size_t) i];
            right[i] += gain * master * fxRight[(size_t) i];
        }

        returnGain[(size_t) f] = p.gain;
    }

    lastMaster = masterEnd;
}
