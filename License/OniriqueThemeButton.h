#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OniriqueTheme.h"

//==============================================================================
//  Le bouton ⋯ des plugins ONIRIQUE qui n'ont pas la barre commune (OniriqueBar.h) : il ouvre INTERFACE COLOUR, les mêmes
//  8 teintes que le menu de la barre. Le choix est gardé pour ce plugin (OniriqueTheme.h) ; la fenêtre se redessine
//  dans son minuteur (onirique::look::changedSince).
//==============================================================================
namespace onirique::look
{
class ThemeButton final : public juce::Button
{
public:
    ThemeButton() : juce::Button ("Interface colour") { setTooltip ("Interface colour"); setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float rad = r.getHeight() * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r, rad);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.015f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (over || down ? emberLit.withAlpha (0.6f) : juce::Colour (0xffffebee).withAlpha (0.12f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        const float d = juce::jmax (2.0f, r.getHeight() * 0.11f);
        g.setColour ((over || down) ? juce::Colours::white : juce::Colour (0xffd9c6c9));
        for (int i = -1; i <= 1; ++i)
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre ({ r.getCentreX() + (float) i * d * 2.4f, r.getCentreY() }));
    }

    void clicked() override
    {
        juce::PopupMenu m;
        m.addSectionHeader ("INTERFACE COLOUR");
        const auto cur = ember.getARGB() | 0xff000000u;
        for (int i = 0; i < numSwatches; ++i)
        {
            juce::PopupMenu::Item it (swatches[i].name);
            it.itemID = i + 1;
            it.colour = juce::Colour (swatches[i].argb);
            it.isTicked = (swatches[i].argb | 0xff000000u) == cur;
            m.addItem (it);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [] (int r) { if (r > 0 && r <= numSwatches) choose (swatches[r - 1].argb); });
    }
};
} // namespace onirique::look
