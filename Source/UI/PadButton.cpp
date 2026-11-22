//
//  PadButton.cpp
//  Tonbox
//

#include "PadButton.h"
#include "Controls.h"
#include "Palette.h"
#include "PluginProcessor.h"
#include "ValueText.h"

namespace
{
    const juce::String dragPrefix ("tonbox-channel:");
}

PadButton::PadButton (TonboxProcessor& processorToUse, int channelIndex)
    : processor (processorToUse), channel (channelIndex)
{
    setRepaintsOnMouseActivity (true);
    lastHitCount = processor.getEngine().getHitCount (channel);
    shownNote = processor.getChannelNote (channel);
    shownName = title (processor, channel);
    shownText = describeShort (processor, channel);
    selected = processor.getSelectedChannel() == channel;
}

juce::String PadButton::describe (TonboxProcessor& processor, int channel)
{
    const auto s = processor.getChannelSettings (channel);

    if (s.mix > 50.0f)
        return TbParams::filterNames[s.filterMode].toUpperCase() + " NOISE " + ValueText::hz (s.filterHz);

    return TbParams::waveNames[s.wave].toUpperCase() + " " + ValueText::hz (s.pitchHz);
}

juce::String PadButton::describeShort (TonboxProcessor& processor, int channel)
{
    const auto s = processor.getChannelSettings (channel);

    if (s.mix > 50.0f)
        return TbParams::filterNames[s.filterMode] + " " + ValueText::hz (s.filterHz);

    return TbParams::waveNames[s.wave].toUpperCase() + " " + ValueText::hz (s.pitchHz);
}

juce::String PadButton::title (TonboxProcessor& processor, int channel)
{
    const auto name = processor.getChannelName (channel);
    return name.isNotEmpty() ? name : describe (processor, channel);
}

juce::String PadButton::dragDescription (int channel)
{
    return dragPrefix + juce::String (channel);
}

bool PadButton::parseDragDescription (const juce::var& description, int& channel)
{
    const auto text = description.toString();

    if (! text.startsWith (dragPrefix))
        return false;

    channel = text.fromFirstOccurrenceOf (dragPrefix, false, false).getIntValue();
    return channel >= 0 && channel < TbParams::numChannels;
}

void PadButton::setDropHover (bool hover)
{
    dropHover = hover;
    repaint();
}

void PadButton::updateFlash()
{
    const auto note = processor.getChannelNote (channel);
    const auto isSelected = processor.getSelectedChannel() == channel;
    const auto name = title (processor, channel);
    const auto text = describeShort (processor, channel);

    if (note != shownNote || isSelected != selected || name != shownName || text != shownText)
    {
        shownNote = note;
        selected = isSelected;
        shownName = name;
        shownText = text;
        repaint();
    }

    const auto hits = processor.getEngine().getHitCount (channel);

    if (hits != lastHitCount)
    {
        lastHitCount = hits;
        flash = 0.35f + 0.65f * processor.getEngine().getLastVelocity (channel);
        repaint();
    }
    else if (flash > 0.0f)
    {
        flash = flash < 0.02f ? 0.0f : flash * 0.78f;
        repaint();
    }
}

void PadButton::mouseDown (const juce::MouseEvent& event)
{
    dragStarted = false;
    processor.setSelectedChannel (channel);

    if (event.mods.isPopupMenu())
    {
        showMenu();
        return;
    }

    // While Learn waits, a click on another pad hands the wait over to it.
    if (processor.getLearnChannel() >= 0)
        processor.startLearn (channel);

    const auto height = juce::jmax (1, getHeight());
    const auto velocity = 1.0f - 0.67f * juce::jlimit (0.0f, 1.0f, (float) event.y / (float) height);
    processor.playChannel (channel, velocity);
    updateFlash();
}

void PadButton::mouseDrag (const juce::MouseEvent& event)
{
    if (dragStarted || event.mods.isPopupMenu() || event.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        const auto image = createComponentSnapshot (getLocalBounds()).convertedToFormat (juce::Image::ARGB);
        container->startDragging (dragDescription (channel), this, juce::ScaledImage (image), false);
    }
}

void PadButton::mouseUp (const juce::MouseEvent&)
{
    dragStarted = false;
}

bool PadButton::isInterestedInDragSource (const SourceDetails& details)
{
    int source = -1;
    return parseDragDescription (details.description, source) && source != channel;
}

void PadButton::itemDropped (const SourceDetails& details)
{
    setDropHover (false);

    int source = -1;

    if (parseDragDescription (details.description, source))
        processor.swapChannels (source, channel);
}

void PadButton::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem (3, "Copy Sound");
    menu.addItem (4, "Paste Sound", TonboxProcessor::isCopiedSound (juce::SystemClipboard::getTextFromClipboard()));
    menu.addSeparator();
    menu.addItem (1, "Rename...");
    menu.addItem (2, "Clear Name", processor.getChannelName (channel).isNotEmpty());

    menu.showMenuAsync (panelMenu (*this),
                        [safe = juce::Component::SafePointer<PadButton> (this)] (int result)
    {
        if (safe == nullptr)
            return;

        auto& p = safe->processor;
        const auto c = safe->channel;

        if (result == 2)
            p.setChannelName (c, {});

        // The system clipboard, so a sound also travels between Tonbox
        // instances and projects.
        if (result == 3)
            juce::SystemClipboard::copyTextToClipboard (p.copyChannel (c));

        if (result == 4)
            p.pasteChannel (c, juce::SystemClipboard::getTextFromClipboard());

        if (result == 1)
            safe->rename.open (*safe, "Rename Channel " + juce::String (c + 1), p.getChannelName (c),
                               [&p, c] (const juce::String& name) { p.setChannelName (c, name); });
    });
}

void PadButton::paint (juce::Graphics& g)
{
    const auto face = getLocalBounds().toFloat().reduced ((float) ringMargin);
    constexpr float radius = 3.0f;

    // The selected pad: an orange ring outside, a white one inside.
    if (selected)
    {
        g.setColour (Palette::accent);
        g.fillRoundedRectangle (face.expanded ((float) ringMargin), radius + 2.0f);
    }

    auto colour = Palette::soundColour (channel);
    if (isMouseOver())
        colour = colour.brighter (0.06f);
    colour = colour.interpolatedWith (juce::Colours::white, flash * 0.45f);

    juce::Path shape;
    shape.addRoundedRectangle (face, radius);

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (shape);

        g.setColour (colour);
        g.fillRect (face);

        // The bar: the colour a fifth towards white, a faint line under it.
        const auto bar = face.withHeight (28.0f);
        g.setColour (colour.interpolatedWith (juce::Colours::white, 0.22f));
        g.fillRect (bar);
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillRect (bar.withTop (bar.getBottom() - 1.0f));

        if (selected || dropHover)
        {
            g.setColour (Palette::padRing);
            g.drawRoundedRectangle (face.reduced (2.0f), radius, 2.0f);
        }
    }

    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (face.reduced (0.5f), radius, 1.0f);

    const auto ink = Palette::padInk;

    // The bar: the number and the sound's name.
    {
        auto bar = face.withHeight (27.0f).reduced (10.0f, 0.0f);
        const auto number = juce::String (channel + 1);
        const auto numberFont = Palette::font (14.0f, true);
        g.setFont (numberFont);
        g.setColour (ink.withAlpha (0.8f));
        g.drawText (number, bar.removeFromLeft (juce::GlyphArrangement::getStringWidth (numberFont, number) + 1.0f),
                    juce::Justification::centredLeft);
        bar.removeFromLeft (8.0f);

        g.setColour (ink);
        g.setFont (Palette::font (12.5f, true, 0.04f));
        g.drawFittedText (shownName.toUpperCase(), bar.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    }

    // Under it: the note, and what the sound is.
    {
        auto row = face.withTrimmedTop (28.0f).reduced (10.0f, 0.0f);
        g.setColour (ink.withAlpha (0.78f));
        g.setFont (Palette::font (11.0f, true));

        const auto note = TbParams::noteName (shownNote) + " " + juce::String (shownNote);
        const auto noteWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), note) + 2.0f;
        g.drawText (note, row.removeFromLeft (noteWidth), juce::Justification::centredLeft);

        if (processor.getChannelName (channel).isNotEmpty())
        {
            row.removeFromLeft (6.0f);
            g.drawFittedText (shownText, row.toNearestInt(), juce::Justification::centredRight, 1, 0.75f);
        }
    }
}
