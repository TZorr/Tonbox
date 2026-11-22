//
//  SoundLibrary.h
//  Tonbox
//
//  The single sounds the display's browser steps through: .tonboxsound files
//  in ~/Library/Audio/Presets/<company>/<plugin>/Sounds/, one folder per group
//  (BD, SD, HH, PERC, FX and any folder of the player's own). The kit menu does not list them: it shows
//  only folders that hold .tonbox kits.
//
//  Read from disk when asked - on a group change, a step, the list - never in
//  the display's timer.
//

#pragma once

#include <juce_core/juce_core.h>

namespace SoundLibrary
{
    /** <preset folder>/Sounds/, created if it is missing. */
    juce::File folder();

    /** The group folders: BD, SD, HH, PERC, FX first, then the rest by name. */
    juce::Array<juce::File> groups (const juce::File& root);

    /** A group's sounds, in natural order (BD 2 before BD 10). */
    juce::Array<juce::File> sounds (const juce::File& group);

    /** The sound `delta` places from `current` in the group, wrapping round.
        If `current` is not in it: the first (forwards) or last (backwards). */
    juce::File step (const juce::File& group, const juce::File& current, int delta);

    /** The submenu a sound is listed under in a large group: its name up to
        the first space, and of that up to the first hyphen - "KT BD Deep"
        is under KT, "XR-1 BD" and "XR-2 BD" both under XR. */
    juce::String listPrefix (const juce::String& soundName);

    /** The group folder a sound file sits in, if it is one of the root's. */
    juce::File groupOf (const juce::File& root, const juce::File& sound);
}
