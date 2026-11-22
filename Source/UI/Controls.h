//
//  Controls.h
//  Tonbox
//
//  Kitbox 0.10's header controls, and how every menu on the panel opens.
//
//  - panelMenu: a menu as a child of the plugin window (see below).
//  - PanelButton: the rounded buttons - an icon, a word, or both - and the
//    preset field, whose text is the kit's name with a small framed tag
//    after it.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Palette.h"

/** How every menu on the panel opens: as a child of the plugin window, not
    as a window of its own. Logic hosts Audio Units in a process of its own
    (AUHostingService), and a JUCE menu in a separate desktop window there is
    drawn but never gets the click - the menu shows and nothing happens. */
inline juce::PopupMenu::Options panelMenu (juce::Component& target)
{
    return juce::PopupMenu::Options().withTargetComponent (&target)
                                     .withParentComponent (target.getTopLevelComponent());
}

class PanelButton : public juce::Button
{
public:
    enum class Style
    {
        plain,    // the header's: icon and word, 12 points
        column,   // LOAD SOUND, SAVE SOUND, LEARN NOTE: a word, 11 points, wide apart
        field     // the preset field: a name, its tag, a chevron
    };

    /** `iconPath` is SVG path data in a 16-unit box, stroked. May be empty. */
    PanelButton (const juce::String& text, const juce::String& iconPath = {}, Style styleToUse = Style::plain);

    void setIcon (const juce::String& iconPath);

    /** Lit in the accent colour: Learn while it waits for a note. */
    bool active = false;

    /** The preset field's small framed word after the name. */
    juce::String tag;

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    juce::Path stroke;
    Style style;
};
