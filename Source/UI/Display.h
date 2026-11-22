//
//  Display.h
//  Tonbox (from FMBox)
//
//  The card under the header (the Kitbox-Look mockup). On the left the
//  selected pad - the channel, its sound's name, note and what it is. In the
//  middle the shape of its two envelopes, oscillator and noise, drawn from
//  their knobs. On the right the sound browser for that pad, over the kit's
//  name:
//
//      BD  SD  HH  PERC  FX                  the groups of the sound library
//      [<] [ AC BD BB      v ] [>]           step through the group; click the
//                                            name for the whole list
//      BD . 1 / 210            KIT  NAME     where in the group; the kit -
//                                            click to rename it
//
//  A group button only picks what to browse; the arrows and the list load the
//  sound onto the selected pad (the note stays). Nothing plays: the pad or a
//  key does that. The group shown follows the pad: a pad whose sound came
//  from the library shows that sound's group.
//
//  It follows the pad focus only, never the notes coming in: while a pattern
//  plays it stays on the sound being edited. The pads flash for the notes.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "RenameDialog.h"

class TonboxProcessor;

class Display : public juce::Component
{
public:
    explicit Display (TonboxProcessor& processorToUse);

    /** 30 Hz, from the editor: repaints only when something shown changed. */
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    /** The group being browsed, or the library itself if it has none: where
        Load Sound and Save Sound open. */
    juce::File getBrowseFolder() const;

    /** The library's groups, read again from disk. */
    void rescanLibrary();

    /** Browse group `index` of getGroups(). Loads nothing. */
    void selectGroup (int index);

    /** Loads the sound `delta` places on in the browsed group onto the
        selected pad - the first or last if the pad's is not in it. */
    void stepSound (int delta);

    /** Test hooks. */
    int getShownChannel() const noexcept                 { return shownSelected; }
    const juce::Array<juce::File>& getGroups() const     { return groups; }
    int getBrowseGroup() const noexcept                  { return browseGroup; }
    int getSoundIndex() const noexcept                   { return soundIndex; }
    int getGroupSize() const noexcept                    { return groupSounds.size(); }
    void setLibraryRoot (const juce::File& root);

private:
    void loadSound (const juce::File&);
    void showSoundList();

    /** Re-reads the browsed group's list if the group or the pad's sound file
        changed; true if anything shown did. */
    bool updateBrowser();

    TonboxProcessor& processor;
    juce::File libraryRoot;
    juce::Array<juce::File> groups, groupSounds;
    int browseGroup = 0, soundIndex = -1, listedGroup = -2;
    juce::File padSound, listedSound;

    /** The envelopes' outlines across the graph, in its own 0..1 units. */
    void paintEnvelopes (juce::Graphics&, juce::Rectangle<float> area);

    int shownLearn = -2, shownNote = -1, shownSelected = -1;
    juce::String shownKit, shownName, shownText;
    std::array<float, 6> shownShape {};   // what the envelopes are drawn from

    // Where things are drawn, for the clicks.
    juce::Array<juce::Rectangle<float>> groupAreas;
    juce::Rectangle<float> kitArea, prevArea, nextArea, nameArea;
    RenameDialog rename;
};
