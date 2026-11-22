//
//  Controls.cpp
//  Tonbox
//

#include "Controls.h"

namespace
{
    void drawChevron (juce::Graphics& g, juce::Point<float> centre, float size, juce::Colour colour)
    {
        juce::Path chevron;
        chevron.startNewSubPath (centre.x - size * 0.5f, centre.y - size * 0.25f);
        chevron.lineTo (centre.x, centre.y + size * 0.25f);
        chevron.lineTo (centre.x + size * 0.5f, centre.y - size * 0.25f);
        g.setColour (colour);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

PanelButton::PanelButton (const juce::String& text, const juce::String& iconPath, Style styleToUse)
    : juce::Button (text), style (styleToUse)
{
    setIcon (iconPath);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PanelButton::setIcon (const juce::String& iconPath)
{
    stroke = iconPath.isNotEmpty() ? juce::Drawable::parseSVGPath (iconPath) : juce::Path();
    repaint();
}

void PanelButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto lit = active || down;
    constexpr float radius = 5.0f;

    g.setColour (highlighted || down ? Palette::button.brighter (0.06f) : Palette::button);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (lit ? Palette::accent : Palette::buttonLine);
    g.drawRoundedRectangle (bounds, radius, 1.0f);

    if (style == Style::field)
    {
        auto inner = bounds.withTrimmedLeft (14.0f).withTrimmedRight (12.0f);
        drawChevron (g, inner.removeFromRight (12.0f).getCentre(), 6.0f, Palette::inkDim);
        inner.removeFromRight (8.0f);

        // The name, then the tag framed after it.
        const auto nameFont = Palette::font (14.0f, true);
        const auto tagFont = Palette::font (10.0f, true, 0.1f);
        const auto tagText = tag.toUpperCase();
        const auto tagWidth = tag.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (tagFont, tagText) + 12.0f;
        const auto nameWidth = juce::jmin (juce::GlyphArrangement::getStringWidth (nameFont, getButtonText()) + 2.0f,
                                           inner.getWidth() - (tagWidth > 0.0f ? tagWidth + 10.0f : 0.0f));

        g.setColour (Palette::title);
        g.setFont (nameFont);
        g.drawFittedText (getButtonText(), inner.removeFromLeft (nameWidth).toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);

        if (tagWidth > 0.0f)
        {
            inner.removeFromLeft (10.0f);
            const auto column = inner.removeFromLeft (juce::jmin (tagWidth, inner.getWidth()));
            const auto frame = column.withSizeKeepingCentre (column.getWidth(), 18.0f);
            g.setColour (Palette::tagLine);
            g.drawRoundedRectangle (frame.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (Palette::inkDim);
            g.setFont (tagFont);
            g.drawFittedText (tagText, frame.toNearestInt(), juce::Justification::centred, 1, 0.8f);
        }
        return;
    }

    if (style == Style::column)
    {
        g.setColour (lit ? Palette::accent : Palette::buttonText);
        g.setFont (Palette::font (11.0f, true, 0.08f));
        g.drawText (getButtonText().toUpperCase(), bounds, juce::Justification::centred);
        return;
    }

    const auto hasText = getButtonText().isNotEmpty();
    const auto font = Palette::label (12.0f);
    const auto textWidth = hasText ? juce::GlyphArrangement::getStringWidth (font, getButtonText().toUpperCase()) : 0.0f;
    const auto hasIcon = ! stroke.isEmpty();
    const auto contentWidth = (hasIcon ? 16.0f : 0.0f) + (hasIcon && hasText ? 8.0f : 0.0f) + textWidth;
    auto content = bounds.withSizeKeepingCentre (contentWidth, bounds.getHeight());

    if (hasIcon)
    {
        const auto iconArea = content.removeFromLeft (16.0f).withSizeKeepingCentre (16.0f, 16.0f);
        g.setColour (lit ? Palette::accent : Palette::icon);
        g.strokePath (stroke, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (iconArea.getX(), iconArea.getY()));
        content.removeFromLeft (8.0f);
    }

    if (hasText)
    {
        g.setColour (lit ? Palette::accent : Palette::buttonText);
        g.setFont (font);
        g.drawText (getButtonText().toUpperCase(), content, juce::Justification::centredLeft);
    }
}
