//
//  ValueText.h
//  Tonbox
//
//  How a value is printed - one place, so the host, the knobs and the display
//  all say "1.25 kHz" the same way.
//

#pragma once

#include <juce_core/juce_core.h>

#include "Engine/Curves.h"
#include "Engine/ToneEngine.h"

namespace ValueText
{
    inline juce::String hz (float v)
    {
        if (v >= 10000.0f) return juce::String (v / 1000.0f, 1) + " kHz";
        if (v >= 1000.0f)  return juce::String (v / 1000.0f, 2) + " kHz";
        if (v >= 100.0f)   return juce::String (juce::roundToInt (v)) + " Hz";
        if (v >= 1.0f)     return juce::String (v, 1) + " Hz";
        return juce::String (v, 2) + " Hz";
    }

    inline juce::String ms (float v)
    {
        if (std::isinf (v)) return "hold";
        if (v >= 1000.0f)   return juce::String (v / 1000.0f, 2) + " s";
        if (v >= 10.0f)     return juce::String (juce::roundToInt (v)) + " ms";
        return juce::String (v, 1) + " ms";
    }

    inline juce::String semitones (float v)
    {
        return (v > 0.0f ? "+" : "") + juce::String (v, std::abs (v) < 10.0f ? 2 : 1) + " st";
    }

    inline juce::String q (float v)
    {
        if (v >= 100.0f) return juce::String (juce::roundToInt (v));
        if (v >= 10.0f)  return juce::String (v, 1);
        return juce::String (v, 2);
    }

    inline juce::String db (float v)
    {
        return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    /** A Level or Master knob in dB; its bottom is off. */
    inline juce::String level (float db)
    {
        return db <= Curves::minLevelDb + 0.01f ? juce::String ("-inf") : ValueText::db (db);
    }

    inline juce::String pan (float v)
    {
        const auto amount = juce::roundToInt (std::abs (v));
        return amount == 0 ? juce::String ("C") : juce::String (amount) + (v < 0.0f ? " L" : " R");
    }

    /** Mod Rate means a time in Decay mode and a frequency otherwise. */
    inline juce::String modRate (float x, int mode)
    {
        switch (mode)
        {
            case ToneEngine::modSine:  return hz (Curves::modSineHz (x));
            case ToneEngine::modNoise: return hz (Curves::modNoiseHz (x));
            default:                   return ms (Curves::modDecayMs (x));
        }
    }
}
