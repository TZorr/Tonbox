//
//  PresetButton.h
//  Tonbox
//
//  The plugin's own preset menu: the header's preset field, which shows the
//  kit's name, with < > beside it and LOAD / SAVE after it. Logic has a preset
//  menu of its own for every AU; a VST3 host such as Bitwig has none, so
//  without this the plugin could neither load nor save a setting there.
//
//  The menu lists ~/Library/Audio/Presets/<company>/<plugin>/ with each
//  subfolder as a submenu, and saves into it in the plugin's own file type
//  (Engine/PresetFile.h).
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"

class PresetButton : public PanelButton
{
public:
    explicit PresetButton (juce::AudioProcessor&);

    /** The name Save Preset suggests. Unset: the preset last loaded or saved. */
    std::function<juce::String()> currentName;

    /** Called with the preset's name just before its state is taken, so a
        plugin that keeps a name in its state can store it there. */
    std::function<void (const juce::String&)> onSaving;

    /** Loads the next (+1) or previous (-1) preset of the menu's list, wrapping
        round. Unset lastFile: + starts at the first, - at the last. */
    void step (int delta);

    /** A file panel to save the kit as a preset (true), or to load one. */
    void choose (bool saving);

    /** ~/Library/Audio/Presets/<company>/<plugin>/, created if it is missing. */
    static juce::File folder();

    /** The plugin's state as a preset file named after the file. */
    static juce::Result save (juce::AudioProcessor&, const juce::File&);

    /** A preset file of this plugin into its state. */
    static juce::Result load (juce::AudioProcessor&, const juce::File&);

private:
    void clicked() override;
    void loadFile (const juce::File&);
    void saveFile (const juce::File&);

    juce::AudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetButton)
};
