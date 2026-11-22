//
//  ChoiceButtons.cpp
//  Tonbox (copied from Rackbox Plus)
//

#include "ChoiceButtons.h"
#include "Palette.h"

void ChoiceButtons::setLabels (const juce::StringArray& newLabels)
{
    if (newLabels == labels)
        return;

    labels = newLabels;
    hovered = -1;
    repaint();
}

void ChoiceButtons::setSelected (int index)
{
    if (index == selected)
        return;

    selected = index;
    repaint();
}

juce::String ChoiceButtons::getSelectedText() const
{
    return juce::isPositiveAndBelow (selected, labels.size()) ? labels[selected] : juce::String();
}

juce::Rectangle<float> ChoiceButtons::buttonBounds (int index) const
{
    const auto count = juce::jmax (1, labels.size());
    const auto bounds = getLocalBounds().toFloat();
    const auto width = (bounds.getWidth() - gap * (float) (count - 1)) / (float) count;

    return { bounds.getX() + (float) index * (width + gap), bounds.getY(), width, bounds.getHeight() };
}

int ChoiceButtons::indexAt (juce::Point<int> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (buttonBounds (i).contains (p.toFloat()))
            return i;

    return -1;
}

void ChoiceButtons::paint (juce::Graphics& g)
{
    const auto colour = Palette::zone (zone).line;

    for (int i = 0; i < labels.size(); ++i)
    {
        const auto bounds = buttonBounds (i).reduced (0.5f);
        const auto lit = i == selected;
        const auto face = lit ? colour : (i == hovered ? Palette::button.brighter (0.07f) : Palette::button);

        g.setColour (face);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (lit ? colour : Palette::buttonLine);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        g.setColour (lit ? juce::Colour (0xff141416) : Palette::segInk);
        g.setFont (Palette::font (10.5f, true, 0.04f));
        g.drawFittedText (labels[i].toUpperCase(), bounds.reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void ChoiceButtons::mouseDown (const juce::MouseEvent& e)
{
    const auto index = indexAt (e.getPosition());

    if (index >= 0 && onSelect)
        onSelect (index);
}

void ChoiceButtons::mouseMove (const juce::MouseEvent& e)
{
    const auto index = indexAt (e.getPosition());

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void ChoiceButtons::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}
