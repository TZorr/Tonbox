//
//  PluginEditor.cpp
//  Tonbox
//

#include "PluginEditor.h"
#include "UI/Palette.h"
#include "TextUtf8.h"
#include "ValueText.h"

namespace
{
    /** The panel's theme and the window's size are the user's, not the
        session's: they live in ~/Library/Application Support/Tonbox/
        Tonbox.settings, shared by every instance and every project, and never
        travel with a session. */
    std::unique_ptr<juce::PropertiesFile> openSettings()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = JucePlugin_Name;
        options.filenameSuffix      = ".settings";
        options.folderName          = JucePlugin_Name;
        options.osxLibrarySubFolder = "Application Support";
        options.getDefaultFile().getParentDirectory().createDirectory();
        return std::make_unique<juce::PropertiesFile> (options);
    }

    constexpr const char* darkModeKey = "darkMode";
    constexpr const char* scaleKey    = "panelScale";

    // The mockup's measures.
    constexpr int margin = 12;            // the body's and the header's ends
    constexpr int headerHeight = 65;      // 64 and its hairline
    constexpr int footerHeight = 37;      // 36 and its hairline
    constexpr int topHeight = 132;        // the display and its button column
    constexpr int columnWidth = 150;
    constexpr int rowHeight = 108;        // a rack row
    constexpr int titleColumn = 6 + 22;   // a section's upright title and the room before it
    constexpr int controlRow = 24;        // a section's buttons or hint
    constexpr int padHeight = 72;
    constexpr int padGap = 6;

    const juce::String moonIcon = "M13.2 9.6A5.6 5.6 0 1 1 6.4 2.8a4.5 4.5 0 0 0 6.8 6.8z";
    const juce::String sunIcon  = "M11 8a3 3 0 1 1-6 0a3 3 0 1 1 6 0M8 1.5v1.6M8 12.9v1.6M1.5 8h1.6M12.9 8h1.6"
                                  "M3.4 3.4l1.1 1.1M11.5 11.5l1.1 1.1M3.4 12.6l1.1-1.1M11.5 4.5l1.1-1.1";

    /** The meter's scale: -48 dB at the bottom, +3 at the top. */
    float meterFraction (float gain)
    {
        const auto db = juce::Decibels::gainToDecibels (gain, -100.0f);
        return juce::jlimit (0.0f, 1.0f, juce::jmap (db, -48.0f, 3.0f, 0.0f, 1.0f));
    }

    int textWidth (const juce::Font& font, const juce::String& text)
    {
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, text));
    }

    void setParameter (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        if (auto* parameter = state.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }
}

TonboxEditor::TonboxEditor (TonboxProcessor& p)
    : AudioProcessorEditor (p), tonboxProcessor (p), display (p), presetButton (p), settings (openSettings())
{
    using namespace TbParams;
    using Z = Palette::ZoneId;

    addAndMakeVisible (panel);

    // The saved theme first, so the first paint is already the right one.
    // Dark is the mockup's, and so the default.
    Palette::setDark (settings->getBoolValue (darkModeKey, true));
    shownDark = Palette::dark;
    themeButton.onClick = [this] { setDarkMode (! Palette::dark); };
    refreshThemeIcon();
    panel.addAndMakeVisible (themeButton);

    // A preset is a kit: saved under the kit's name, and the kit takes the
    // name it is saved as - what Logic's menu, the display and the file say
    // is one name.
    presetButton.currentName = [this] { return tonboxProcessor.getKitName(); };
    presetButton.onSaving = [this] (const juce::String& name) { tonboxProcessor.setKitName (name); };
    presetButton.setButtonText (tonboxProcessor.getKitName());

    prevPresetButton.setTooltip ("Previous preset");
    nextPresetButton.setTooltip ("Next preset");
    loadPresetButton.setTooltip ("Load a preset file");
    savePresetButton.setTooltip ("Save the kit as a preset");
    prevPresetButton.onClick = [this] { presetButton.step (-1); };
    nextPresetButton.onClick = [this] { presetButton.step (+1); };
    loadPresetButton.onClick = [this] { presetButton.choose (false); };
    savePresetButton.onClick = [this] { presetButton.choose (true); };

    for (auto* button : { (juce::Button*) &prevPresetButton, (juce::Button*) &nextPresetButton, (juce::Button*) &presetButton,
                          (juce::Button*) &loadPresetButton, (juce::Button*) &savePresetButton })
        panel.addAndMakeVisible (button);

    display.setComponentID ("display");
    panel.addAndMakeVisible (display);

    // The column beside the display: the selected pad's sound in and out of
    // a file, and MIDI Learn for its note.
    for (auto* button : { &loadButton, &saveButton, &learnButton })
        panel.addAndMakeVisible (button);

    loadButton.setComponentID ("loadSound");
    saveButton.setComponentID ("saveSound");
    learnButton.setComponentID ("learnNote");
    loadButton.setTooltip ("Load a sound onto the selected pad - its note stays");
    saveButton.setTooltip ("Save the selected pad's sound");
    learnButton.setTooltip ("Learn: click a pad, press LEARN NOTE, play a key - the key becomes the pad's note");
    loadButton.onClick = [this] { loadSound(); };
    saveButton.onClick = [this] { saveSound(); };
    learnButton.onClick = [this]
    {
        if (tonboxProcessor.getLearnChannel() >= 0)
            tonboxProcessor.cancelLearn();
        else
            tonboxProcessor.startLearn (tonboxProcessor.getSelectedChannel());

        learnButton.active = tonboxProcessor.getLearnChannel() >= 0;
        learnButton.repaint();
        display.refresh();
    };

    for (int c = 0; c < numChannels; ++c)
    {
        pads[(size_t) c] = std::make_unique<PadButton> (tonboxProcessor, c);
        panel.addAndMakeVisible (*pads[(size_t) c]);
    }

    waveButtons.setLabels (waveNames);
    modModeButtons.setLabels (modModeNames);
    filterButtons.setLabels (filterNames);
    envButtons.setLabels (envModeNames);

    // Row one: the sound.
    sections[0] = { "Osc", {}, Z::osc, &waveButtons, Ch::wave, nullptr, nullptr,
                    { { &pitch, Ch::pitch }, { &oscAttack, Ch::oscAttack }, { &oscDecay, Ch::oscDecay } }, {} };
    sections[1] = { "Pitch Mod", {}, Z::osc, &modModeButtons, Ch::modMode, nullptr, nullptr,
                    { { &modAmount, Ch::modAmount }, { &modRate, Ch::modRate } }, {} };
    sections[2] = { "Noise", {}, Z::noise, &filterButtons, Ch::filterMode, &stereoToggle, Ch::stereo,
                    { { &filterFreq, Ch::filterFreq }, { &filterQ, Ch::filterQ } }, {} };
    sections[3] = { "Noise Env", {}, Z::noise, &envButtons, Ch::envMode, nullptr, nullptr,
                    { { &noiseAttack, Ch::noiseAttack }, { &noiseDecay, Ch::noiseDecay } }, {} };

    // Row two: what follows it.
    sections[4] = { "Mix & Drive", utf8 ("Osc \xe2\x86\x90 Mix \xe2\x86\x92 Noise"), Z::shape, nullptr, nullptr, nullptr, nullptr,
                    { { &mix, Ch::mix }, { &distortion, Ch::distortion } }, {} };
    sections[5] = { "EQ", "Peak, after drive", Z::shape, nullptr, nullptr, nullptr, nullptr,
                    { { &eqFreq, Ch::eqFreq }, { &eqGain, Ch::eqGain } }, {} };
    sections[6] = { "Channel", "Shift + Level: all 8", Z::neutral, nullptr, nullptr, &chokeToggle, Ch::choke,
                    { { &level, Ch::level }, { &pan, Ch::pan } }, {} };
    sections[7] = { "Sends", "Post-level, pre-pan", Z::neutral, nullptr, nullptr, nullptr, nullptr,
                    { { &reverbSend, Ch::sends[Fx::reverb] }, { &delaySend, Ch::sends[Fx::delay] }, { &modSend, Ch::sends[Fx::mod] } }, {} };
    sections[8] = { "Velocity", "How much a soft hit takes away", Z::neutral, nullptr, nullptr, nullptr, nullptr,
                    { { &oscVel, Ch::oscVel }, { &noiseVel, Ch::noiseVel }, { &modVel, Ch::modVel } }, {} };

    // Row three: the send effects, as in FMBox.
    for (int slot = 0; slot < Fx::numSlots; ++slot)
    {
        auto& f = fx[(size_t) slot];
        sections[(size_t) (firstFxSection + slot)] = { Fx::titles[slot], {}, Palette::fxZone (slot), &f.types, nullptr, nullptr, nullptr,
                                                       { { &f.a, nullptr }, { &f.b, nullptr }, { &f.level, nullptr } }, {} };
    }

    for (auto& section : sections)
    {
        if (section.choice != nullptr)
        {
            section.choice->setZone (section.zone);

            if (const auto suffix = section.choiceSuffix)
                section.choice->onSelect = [this, suffix] (int index) { setChannelParameter (suffix, (float) index); };

            panel.addAndMakeVisible (section.choice);
        }

        if (section.toggle != nullptr)
        {
            const auto suffix = section.toggleSuffix;
            auto* toggle = section.toggle;
            toggle->setClickingTogglesState (true);
            toggle->onClick = [this, suffix, toggle] { setChannelParameter (suffix, toggle->getToggleState() ? 1.0f : 0.0f); };
            panel.addAndMakeVisible (toggle);
        }

        for (auto& [knob, suffix] : section.knobs)
        {
            knob->setZone (section.zone);
            panel.addAndMakeVisible (knob);
        }
    }

    // Each send knob in the colour of the effect it feeds.
    reverbSend.setZone (Z::reverb);
    delaySend.setZone (Z::delay);
    modSend.setZone (Z::mod);

    for (int slot = 0; slot < Fx::numSlots; ++slot)
    {
        const auto& ids = Fx::ids[slot];
        auto& section = fx[(size_t) slot];
        const auto category = Fx::categories[slot];

        juce::StringArray names;
        for (int t = 0; t < EffectCatalog::numSubtypes (category); ++t)
            names.add (EffectCatalog::info (category, t).name);

        section.types.setLabels (names);
        section.types.onSelect = [this, slot] (int type)
        {
            setParameter (tonboxProcessor.parameters, Fx::ids[slot].type, (float) type);
            refreshFx();
        };

        section.a.attach (tonboxProcessor.parameters, ids.a);
        section.b.attach (tonboxProcessor.parameters, ids.b);
        section.level.attach (tonboxProcessor.parameters, ids.level);

        // Set after attach(): the attachment installs its own text functions,
        // which only know "0..1". These print what the loaded effect means by
        // it - Hz, ms - and the delay's time as the note value it plays.
        section.a.textFromValueFunction = [this, slot] (double v)
        {
            const auto unit = slot == Fx::delay ? EffectCatalog::Unit::Division : fxInfo (slot).unitA;
            return EffectCatalog::format (unit, (float) v);
        };

        section.b.textFromValueFunction = [this, slot] (double v)
        {
            return EffectCatalog::format (fxInfo (slot).unitB, (float) v);
        };
    }

    // The delay always follows the tempo, so its time knob steps through the
    // note values instead of sliding between them.
    fx[Fx::delay].a.setRange (0.0, 1.0, 1.0 / (Dsp::Sync::numDivisions - 1));
    refreshFx();

    // Shift + Level: every channel's level by the same dB (the processor
    // keeps the levels from the start of the drag).
    level.onDragStart = [this]
    {
        if (juce::ModifierKeys::currentModifiers.isShiftDown())
        {
            levelGroupStartDb = (float) level.getValue();
            tonboxProcessor.startLevelGroup (boundChannel);
        }
    };
    level.onValueChange = [this]
    {
        if (tonboxProcessor.isLevelGroupActive())
            tonboxProcessor.moveLevelGroup ((float) level.getValue() - levelGroupStartDb);
    };
    level.onDragEnd = [this] { tonboxProcessor.endLevelGroup(); };
    level.setComponentID ("level");

    stereoToggle.setTooltip ("Separate noise for left and right");
    chokeToggle.setTooltip ("Channels with Choke on cut each other off");

    // Master acts on everything: neutral, not orange.
    masterKnob.setZone (Z::neutral);
    masterKnob.attach (tonboxProcessor.parameters, master);
    panel.addAndMakeVisible (masterKnob);

    bindChannel (tonboxProcessor.getSelectedChannel());
    display.refresh();

    panel.setSize (panelWidth, panelHeight);
    layoutPanel();

    // Any size at the panel's proportions; 85 % the first time.
    const auto scale = juce::jlimit (minScale, maxScale, settings->getDoubleValue (scaleKey, defaultScale));
    setResizable (true, true);
    setResizeLimits (juce::roundToInt (panelWidth * minScale), juce::roundToInt (panelHeight * minScale),
                     juce::roundToInt (panelWidth * maxScale), juce::roundToInt (panelHeight * maxScale));
    getConstrainer()->setFixedAspectRatio ((double) panelWidth / (double) panelHeight);
    setSize (juce::roundToInt (panelWidth * scale), juce::roundToInt (panelHeight * scale));

    startTimerHz (30);
}

TonboxEditor::~TonboxEditor()
{
    stopTimer();
}

const EffectCatalog::Info& TonboxEditor::fxInfo (int slot) const
{
    const auto category = TbParams::Fx::categories[slot];
    const auto type = (int) std::lround (tonboxProcessor.parameters.getRawParameterValue (TbParams::Fx::ids[slot].type)->load());
    return EffectCatalog::info (category, EffectCatalog::clampSubtype (category, type));
}

void TonboxEditor::refreshFx()
{
    for (int slot = 0; slot < TbParams::Fx::numSlots; ++slot)
    {
        auto& section = fx[(size_t) slot];
        const auto category = TbParams::Fx::categories[slot];
        const auto type = EffectCatalog::clampSubtype (
            category, (int) std::lround (tonboxProcessor.parameters.getRawParameterValue (TbParams::Fx::ids[slot].type)->load()));

        if (type == section.shownType)
            continue;

        // A type change - from a button, automation or a restored session -
        // lights the button, renames the two effect knobs and reprints their
        // values in the new units.
        section.shownType = type;
        section.types.setSelected (type);

        const auto& info = fxInfo (slot);
        section.a.setLabel (info.labelA);
        section.b.setLabel (info.labelB);
    }
}

void TonboxEditor::setChannelParameter (const char* suffix, float value)
{
    if (auto* parameter = tonboxProcessor.parameters.getParameter (TbParams::channelId (boundChannel, suffix)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        parameter->endChangeGesture();
    }

    refreshControls();
}

void TonboxEditor::loadSound()
{
    chooser = std::make_unique<juce::FileChooser> ("Load Sound", display.getBrowseFolder(),
                                                   juce::String ("*") + TbParams::soundExtension);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<TonboxEditor> (this)] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        auto& p = safe->tonboxProcessor;
        const auto result = p.loadSound (p.getSelectedChannel(), fc.getResult());

        if (result.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Sound", result.getErrorMessage());

        safe->display.refresh();
    });
}

void TonboxEditor::saveSound()
{
    // A plain name, never one with a dot in it: the macOS save panel selects
    // only the part before the last dot (as Kitbox's Save Kit).
    const auto channel = tonboxProcessor.getSelectedChannel();
    const auto name = PadButton::title (tonboxProcessor, channel);
    const auto suggested = name.isNotEmpty() ? juce::File::createLegalFileName (name).replaceCharacter ('.', ' ').trim()
                                             : juce::String ("Sound");

    chooser = std::make_unique<juce::FileChooser> ("Save Sound", display.getBrowseFolder().getChildFile (suggested),
                                                   juce::String ("*") + TbParams::soundExtension);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe = juce::Component::SafePointer<TonboxEditor> (this), channel] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getResult() == juce::File())
            return;

        const auto file = fc.getResult().withFileExtension (TbParams::soundExtension);
        const auto result = safe->tonboxProcessor.saveSound (channel, file);

        if (result.failed())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save Sound", result.getErrorMessage());

        safe->display.rescanLibrary();
        safe->display.refresh();
    });
}

void TonboxEditor::bindChannel (int channel)
{
    boundChannel = channel;

    for (auto& section : sections)
        for (auto& [knob, suffix] : section.knobs)
            if (suffix != nullptr)
                knob->attach (tonboxProcessor.parameters, TbParams::channelId (channel, suffix));

    shownModMode = -1;
    refreshControls();
}

void TonboxEditor::refreshControls()
{
    const auto read = [this] (const char* suffix)
    {
        return tonboxProcessor.parameters.getRawParameterValue (TbParams::channelId (boundChannel, suffix))->load();
    };

    for (auto& section : sections)
    {
        if (section.choiceSuffix != nullptr)
            section.choice->setSelected ((int) std::lround (read (section.choiceSuffix)));

        if (section.toggleSuffix != nullptr)
            section.toggle->setToggleState (read (section.toggleSuffix) > 0.5f, juce::dontSendNotification);
    }

    // The attachment prints Mod Rate as 0..1; the panel prints what it means
    // in the selected mode. Set after every attach, which installs its own.
    const auto mode = (int) std::lround (read (TbParams::Ch::modMode));

    if (mode != shownModMode)
    {
        shownModMode = mode;
        modRate.textFromValueFunction = [mode] (double v) { return ValueText::modRate ((float) v, mode); };
        modRate.repaint();
    }
}

void TonboxEditor::setDarkMode (bool shouldBeDark)
{
    Palette::setDark (shouldBeDark);
    settings->setValue (darkModeKey, shouldBeDark);
    settings->saveIfNeeded();

    shownDark = shouldBeDark;
    refreshThemeIcon();
    panel.repaint();
}

void TonboxEditor::refreshThemeIcon()
{
    themeButton.setIcon (Palette::dark ? moonIcon : sunIcon);
    themeButton.setTooltip (Palette::dark ? "Switch to the light panel" : "Switch to the dark panel");
}

void TonboxEditor::timerCallback()
{
    tonboxProcessor.applyLearnedNote();

    // Another Tonbox window may have switched the theme: it is process-wide.
    if (shownDark != Palette::dark)
    {
        shownDark = Palette::dark;
        refreshThemeIcon();
        panel.repaint();
    }

    if (tonboxProcessor.getSelectedChannel() != boundChannel)
    {
        bindChannel (tonboxProcessor.getSelectedChannel());
        panel.repaint();
    }
    else
    {
        refreshControls();
    }

    for (auto& pad : pads)
        pad->updateFlash();

    const auto learning = tonboxProcessor.getLearnChannel() >= 0;
    if (learnButton.active != learning)
    {
        learnButton.active = learning;
        learnButton.repaint();
    }

    if (presetButton.getButtonText() != tonboxProcessor.getKitName())
        presetButton.setButtonText (tonboxProcessor.getKitName());

    refreshFx();
    display.refresh();

    // The meter: peaks held since the last tick, falling back at about 20 dB a second.
    {
        bool moved = false;
        for (int c = 0; c < 2; ++c)
        {
            const auto peak = tonboxProcessor.takeMeterPeak (c);
            const auto next = juce::jmax (peak, meterLevels[(size_t) c] * 0.86f);
            const auto shown = next < 1.0e-4f ? 0.0f : next;
            moved = moved || std::abs (meterFraction (shown) - meterFraction (meterLevels[(size_t) c])) > 0.002f;
            meterLevels[(size_t) c] = shown;
        }
        if (moved)
            panel.repaint (metersArea);
    }

    // The status bar: the MIDI light, lit for a moment by each note, and the
    // notes the pads play.
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const auto count = tonboxProcessor.getMidiNoteCount();
        const auto wasLit = now < midiLitUntil;

        if (count != shownMidiCount)
        {
            shownMidiCount = count;
            midiLitUntil = now + 150.0;
        }

        int low = 127, high = 0;
        for (int c = 0; c < TbParams::numChannels; ++c)
        {
            low = juce::jmin (low, tonboxProcessor.getChannelNote (c));
            high = juce::jmax (high, tonboxProcessor.getChannelNote (c));
        }
        const auto notes = "Notes " + TbParams::noteName (low) + utf8 (" \xe2\x80\x93 ") + TbParams::noteName (high);

        if (notes != shownNotes || wasLit != (now < midiLitUntil))
        {
            shownNotes = notes;
            panel.repaint (footerArea);
        }
    }
}

//==============================================================================
void TonboxEditor::paint (juce::Graphics& g)
{
    // Only where the scaled panel leaves a sliver at the edge.
    g.fillAll (Palette::body);
}

void TonboxEditor::resized()
{
    const auto scale = (float) getWidth() / (float) panelWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));

    // Remembered once the window is up: the constructor's own setSize is the
    // saved size, not a choice.
    if (isTimerRunning() && std::abs (settings->getDoubleValue (scaleKey, defaultScale) - scale) > 0.001)
        settings->setValue (scaleKey, (double) scale);
}

void TonboxEditor::layoutRow (int first, int count, juce::Rectangle<int> row)
{
    // Each section as wide as its knobs are many, a 1-point upright between
    // them; the upright title 22 wide, 6 in; then, 8 in and centred upright,
    // the buttons or hint over the knobs, 4 apart.
    int columnsTotal = 0;
    for (int i = first; i < first + count; ++i)
        columnsTotal += (int) sections[(size_t) i].knobs.size();

    const auto available = (float) (row.getWidth() - (count - 1));
    int before = 0;

    for (int i = 0; i < count; ++i)
    {
        auto& section = sections[(size_t) (first + i)];
        const auto columns = (int) section.knobs.size();
        const auto after = before + columns;
        const auto left  = row.getX() + i + juce::roundToInt (available * (float) before / (float) columnsTotal);
        const auto right = i + 1 == count ? row.getRight()
                                          : row.getX() + i + juce::roundToInt (available * (float) after / (float) columnsTotal);
        section.area = { left, row.getY(), right - left, row.getHeight() };
        before = after;

        auto inner = section.area.withTrimmedLeft (titleColumn).reduced (8, 0);
        const auto contentHeight = controlRow + 4 + Knob::stackedHeight;
        inner = inner.withSizeKeepingCentre (inner.getWidth(), contentHeight);

        auto controls = inner.removeFromTop (controlRow);
        inner.removeFromTop (4);

        if (section.toggle != nullptr)
        {
            section.toggle->setBounds (controls.removeFromRight (section.toggle->getIdealWidth()));
            controls.removeFromRight (6);
        }

        if (section.choice != nullptr)
            section.choice->setBounds (controls);

        const auto cell = (float) inner.getWidth() / (float) columns;

        for (int c = 0; c < columns; ++c)
        {
            const auto x = inner.getX() + juce::roundToInt (cell * (float) c);
            const auto w = inner.getX() + juce::roundToInt (cell * (float) (c + 1)) - x;
            section.knobs[(size_t) c].first->setBounds (x, inner.getY(), w, Knob::stackedHeight);
        }
    }
}

void TonboxEditor::layoutPanel()
{
    auto area = juce::Rectangle<int> (0, 0, panelWidth, panelHeight);

    // Header: from the left, the wordmark, the preset, Load / Save; from the
    // right, the theme, Master and its meter.
    headerArea = area.removeFromTop (headerHeight);
    {
        auto row = headerArea.withTrimmedBottom (1).reduced (margin, 0);
        const auto buttonRow = [] (juce::Rectangle<int> r) { return r.withSizeKeepingCentre (r.getWidth(), 36); };

        const auto wordmark = textWidth (Palette::font (22.0f, true, 0.16f), "TONBOX");
        const auto subtitle = textWidth (Palette::label (12.0f), "8-CHANNEL DRUM SYNTH");
        wordmarkArea = row.removeFromLeft (wordmark + 14 + subtitle);
        row.removeFromLeft (20);
        headerDividers[0] = row.removeFromLeft (1).withSizeKeepingCentre (1, 28);
        row.removeFromLeft (20);

        prevPresetButton.setBounds (buttonRow (row.removeFromLeft (36)));
        row.removeFromLeft (6);
        nextPresetButton.setBounds (buttonRow (row.removeFromLeft (36)));
        row.removeFromLeft (6);
        presetButton.setBounds (buttonRow (row.removeFromLeft (280)));
        row.removeFromLeft (20);

        for (auto* button : { &loadPresetButton, &savePresetButton })
        {
            const auto width = 14 + 16 + 8 + textWidth (Palette::label (12.0f), button->getButtonText().toUpperCase()) + 14;
            button->setBounds (buttonRow (row.removeFromLeft (width)));
            row.removeFromLeft (6);
        }

        themeButton.setBounds (buttonRow (row.removeFromRight (36)));
        row.removeFromRight (20);
        headerDividers[1] = row.removeFromRight (1).withSizeKeepingCentre (1, 28);
        row.removeFromRight (20);

        metersArea = row.removeFromRight (13).withSizeKeepingCentre (13, 34);
        row.removeFromRight (10);
        masterKnob.setBounds (row.removeFromRight (38 + 10 + 64).withSizeKeepingCentre (38 + 10 + 64, 38));
    }

    footerArea = area.removeFromBottom (footerHeight);

    auto body = area.reduced (margin, 0);
    body.removeFromTop (8);

    // The display and the column of sound buttons.
    {
        auto top = body.removeFromTop (topHeight);
        auto column = top.removeFromRight (columnWidth);
        top.removeFromRight (6);
        display.setBounds (top);

        const auto buttonHeight = (column.getHeight() - 2 * 6) / 3;
        loadButton.setBounds (column.removeFromTop (buttonHeight));
        column.removeFromTop (6);
        saveButton.setBounds (column.removeFromTop (buttonHeight));
        column.removeFromTop (6);
        learnButton.setBounds (column);
    }

    body.removeFromTop (6);

    // The rack: a hairline round it, three rows with a hairline between.
    rackArea = body.removeFromTop (1 + 3 * rowHeight + 2 + 1);
    {
        auto inner = rackArea.reduced (1);

        // The send effects first, right under the display; then the sound
        // and what follows it.
        for (int r = 0; r < 3; ++r)
        {
            rackRows[(size_t) r] = inner.removeFromTop (rowHeight);
            inner.removeFromTop (1);

            const auto group = rowOrder[r];
            int first = 0;
            for (int g = 0; g < group; ++g)
                first += rowSizes[g];

            layoutRow (first, rowSizes[group], rackRows[(size_t) r]);
        }
    }

    body.removeFromTop (6);

    // The pads, in a dark well.
    padWellArea = body.removeFromTop (padHeight + 2 * padGap);
    {
        const auto grid = padWellArea.reduced (padGap);
        const auto width = (float) (grid.getWidth() - 7 * padGap) / 8.0f;

        for (int c = 0; c < TbParams::numChannels; ++c)
        {
            const auto x = grid.getX() + juce::roundToInt ((width + (float) padGap) * (float) c);
            const auto right = grid.getX() + juce::roundToInt ((width + (float) padGap) * (float) c + width);
            pads[(size_t) c]->setBounds (juce::Rectangle<int> (x, grid.getY(), right - x, padHeight).expanded (PadButton::ringMargin));
        }
    }
}

//==============================================================================
void TonboxEditor::paintPanel (juce::Graphics& g)
{
    g.fillAll (Palette::body);

    // Header.
    g.setColour (Palette::header);
    g.fillRect (headerArea);
    g.setColour (Palette::line);
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 1));

    {
        const auto font = Palette::font (22.0f, true, 0.16f);
        const auto tonWidth = textWidth (font, "TON");
        const auto boxWidth = textWidth (font, "TONBOX");
        g.setFont (font);
        g.setColour (Palette::accent);
        g.drawText ("TON", wordmarkArea, juce::Justification::centredLeft);
        g.setColour (Palette::title);
        g.drawText ("BOX", wordmarkArea.withTrimmedLeft (tonWidth), juce::Justification::centredLeft);

        g.setColour (Palette::inkFaint);
        g.setFont (Palette::label (12.0f));
        g.drawText ("8-CHANNEL DRUM SYNTH", wordmarkArea.withTrimmedLeft (boxWidth + 14).translated (0, 2), juce::Justification::centredLeft);
    }

    g.setColour (Palette::divider);
    for (const auto& divider : headerDividers)
        g.fillRect (divider);

    // The meter beside Master: two bars, left and right.
    for (int c = 0; c < 2; ++c)
    {
        const auto bar = juce::Rectangle<float> ((float) metersArea.getX() + (float) c * 8.0f, (float) metersArea.getY(),
                                                 5.0f, (float) metersArea.getHeight());
        g.setColour (Palette::meterTrack);
        g.fillRoundedRectangle (bar, 2.0f);

        const auto peak = meterLevels[(size_t) c];
        const auto fraction = meterFraction (peak);
        if (fraction > 0.0f)
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (bar, 2.0f);
            g.reduceClipRegion (clip);
            g.setColour (peak >= 1.0f ? Palette::warning : peak >= 0.7f ? Palette::accent : Palette::signal);
            g.fillRect (bar.withTop (bar.getBottom() - bar.getHeight() * fraction));
        }
    }

    // The rack: one panel with a hairline edge, the send effects' row on top
    // a step darker, hairlines between the rows and uprights between the sections.
    {
        const auto outer = rackArea.toFloat();
        constexpr float radius = 5.0f;

        juce::Graphics::ScopedSaveState state (g);
        juce::Path shape;
        shape.addRoundedRectangle (outer, radius);
        g.setColour (Palette::rackEdge);
        g.fillPath (shape);

        juce::Path inside;
        inside.addRoundedRectangle (outer.reduced (1.0f), radius - 1.0f);
        g.reduceClipRegion (inside);
        g.setColour (Palette::rackPanel);
        g.fillRect (outer);
        g.setColour (Palette::rackFxRow);
        g.fillRect (rackRows[0].toFloat().withTop (outer.getY()));

        g.setColour (Palette::rackEdge);
        g.fillRect (rackRows[0].getX(), rackRows[0].getBottom(), rackRows[0].getWidth(), 1);
        g.fillRect (rackRows[1].getX(), rackRows[1].getBottom(), rackRows[1].getWidth(), 1);

        int first = 0;
        for (int r = 0; r < 3; ++r)
        {
            for (int i = first; i < first + rowSizes[r]; ++i)
            {
                const auto& section = sections[(size_t) i];
                const auto zone = Palette::zone (section.zone);

                if (i > first)
                {
                    g.setColour (Palette::rackDivider);
                    g.fillRect (section.area.getX() - 1, section.area.getY(), 1, section.area.getHeight());
                }

                // The dot over the title, the title read bottom to top; centred as a pair.
                const auto column = section.area.withWidth (titleColumn).withTrimmedLeft (6).toFloat();
                const auto font = Palette::font (10.0f, true, 0.12f);
                const auto title = section.title.toUpperCase();
                const auto length = juce::GlyphArrangement::getStringWidth (font, title);
                const auto top = column.getCentreY() - (6.0f + 6.0f + length) * 0.5f;

                g.setColour (zone.line);
                g.fillEllipse (column.getCentreX() - 3.0f, top, 6.0f, 6.0f);

                const auto centre = juce::Point<float> (column.getCentreX(), top + 12.0f + length * 0.5f);
                juce::Graphics::ScopedSaveState rotated (g);
                g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, centre.x, centre.y));
                g.setColour (zone.label);
                g.setFont (font);
                g.drawText (title, juce::Rectangle<float> (length + 4.0f, 14.0f).withCentre (centre), juce::Justification::centred, false);
            }

            first += rowSizes[r];
        }
    }

    // The hints, where a section has no buttons.
    for (const auto& section : sections)
    {
        if (section.hint.isEmpty())
            continue;

        auto row = section.area.withTrimmedLeft (titleColumn).reduced (8, 0);
        row = row.withSizeKeepingCentre (row.getWidth(), controlRow + 4 + Knob::stackedHeight).removeFromTop (controlRow);

        if (section.toggle != nullptr)
            row.removeFromRight (section.toggle->getIdealWidth() + 6);

        g.setColour (Palette::inkFaint);
        g.setFont (Palette::font (10.5f, true, 0.06f));
        g.drawFittedText (section.hint.toUpperCase(), row, juce::Justification::centredLeft, 1, 0.8f);
    }

    g.setColour (Palette::padWell);
    g.fillRoundedRectangle (padWellArea.toFloat(), 4.0f);

    // Status bar.
    {
        auto area = footerArea;
        g.setColour (Palette::footer);
        g.fillRect (area);
        g.setColour (Palette::line.interpolatedWith (Palette::footer, 0.2f));
        g.fillRect (area.removeFromTop (1));
        area = area.reduced (margin, 0);

        const auto font = Palette::font (11.0f, false, 0.06f);
        g.setFont (font);

        const auto version = juce::String ("TONBOX " JucePlugin_VersionString);
        g.setColour (Palette::inkSoft);
        g.drawText (version, area.removeFromLeft (textWidth (font, version)), juce::Justification::centredLeft);
        area.removeFromLeft (16);
        g.setColour (Palette::divider);
        g.fillRect (area.removeFromLeft (1).withSizeKeepingCentre (1, 14));
        area.removeFromLeft (16);

        // Right: the MIDI light and the notes the pads play.
        g.setColour (Palette::inkFaint);
        g.drawText (shownNotes, area.removeFromRight (textWidth (font, shownNotes) + 2), juce::Justification::centredRight);
        area.removeFromRight (16);

        auto midi = area.removeFromRight (6 + 6 + textWidth (font, "MIDI"));
        const auto lit = juce::Time::getMillisecondCounterHiRes() < midiLitUntil;
        g.setColour (lit ? Palette::signal : Palette::lightOff);
        g.fillEllipse (midi.removeFromLeft (6).withSizeKeepingCentre (6, 6).toFloat());
        midi.removeFromLeft (6);
        g.setColour (Palette::inkFaint);
        g.drawText ("MIDI", midi, juce::Justification::centredLeft);
        area.removeFromRight (16);

        const auto hints = juce::StringArray { "Click a pad to select and hear it", "Drag onto another to swap",
                                               "Right-click: copy, paste, rename", "Display: pick a group, step the sounds" };
        g.drawText (hints.joinIntoString (utf8 ("   \xc2\xb7   ")), area, juce::Justification::centredLeft, true);
    }
}
