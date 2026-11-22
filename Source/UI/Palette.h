//
//  Palette.h
//  Tonbox
//
//  Every colour on the panel, and the fonts.
//
//  Since 0.8.0 the panel follows the "Tonbox - Kitbox-Look" mockup, Kitbox
//  0.10's "Variante B - Kompakt, Farbringe": a near-black body, one control
//  rack split by hairlines, light metal knobs whose ring carries the colour
//  of what they work on, and pads filled with their sound's colour. Flat
//  throughout: depth is said with greys and hairlines, not with light.
//
//  The mockup is the dark panel. The light one is Kitbox's, derived from it:
//  the same layout, accent and colours, the greys turned round. The choice
//  is kept per user, not per session (see PluginEditor.cpp).
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Palette
{
    // The colours below are the dark panel, the mockup's. They are not
    // const: setDark() swaps the whole set, and since every component reads
    // them while it paints, a repaint is all a switch needs.
    inline juce::Colour body        { 0xff111113 };   // the window
    inline juce::Colour header      { 0xff161619 };   // header bar
    inline juce::Colour footer      { 0xff151518 };   // status bar
    inline juce::Colour line        { 0xff25252a };   // header and card edges
    inline juce::Colour divider     { 0xff2a2a2f };   // the short uprights in the header

    inline juce::Colour card        { 0xff19191c };   // the display
    inline juce::Colour padWell     { 0xff0b0b0d };   // behind the pads

    inline juce::Colour button      { 0xff1f1f23 };
    inline juce::Colour buttonLine  { 0xff2e2e34 };
    inline juce::Colour buttonText  { 0xffd7d5d2 };
    inline juce::Colour icon        { 0xffc9c7c3 };

    inline juce::Colour field       { 0xff222226 };   // small buttons: Stereo, Choke, the steppers
    inline juce::Colour fieldLine   { 0xff33333a };
    inline juce::Colour tagLine     { 0xff36363d };   // the frame round the preset field's tag

    inline juce::Colour title       { 0xfff2f0ed };   // the brightest type
    inline juce::Colour ink         { 0xffeceae6 };   // values
    inline juce::Colour inkSoft     { 0xffa3a3aa };
    inline juce::Colour inkDim      { 0xff9a9aa1 };   // labels
    inline juce::Colour inkFaint    { 0xff8e8e95 };   // hints, meta

    inline juce::Colour accent      { 0xffff8a3d };
    inline juce::Colour signal      { 0xff4fc1a6 };   // meter, MIDI light
    inline juce::Colour warning     { 0xffe04a4f };
    inline juce::Colour meterTrack  { 0xff26262b };
    inline juce::Colour lightOff    { 0xff3a3a42 };   // the MIDI light, unlit

    inline juce::Colour wave        { 0xff121215 };   // the envelope's well, the sound field
    inline juce::Colour waveLine    { 0xff232328 };
    inline juce::Colour fieldEdge   { 0xff2a2a30 };   // the sound field's edge

    inline juce::Colour padRing     { 0xfff7f5f2 };   // inside the selected pad's orange ring
    inline juce::Colour padInk      { 0xff141416 };   // type on a pad
    inline juce::Colour onFill      { 0xff141416 };   // type on a lit button

    // The control rack: one panel, rows and sections split by hairlines.
    inline juce::Colour rackPanel   { 0xff1a1a1d };
    inline juce::Colour rackEdge    { 0xff26262b };
    inline juce::Colour rackFxRow   { 0xff141417 };   // the send effects' row, a step darker
    inline juce::Colour rackDivider { 0xff2a2a30 };

    inline juce::Colour knobName    { 0xffc4c4ca };   // the name under a knob
    inline juce::Colour segInk      { 0xffc4c4ca };   // an unlit segment's word

    // The metal knob: the same on both panels - it is a thing, not a surface.
    inline const juce::Colour capRim       { 0xff7e7e85 };
    inline const juce::Colour cap          { 0xffc6c6cb };
    inline const juce::Colour capHighlight { 0xffd4d4d8 };
    inline const juce::Colour capPointer   { 0xff18181b };
    inline const juce::Colour bigCapRim    { 0xff86868c };   // Master
    inline const juce::Colour bigCap       { 0xffc9c9ce };

    inline bool dark = true;

    /** Dark (the mockup) or light. Process-wide, so every open Tonbox window
        follows (each repaints when it notices). */
    inline void setDark (bool shouldBeDark)
    {
        dark = shouldBeDark;

        const auto set = [] (juce::Colour& c, juce::uint32 darkValue, juce::uint32 light)
        {
            c = juce::Colour (dark ? darkValue : light);
        };

        set (body,        0xff111113, 0xffe9e7e3);
        set (header,      0xff161619, 0xfff4f3f0);
        set (footer,      0xff151518, 0xfff1f0ed);
        set (line,        0xff25252a, 0xffd6d3cd);
        set (divider,     0xff2a2a2f, 0xffcfccc6);

        set (card,        0xff19191c, 0xfff7f6f4);
        set (padWell,     0xff0b0b0d, 0xffd5d2cc);

        set (button,      0xff1f1f23, 0xfffbfaf8);
        set (buttonLine,  0xff2e2e34, 0xffd0cdc7);
        set (buttonText,  0xffd7d5d2, 0xff34322f);
        set (icon,        0xffc9c7c3, 0xff4a4744);

        set (field,       0xff222226, 0xffffffff);
        set (fieldLine,   0xff33333a, 0xffcfccc6);
        set (tagLine,     0xff36363d, 0xffc9c5be);

        set (title,       0xfff2f0ed, 0xff161514);
        set (ink,         0xffeceae6, 0xff1f1e1c);
        set (inkSoft,     0xffa3a3aa, 0xff5f5c57);
        set (inkDim,      0xff9a9aa1, 0xff6e6b65);
        set (inkFaint,    0xff8e8e95, 0xff86837c);

        set (accent,      0xffff8a3d, 0xfff57a28);
        set (signal,      0xff4fc1a6, 0xff2fa486);
        set (warning,     0xffe04a4f, 0xffd63a40);
        set (meterTrack,  0xff26262b, 0xffdcd9d3);
        set (lightOff,    0xff3a3a42, 0xffb9b5ae);

        set (wave,        0xff121215, 0xfffdfcfa);
        set (waveLine,    0xff232328, 0xffdcd9d3);
        set (fieldEdge,   0xff2a2a30, 0xffd0cdc7);

        set (padRing,     0xfff7f5f2, 0xffffffff);
        set (padInk,      0xff141416, 0xff141416);
        set (onFill,      0xff141416, 0xffffffff);

        set (rackPanel,   0xff1a1a1d, 0xfff7f6f4);
        set (rackEdge,    0xff26262b, 0xffd6d3cd);
        set (rackFxRow,   0xff141417, 0xffefede9);
        set (rackDivider, 0xff2a2a30, 0xffdcd9d3);

        set (knobName,    0xffc4c4ca, 0xff4a4744);
        set (segInk,      0xffc4c4ca, 0xff4a4744);
    }

    //==============================================================================
    //  Colour zones
    //
    //  Each section says by colour what it works on: amber for the oscillator
    //  and its pitch, blue for the noise, red for what shapes the mix (drive,
    //  EQ), grey for the channel strip, and one colour per send effect - the
    //  same on its section and on its send knob, so a send and its return
    //  read as a pair.
    //
    //  A zone is one colour, desaturated to 80 %, and what is mixed from it:
    //  the ring's track (30 % into the bare track) and its title (30 %
    //  towards white - a third towards black on the light panel). The
    //  channel strip is a grey of its own. Computed while painting, so a theme
    //  switch needs no more than a repaint.

    enum class ZoneId { none, osc, noise, shape, neutral, reverb, delay, mod };

    struct Zone
    {
        juce::Colour line, label, track;
    };

    inline juce::Colour zoneBase (ZoneId id)
    {
        switch (id)
        {
            case ZoneId::osc:     return juce::Colour (0xffe9a23b);
            case ZoneId::noise:   return juce::Colour (0xff4c9eeb);
            case ZoneId::shape:   return juce::Colour (0xffe2574c);
            case ZoneId::reverb:  return juce::Colour (0xff8b7cf6);
            case ZoneId::delay:   return juce::Colour (0xff3fc4a8);
            case ZoneId::mod:     return juce::Colour (0xffe06ab0);
            case ZoneId::neutral: return dark ? juce::Colour (0xffc4c4cc) : juce::Colour (0xff7d7a74);
            case ZoneId::none:    break;
        }
        return accent;
    }

    inline Zone zone (ZoneId id)
    {
        const auto raw = zoneBase (id);
        const auto bareTrack = dark ? juce::Colour (0xff28282d) : juce::Colour (0xffdcd9d3);

        if (id == ZoneId::none)
            return { accent, inkSoft, bareTrack };

        if (id == ZoneId::neutral)
            return { raw,
                     dark ? juce::Colour (0xffc4c4ca) : raw.darker (0.5f),
                     dark ? juce::Colour (0xff36363c) : bareTrack.darker (0.06f) };

        const auto grey = 0.3f * raw.getFloatRed() + 0.59f * raw.getFloatGreen() + 0.11f * raw.getFloatBlue();
        const auto desaturate = [grey] (float v) { return grey + (v - grey) * 0.8f; };
        const auto colour = juce::Colour::fromFloatRGBA (desaturate (raw.getFloatRed()), desaturate (raw.getFloatGreen()),
                                                         desaturate (raw.getFloatBlue()), 1.0f);

        return { colour,
                 colour.interpolatedWith (dark ? juce::Colours::white : juce::Colours::black, dark ? 0.3f : 0.35f),
                 bareTrack.interpolatedWith (colour, 0.3f) };
    }

    /** The send effects' zones, by slot: reverb, delay, mod. */
    inline ZoneId fxZone (int slot)
    {
        static const ZoneId zones[] = { ZoneId::reverb, ZoneId::delay, ZoneId::mod };
        return zones[slot % 3];
    }

    //==============================================================================
    //  Pads

    /** One colour per sound, as the mockup has them: the kicks orange, clap
        and perc red, the hats yellow, tom and rim green. */
    inline juce::Colour soundColour (int sound)
    {
        static const juce::Colour orange (0xfff0913a), red (0xffde5a5c), yellow (0xffebcb55), green (0xff6db872);
        static const juce::Colour colours[] = { orange, orange, red, yellow, yellow, green, green, red };
        return colours[sound & 7];
    }

    //==============================================================================
    //  Type
    //
    //  The mockup sets Inter with system-ui behind it; the panel uses the
    //  system face, which is what a Mac without Inter shows the mockup in.
    //  600 and up is bold, the rest plain.

    inline juce::Font font (float height, bool bold = false, float tracking = 0.0f)
    {
        auto f = juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
        return tracking != 0.0f ? f.withExtraKerningFactor (tracking) : f;
    }

    /** Uppercase labels: 0.04 em apart, bold. */
    inline juce::Font label (float height = 11.0f)
    {
        return font (height, true, 0.04f);
    }
}
