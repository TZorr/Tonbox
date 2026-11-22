//
//  FlatToggle.h
//  Tonbox
//
//  The rack's small switch beside a section's buttons - STEREO, CHOKE: a
//  dark field with a hairline that turns, with its word, orange while on.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Palette.h"

class FlatToggle : public juce::ToggleButton
{
public:
    explicit FlatToggle (const juce::String& text) : juce::ToggleButton (text)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    /** The width the word needs, with the mockup's 9 points either side. */
    int getIdealWidth() const
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (font(), getButtonText().toUpperCase())) + 18 + 2;
    }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto lit = getToggleState();

        g.setColour (highlighted || down ? Palette::field.brighter (0.07f) : Palette::field);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (lit ? Palette::accent : Palette::fieldLine);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        g.setColour (lit ? Palette::accent : Palette::inkDim);
        g.setFont (font());
        g.drawText (getButtonText().toUpperCase(), bounds, juce::Justification::centred);
    }

private:
    static juce::Font font() { return Palette::font (10.5f, true, 0.06f); }
};
