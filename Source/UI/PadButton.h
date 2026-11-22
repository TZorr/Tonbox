//
//  PadButton.h
//  Tonbox (from FMBox, drag and drop from Kitbox)
//
//  One pad per channel, in a row at the bottom: the channel's number and the
//  name of its sound, its note and a short description of what it is set to
//  - "SINE 52 Hz", "BP NOISE 1.25 kHz" - and a flash when it plays.
//
//  Click to select the channel - the panel above edits it, Learn assigns its
//  note - and to audition it, louder the higher up it is hit. Drag a pad onto
//  another to swap the two sounds; the notes stay where they are. Right-click
//  to copy the sound, paste a copied one, or rename it.
//
//  Since 0.8.0 (the Kitbox-Look mockup) a pad is filled with its sound's
//  colour, its number and name on a lighter bar along the top; the selected
//  pad has a white ring inside an orange one. The orange ring lies outside
//  the pad, so the pad's bounds are its face plus ringMargin all round.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "RenameDialog.h"

class TonboxProcessor;

class PadButton : public juce::Component,
                  public juce::DragAndDropTarget
{
public:
    PadButton (TonboxProcessor& processorToUse, int channelIndex);

    /** 30 Hz, from the editor: picks up hits, the selection, the note, name
        and description, and fades the flash. */
    void updateFlash();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override  { setDropHover (true); }
    void itemDragExit (const SourceDetails&) override   { setDropHover (false); }
    void itemDropped (const SourceDetails&) override;

    /** "SINE 52 Hz" or "BP NOISE 1.25 kHz": whichever side of the mix leads. */
    static juce::String describe (TonboxProcessor&, int channel);

    /** The same, short enough for the pad's bottom line: "BP 2.20 kHz". */
    static juce::String describeShort (TonboxProcessor&, int channel);

    /** The channel's name, or its description if it has none. */
    static juce::String title (TonboxProcessor&, int channel);

    /** The orange ring's room outside the pad's face. */
    static constexpr int ringMargin = 2;

private:
    static juce::String dragDescription (int channel);
    static bool parseDragDescription (const juce::var& description, int& channel);
    void setDropHover (bool hover);
    void showMenu();

    TonboxProcessor& processor;
    const int channel;
    int lastHitCount = 0, shownNote = -1;
    juce::String shownName, shownText;
    bool selected = false, dropHover = false, dragStarted = false;
    float flash = 0.0f;
    RenameDialog rename;
};
