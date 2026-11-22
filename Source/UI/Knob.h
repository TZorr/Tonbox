//
//  Knob.h
//  Tonbox
//
//  A rotary knob with its name and its value printed with it - Kitbox 0.10's
//  knob: a light metal cap inside a ring in the colour of the knob's zone, a
//  dot where the value starts from, a dark pointer on the cap.
//
//  Two layouts: stacked - the value over a 40-point knob and the name under
//  it, as in the control rack - and beside, a 38-point knob with name and
//  value to its right, as Master in the header.
//
//  A stacked knob lights its value while the mouse is over it or turns it,
//  and for a second after. A change the host makes - automation - leaves it
//  dim, so a playing automation lane does not set every value flickering.
//
//  Bipolar knobs - pan, EQ gain, pitch mod amount - draw their arc from
//  twelve o'clock, so "nothing" looks like nothing. Double-click returns a
//  knob to its default.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Palette.h"

class Knob : public juce::Slider,
             private juce::Timer
{
public:
    enum class Layout { stacked, beside };

    Knob (const juce::String& labelText, bool isBipolar = false, Layout layoutToUse = Layout::stacked);
    ~Knob() override;

    /** Binds to a parameter, replacing any previous binding. */
    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId);

    /** The name printed with the knob; an effect's A and B are renamed by the effect loaded. */
    void setLabel (const juce::String& newLabel);

    /** The colour zone the knob sits in: its ring takes the zone's colours. */
    void setZone (Palette::ZoneId zoneToUse);

    void paint (juce::Graphics&) override;
    void valueChanged() override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Whether a stacked knob lights its value. */
    bool isShowingValue() const { return isMouseOverOrDragging() || isTimerRunning(); }

    /** Height of a stacked knob: value 13, 1, dial 40, 1, name 13. */
    static constexpr int stackedHeight = 68;
    static constexpr int dialSize = 40;

    /** How long the value stays after the mouse has left or let go. */
    static constexpr int valueHoldMs = 1000;

    /** Draws a dial of the mockup's geometry - 52 units, ring radius 22 -
        scaled into `area`. `big` is the header's: a heavier cap without the
        highlight and without the origin dot. */
    static void drawDial (juce::Graphics&, juce::Rectangle<float> area, float proportion, bool bipolar,
                          bool hover, juce::Colour value, juce::Colour track, bool big = false);

private:
    void timerCallback() override;
    void holdValue();

    juce::String label;
    bool bipolar;
    Layout layout;
    Palette::ZoneId zone = Palette::ZoneId::none;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};
