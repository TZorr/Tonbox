//
//  Display.cpp
//  Tonbox
//

#include "Display.h"
#include "PadButton.h"
#include "Controls.h"
#include "Palette.h"
#include "PluginProcessor.h"
#include "SoundLibrary.h"
#include "ValueText.h"

namespace
{
    const juce::String dot = juce::String::charToString (0x00b7);

    /** A group with more sounds than this lists them by name prefix (AC, EM,
        SC, VT ...), one submenu each - SoundLibrary::listPrefix. */
    constexpr int flatListLimit = 40;
}

Display::Display (TonboxProcessor& processorToUse) : processor (processorToUse)
{
    setInterceptsMouseClicks (true, false);
    libraryRoot = SoundLibrary::folder();
    rescanLibrary();
}

void Display::setLibraryRoot (const juce::File& root)
{
    libraryRoot = root;
    browseGroup = 0;
    rescanLibrary();
    padSound = juce::File ("/");   // forces updateBrowser to look again
    refresh();
}

void Display::rescanLibrary()
{
    groups = SoundLibrary::groups (libraryRoot);
    browseGroup = juce::jlimit (0, juce::jmax (0, groups.size() - 1), browseGroup);
    listedGroup = -2;
}

juce::File Display::getBrowseFolder() const
{
    return juce::isPositiveAndBelow (browseGroup, groups.size()) ? groups[browseGroup] : libraryRoot;
}

void Display::selectGroup (int index)
{
    if (! juce::isPositiveAndBelow (index, groups.size()))
        return;

    browseGroup = index;
    updateBrowser();
    repaint();
}

void Display::stepSound (int delta)
{
    if (juce::isPositiveAndBelow (browseGroup, groups.size()))
        loadSound (SoundLibrary::step (groups[browseGroup], padSound, delta));
}

void Display::loadSound (const juce::File& file)
{
    if (file == juce::File())
        return;

    const auto result = processor.loadSound (processor.getSelectedChannel(), file);

    if (result.failed())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Load Sound", result.getErrorMessage());

    refresh();
}

void Display::showSoundList()
{
    if (! juce::isPositiveAndBelow (browseGroup, groups.size()))
        return;

    // Read again: a sound saved since shows up.
    groupSounds = SoundLibrary::sounds (groups[browseGroup]);
    listedGroup = browseGroup;

    if (groupSounds.isEmpty())
        return;

    juce::PopupMenu menu;
    juce::StringArray prefixes;
    std::vector<juce::PopupMenu> subMenus;
    std::vector<bool> holdsCurrent;

    for (int i = 0; i < groupSounds.size(); ++i)
    {
        const auto& file = groupSounds.getReference (i);
        const auto name = file.getFileNameWithoutExtension();
        const auto current = file == padSound;

        if (groupSounds.size() <= flatListLimit)
        {
            menu.addItem (i + 1, name, true, current);
            continue;
        }

        const auto prefix = SoundLibrary::listPrefix (name);
        auto index = prefixes.indexOf (prefix);

        if (index < 0)
        {
            index = prefixes.size();
            prefixes.add (prefix);
            subMenus.emplace_back();
            holdsCurrent.push_back (false);
        }

        subMenus[(size_t) index].addItem (i + 1, name, true, current);
        holdsCurrent[(size_t) index] = holdsCurrent[(size_t) index] || current;
    }

    for (size_t i = 0; i < subMenus.size(); ++i)
        menu.addSubMenu (prefixes[(int) i], subMenus[i], true, nullptr, holdsCurrent[i]);

    menu.showMenuAsync (panelMenu (*this).withTargetScreenArea (localAreaToGlobal (nameArea.toNearestInt())),
                        [safe = juce::Component::SafePointer<Display> (this), list = groupSounds] (int result)
    {
        if (safe != nullptr && juce::isPositiveAndBelow (result - 1, list.size()))
            safe->loadSound (list[result - 1]);
    });
}

void Display::mouseDown (const juce::MouseEvent& event)
{
    const auto at = event.position;

    for (int i = 0; i < groupAreas.size(); ++i)
        if (groupAreas.getReference (i).contains (at))
            return selectGroup (i);

    if (prevArea.contains (at))
        return stepSound (-1);

    if (nextArea.contains (at))
        return stepSound (1);

    if (nameArea.contains (at))
        return showSoundList();

    if (kitArea.contains (at))
        rename.open (*this, "Rename Kit", processor.getKitName(),
                     [this] (const juce::String& name) { processor.setKitName (name); refresh(); });
}

void Display::mouseMove (const juce::MouseEvent& event)
{
    const auto at = event.position;
    auto clickable = kitArea.contains (at) || prevArea.contains (at) || nextArea.contains (at) || nameArea.contains (at);

    for (const auto& area : groupAreas)
        clickable = clickable || area.contains (at);

    setMouseCursor (clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

bool Display::updateBrowser()
{
    auto changed = false;
    const auto file = processor.getChannelSoundFile (processor.getSelectedChannel());

    // Another pad, or this one's sound from another file: browse its group.
    if (file != padSound)
    {
        padSound = file;
        const auto group = SoundLibrary::groupOf (libraryRoot, file);

        if (group != juce::File() && ! groups.contains (group))
            rescanLibrary();   // a folder made since

        if (const auto index = groups.indexOf (group); index >= 0)
            browseGroup = index;

        listedGroup = -2;      // the file may be new to the list
        changed = true;
    }

    if (listedGroup != browseGroup)
    {
        listedGroup = browseGroup;
        groupSounds = juce::isPositiveAndBelow (browseGroup, groups.size()) ? SoundLibrary::sounds (groups[browseGroup])
                                                                             : juce::Array<juce::File>();
        changed = true;
    }

    if (const auto index = groupSounds.indexOf (padSound); index != soundIndex)
    {
        soundIndex = index;
        changed = true;
    }

    return changed;
}

void Display::refresh()
{
    // Only the selection and what it is - nothing from the engine, so a note
    // coming in changes nothing here.
    const auto learning = processor.getLearnChannel();
    const auto selected = processor.getSelectedChannel();
    const auto note = processor.getChannelNote (selected);
    const auto kit = processor.getKitName();
    const auto name = PadButton::title (processor, selected);
    const auto text = PadButton::describe (processor, selected);
    const auto browserChanged = updateBrowser();

    const auto settings = processor.getChannelSettings (selected);
    const std::array<float, 6> shape { settings.oscAttackMs, settings.oscDecayMs, settings.noiseAttackMs,
                                       settings.noiseDecayMs, (float) settings.envMode, settings.mix };
    const auto shapeChanged = shape != shownShape;
    shownShape = shape;

    if (browserChanged || shapeChanged || learning != shownLearn || selected != shownSelected || note != shownNote || kit != shownKit
        || name != shownName || text != shownText)
    {
        shownLearn = learning;
        shownSelected = selected;
        shownNote = note;
        shownKit = kit;
        shownName = name;
        shownText = text;
        repaint();
    }
}

namespace
{
    /** One envelope's level (0..1) at `ms` after the hit, as the engine runs
        it: exponential segments of 60 dB, or Lin's straight lines in two
        thirds of the time, or Mod's clap - bursts falling 24 dB each through
        the attack, then the decay. */
    float envelopeAt (float ms, float attackMs, float decayMs, int mode)
    {
        using E = ToneEngine;
        attackMs = juce::jmax (0.0f, attackMs);
        decayMs = juce::jmax (0.1f, decayMs);

        if (mode == E::envLin)
        {
            const auto a = attackMs * (2.0f / 3.0f), d = decayMs * (2.0f / 3.0f);
            if (ms < a)
                return ms / a;
            return juce::jmax (0.0f, 1.0f - (ms - a) / d);
        }

        if (mode == E::envMod && attackMs > 0.0f)
        {
            const auto period = juce::jmax (0.5f, juce::jmin (3.42f * std::pow (attackMs, 0.4261f), attackMs / 3.0f));
            const auto bursts = juce::jmax (1, juce::roundToInt (attackMs / period));
            const auto end = (float) bursts * period;

            if (ms < end)
                return std::pow (0.0630957f, std::fmod (ms, period) / period);

            return std::pow (0.001f, (ms - end) / decayMs);
        }

        if (ms < attackMs)
            return 0.001f * std::pow (1000.0f, ms / attackMs);

        return std::pow (0.001f, (ms - attackMs) / decayMs);
    }

    /** How long an envelope stays visible on a linear scale: past two thirds
        of an exponential decay it is under 1 %. */
    float visibleMs (float attackMs, float decayMs, int mode)
    {
        if (mode == ToneEngine::envLin)
            return (attackMs + decayMs) * (2.0f / 3.0f);
        return attackMs + 0.66f * decayMs;
    }

    void drawTriangle (juce::Graphics& g, juce::Rectangle<float> box, bool pointsLeft, juce::Colour colour)
    {
        const auto c = box.getCentre();
        juce::Path p;
        p.addTriangle (c.x + (pointsLeft ? 2.75f : -2.75f), c.y - 3.5f,
                       c.x + (pointsLeft ? 2.75f : -2.75f), c.y + 3.5f,
                       c.x + (pointsLeft ? -2.75f : 2.75f), c.y);
        g.setColour (colour);
        g.fillPath (p);
    }

    float widthOf (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }
}

void Display::paintEnvelopes (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto oscA = shownShape[0], oscD = shownShape[1], noiseA = shownShape[2], noiseD = shownShape[3];
    const auto mode = (int) shownShape[4];
    const auto mix = shownShape[5];

    const auto span = juce::jmax (20.0f, visibleMs (oscA, oscD, ToneEngine::envExp), visibleMs (noiseA, noiseD, mode));

    // As the mockup: the peak at 18 % of the height, silence at 96 %.
    const auto top = area.getY() + area.getHeight() * 0.18f;
    const auto bottom = area.getY() + area.getHeight() * 0.96f;
    constexpr int steps = 200;

    const auto curve = [&] (float attack, float decay, int envMode)
    {
        juce::Path path;
        path.startNewSubPath (area.getX(), bottom);

        for (int i = 0; i <= steps; ++i)
        {
            const auto ms = span * (float) i / (float) steps;
            const auto level = envelopeAt (ms, attack, decay, envMode);
            path.lineTo (area.getX() + area.getWidth() * (float) i / (float) steps, bottom - (bottom - top) * level);
        }

        return path;
    };

    const auto oscZone = Palette::zone (Palette::ZoneId::osc).line;
    const auto noiseZone = Palette::zone (Palette::ZoneId::noise).line;

    // A side the mix leaves out is drawn faint: its shape is there, its sound is not.
    g.setColour (noiseZone.withAlpha (mix <= 0.0f ? 0.3f : 1.0f));
    g.strokePath (curve (noiseA, noiseD, mode), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (oscZone.withAlpha (mix >= 100.0f ? 0.3f : 1.0f));
    g.strokePath (curve (oscA, oscD, ToneEngine::envExp), juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // The legend, top left.
    auto legend = area.reduced (10.0f, 0.0f).withTrimmedTop (8.0f).withHeight (12.0f);
    const auto font = Palette::font (10.0f, true, 0.08f);
    g.setFont (font);

    for (const auto& [name, colour] : { std::pair<const char*, juce::Colour> { "OSC", oscZone }, { "NOISE", noiseZone } })
    {
        g.setColour (colour);
        g.fillRect (legend.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 2.0f));
        legend.removeFromLeft (5.0f);
        g.setColour (Palette::inkSoft);
        const auto w = widthOf (font, name) + 1.0f;
        g.drawText (name, legend.removeFromLeft (w), juce::Justification::centredLeft);
        legend.removeFromLeft (12.0f);
    }

    // The time shown, bottom right.
    const auto spanText = ValueText::ms (span);
    g.setColour (Palette::inkFaint);
    g.setFont (Palette::font (10.0f));
    g.drawText (spanText, area.reduced (8.0f, 4.0f), juce::Justification::topRight);
}

void Display::paint (juce::Graphics& g)
{
    // The card.
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::card);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Palette::line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    auto area = bounds.reduced (16.0f, 14.0f);
    auto left = area.removeFromLeft (220.0f);
    area.removeFromLeft (14.0f);
    auto right = area.removeFromRight (360.0f);
    area.removeFromRight (14.0f);
    const auto graph = area;

    // Left: the selected pad, or Learn waiting for a key.
    {
        auto head = left.removeFromTop (15.0f);
        const auto learning = shownLearn >= 0;
        const auto channel = learning ? shownLearn : shownSelected;

        g.setColour (learning ? Palette::accent : Palette::soundColour (channel));
        g.fillEllipse (head.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f));
        head.removeFromLeft (10.0f);
        g.setColour (learning ? Palette::accent : Palette::inkSoft);
        g.setFont (Palette::font (12.0f, true, 0.08f));
        g.drawText ((learning ? "LEARN  " + dot + "  CHANNEL " : juce::String ("CHANNEL ")) + juce::String (channel + 1),
                    head, juce::Justification::centredLeft);

        left.removeFromTop (6.0f);
        g.setColour (Palette::accent);
        g.setFont (Palette::font (24.0f, true));
        g.drawFittedText (learning ? PadButton::title (processor, shownLearn) : shownName,
                          left.removeFromTop (29.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

        left.removeFromTop (4.0f);
        g.setColour (Palette::inkSoft);
        g.setFont (Palette::font (12.0f));
        g.drawFittedText (learning ? juce::String ("Play a key ...")
                                   : TbParams::noteName (shownNote) + " " + dot + " note " + juce::String (shownNote),
                          left.removeFromTop (15.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);

        left.removeFromTop (2.0f);
        g.setColour (Palette::inkFaint);
        g.setFont (Palette::font (11.0f));
        g.drawFittedText (learning ? juce::String ("Click LEARN NOTE again to stop") : shownText,
                          left.removeFromTop (14.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }

    // Middle: the envelopes, in a well.
    g.setColour (Palette::wave);
    g.fillRoundedRectangle (graph, 4.0f);
    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (graph.toNearestInt());
        paintEnvelopes (g, graph);
    }
    g.setColour (Palette::waveLine);
    g.drawRoundedRectangle (graph.reduced (0.5f), 4.0f, 1.0f);

    // Right: the browser, after an upright hairline.
    g.setColour (Palette::rackDivider);
    g.fillRect (right.removeFromLeft (1.0f));
    right.removeFromLeft (14.0f);

    // The library's groups.
    groupAreas.clearQuick();
    {
        auto row = right.removeFromTop (24.0f);
        const auto font = Palette::font (10.5f, true, 0.06f);

        if (groups.isEmpty())
        {
            g.setFont (font);
            g.setColour (Palette::inkFaint);
            g.drawText ("SOUNDS", row, juce::Justification::centredLeft);
        }

        for (int i = 0; i < groups.size(); ++i)
        {
            const auto title = groups.getReference (i).getFileName().toUpperCase();
            const auto width = std::ceil (widthOf (font, title)) + 18.0f;

            if (width > row.getWidth())
                break;   // more folders than room: the rest are there through Load Sound

            const auto tab = row.removeFromLeft (width);
            row.removeFromLeft (4.0f);
            groupAreas.add (tab);

            const auto on = i == browseGroup;
            g.setColour (on ? Palette::accent : Palette::button);
            g.fillRoundedRectangle (tab.reduced (0.5f), 3.0f);
            g.setColour (on ? Palette::accent : Palette::buttonLine);
            g.drawRoundedRectangle (tab.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (on ? Palette::onFill : Palette::inkDim);
            g.setFont (font);
            g.drawText (title, tab, juce::Justification::centred);
        }
    }

    right.removeFromTop (8.0f);

    // [<] [ the sound  v ] [>] - the name opens the list.
    const auto hasGroup = juce::isPositiveAndBelow (browseGroup, groups.size()) && groupSounds.size() > 0;
    {
        auto row = right.removeFromTop (30.0f);
        prevArea = row.removeFromLeft (30.0f);
        nextArea = row.removeFromRight (30.0f);
        row.removeFromLeft (6.0f);
        row.removeFromRight (6.0f);
        nameArea = row;

        for (const auto& [box, pointsLeft] : { std::pair<juce::Rectangle<float>, bool> { prevArea, true }, { nextArea, false } })
        {
            g.setColour (Palette::field);
            g.fillRoundedRectangle (box.reduced (0.5f), 3.0f);
            g.setColour (Palette::fieldLine);
            g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
            drawTriangle (g, box, pointsLeft, hasGroup ? Palette::accent : Palette::lightOff);
        }

        g.setColour (Palette::wave);
        g.fillRoundedRectangle (nameArea.reduced (0.5f), 3.0f);
        g.setColour (Palette::fieldEdge);
        g.drawRoundedRectangle (nameArea.reduced (0.5f), 3.0f, 1.0f);

        auto inner = nameArea.reduced (10.0f, 0.0f);
        {
            const auto c = inner.removeFromRight (12.0f).getCentre();
            juce::Path chevron;
            chevron.startNewSubPath (c.x - 3.0f, c.y - 1.5f);
            chevron.lineTo (c.x, c.y + 1.5f);
            chevron.lineTo (c.x + 3.0f, c.y - 1.5f);
            g.setColour (Palette::inkDim);
            g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        inner.removeFromRight (6.0f);

        const auto inGroup = soundIndex >= 0;
        const auto groupName = hasGroup ? groups[browseGroup].getFileName().toUpperCase() : juce::String();
        const auto soundText = inGroup ? groupSounds[soundIndex].getFileNameWithoutExtension()
                             : hasGroup ? "Choose " + groupName
                                        : juce::String ("No sounds");

        g.setColour (inGroup ? Palette::title : Palette::inkFaint);
        g.setFont (Palette::font (14.0f, true));
        g.drawFittedText (soundText, inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }

    right.removeFromTop (8.0f);

    // Where in the group, and the kit.
    {
        auto row = right.removeFromTop (15.0f);
        const auto groupName = hasGroup ? groups[browseGroup].getFileName().toUpperCase() : juce::String();
        const auto counter = hasGroup ? groupName + " " + dot + " " + (soundIndex >= 0 ? juce::String (soundIndex + 1) : juce::String ("-"))
                                            + " / " + juce::String (groupSounds.size())
                                      : juce::String ("Sounds/ is empty");

        const auto font = Palette::font (11.0f);
        g.setFont (font);
        g.setColour (Palette::inkFaint);
        const auto counterWidth = std::ceil (widthOf (font, counter)) + 12.0f;
        g.drawText (counter, row.removeFromLeft (counterWidth), juce::Justification::centredLeft);

        const auto kitText = shownKit.isNotEmpty() ? shownKit.toUpperCase() : juce::String ("UNTITLED KIT");
        const auto kitFont = Palette::font (11.0f, true);
        const auto kitWidth = juce::jmin (juce::jmax (0.0f, row.getWidth() - 30.0f), std::ceil (widthOf (kitFont, kitText)) + 2.0f);
        kitArea = row.removeFromRight (kitWidth);
        g.setFont (kitFont);
        g.setColour (shownKit.isNotEmpty() ? Palette::accent : Palette::inkFaint);
        g.drawFittedText (kitText, kitArea.toNearestInt(), juce::Justification::centredRight, 1, 0.7f);

        g.setFont (font);
        g.setColour (Palette::inkFaint);
        g.drawText ("KIT", row.removeFromRight (30.0f), juce::Justification::centredLeft);
    }
}
