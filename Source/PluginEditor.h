//
//  PluginEditor.h
//  Tonbox
//
//  The panel, since 0.8.0 the "Tonbox - Kitbox-Look" mockup (Kitbox 0.10's
//  "Variante B"):
//
//      TONBOX  [<][>][ kit  KIT  v] LOAD SAVE                (o) Master ||  [moon]
//      [ CHANNEL 2  | envelopes: osc, noise | BD SD HH PERC FX ]  LOAD SOUND
//      [ T BD 2     |                       | [<][ T BD 2  v][>] ]  SAVE SOUND
//      [ C#1 . 37   |                       | BD . 2 / 3   KIT ..]  LEARN NOTE
//      +--------------------------------------------------------------------+
//      |R| REVERB          |D| DELAY           |M| MOD                      |
//      |O| OSC        |P| PITCH MOD |N| NOISE      |N| NOISE ENV           |
//      |M| MIX & DRIVE |E| EQ |C| CHANNEL |S| SENDS |V| VELOCITY           |
//      +--------------------------------------------------------------------+
//      [ 1 KICK ][ 2 T BD 2 ][ 3 CLAP ] ... eight pads, one per channel
//      TONBOX 0.8.0 | hints                                MIDI  Notes C1 - G1
//
//  The rack is one panel: three rows split by hairlines, each row's sections
//  by uprights, every section named down its left edge, and the colour of
//  what it works on in its knobs' rings and its lit buttons - amber for the
//  oscillator, blue for the noise, red for what shapes the mix, grey for the
//  channel strip, one colour per send effect. The lower two rows belong to
//  the selected pad and are rebound when another pad is selected; twenty-four
//  controls a channel are too many to show eight times over, so the panel
//  edits one channel at a time. Above them, right
//  under the display, the three shared send effects, a step darker.
//
//  The panel is laid out once at the mockup's 1416 x 672 and scaled as a
//  whole: the window resizes freely at that aspect ratio, opens at 85 % the
//  first time, and remembers the size per user.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/ChoiceButtons.h"
#include "UI/Controls.h"
#include "UI/Display.h"
#include "UI/FlatToggle.h"
#include "UI/Knob.h"
#include "UI/PadButton.h"
#include "UI/PresetButton.h"

class TonboxEditor : public juce::AudioProcessorEditor,
                     public juce::DragAndDropContainer,   // pads swap by drag and drop
                     private juce::Timer
{
public:
    explicit TonboxEditor (TonboxProcessor&);
    ~TonboxEditor() override;

    /** The panel's own size; the window shows it scaled. */
    static constexpr int panelWidth  = 1416;
    static constexpr int panelHeight = 672;
    static constexpr double defaultScale = 0.85, minScale = 0.6, maxScale = 1.4;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** The 1416 x 672 panel every control sits on, for the tests. */
    juce::Component& getPanel() noexcept { return panel; }

private:
    /** The panel: everything on it is its child, and it is drawn through the
        editor's paintPanel. */
    struct Panel : public juce::Component
    {
        explicit Panel (TonboxEditor& ownerToUse) : owner (ownerToUse) { setOpaque (true); }
        void paint (juce::Graphics& g) override { owner.paintPanel (g); }
        TonboxEditor& owner;
    };

    /** A section of the rack: its upright title, a row of buttons (or a
        hint) over its knobs. A channel section's parts carry the parameter
        suffix they are bound to; an effect's are bound once. */
    struct Section
    {
        juce::String title, hint;
        Palette::ZoneId zone = Palette::ZoneId::none;
        ChoiceButtons* choice = nullptr;
        const char* choiceSuffix = nullptr;
        FlatToggle* toggle = nullptr;
        const char* toggleSuffix = nullptr;
        std::vector<std::pair<Knob*, const char*>> knobs;
        juce::Rectangle<int> area;
    };

    struct FxSection
    {
        ChoiceButtons types;
        Knob a { "A" }, b { "B" }, level { "Level" };
        int shownType = -1;
    };

    void timerCallback() override;
    void setDarkMode (bool shouldBeDark);
    void refreshThemeIcon();

    void paintPanel (juce::Graphics&);
    void layoutPanel();
    void layoutRow (int first, int count, juce::Rectangle<int> row);

    /** Points every control at the selected channel's parameters. */
    void bindChannel (int channel);

    /** Lights the right buttons and toggles, and reprints Mod Rate in the
        unit its mode uses. */
    void refreshControls();

    void setChannelParameter (const char* suffix, float value);

    /** The selected pad's sound from a .tonboxsound file, or into one - the
        file panels open in the group the display is browsing. */
    void loadSound();
    void saveSound();

    /** The effect loaded in a send slot. */
    const EffectCatalog::Info& fxInfo (int slot) const;
    void refreshFx();

    TonboxProcessor& tonboxProcessor;
    Panel panel { *this };
    Display display;
    std::array<std::unique_ptr<PadButton>, TbParams::numChannels> pads;

    ChoiceButtons waveButtons, modModeButtons, filterButtons, envButtons;
    FlatToggle stereoToggle { "Stereo" }, chokeToggle { "Choke" };

    Knob pitch { "Pitch" }, oscAttack { "Attack" }, oscDecay { "Decay" };
    Knob modAmount { "Amount", true }, modRate { "Rate" };
    Knob filterFreq { "Freq" }, filterQ { "Q" };
    Knob noiseAttack { "Attack" }, noiseDecay { "Decay" };
    Knob mix { "Mix" }, distortion { "Drive" };
    Knob eqFreq { "Freq" }, eqGain { "Gain", true };
    Knob level { "Level" }, pan { "Pan", true };
    Knob oscVel { "Osc" }, noiseVel { "Noise" }, modVel { "Mod" };
    Knob reverbSend { "Reverb" }, delaySend { "Delay" }, modSend { "Mod" };

    // Groups of 4, 5 and 3 sections: the sound, what follows it, the send
    // effects. The rack shows them top to bottom in rowOrder: the effects
    // first, under the display.
    static constexpr int rowSizes[] { 4, 5, 3 };
    static constexpr int rowOrder[] { 2, 0, 1 };
    std::array<Section, 12> sections;
    static constexpr int firstFxSection = 9;

    std::array<FxSection, TbParams::Fx::numSlots> fx;

    Knob masterKnob { "Master", false, Knob::Layout::beside };
    PresetButton presetButton;
    PanelButton prevPresetButton { {}, "M10 3.5 5.5 8l4.5 4.5" }, nextPresetButton { {}, "M6 3.5 10.5 8 6 12.5" };
    PanelButton loadPresetButton { "Load", "M2 4.5A1.5 1.5 0 0 1 3.5 3H6l1.5 1.5h5A1.5 1.5 0 0 1 14 6v5.5a1.5 1.5 0 0 1-1.5 1.5h-9A1.5 1.5 0 0 1 2 11.5z" };
    PanelButton savePresetButton { "Save", "M8 2.5v7.5M4.8 7 8 10.2 11.2 7M3 13.5h10" };
    PanelButton themeButton { {} };
    PanelButton loadButton { "Load Sound", {}, PanelButton::Style::column },
                saveButton { "Save Sound", {}, PanelButton::Style::column },
                learnButton { "Learn Note", {}, PanelButton::Style::column };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::PropertiesFile> settings;
    bool shownDark = false;
    int boundChannel = -1, shownModMode = -1;
    float levelGroupStartDb = 0.0f;   // the dragged Level at the start of a Shift drag

    // Header meter, status bar.
    std::array<float, 2> meterLevels {};
    int shownMidiCount = 0;
    double midiLitUntil = 0.0;
    juce::String shownNotes;

    juce::Rectangle<int> headerArea, wordmarkArea, metersArea, rackArea, padWellArea, footerArea;
    std::array<juce::Rectangle<int>, 3> rackRows;
    std::array<juce::Rectangle<int>, 2> headerDividers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TonboxEditor)
};
