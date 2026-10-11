// Onirique ResoBox — shared look, matching the Onirique identity
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace onirique::theme
{

using juce::Colour;

// The palette the rest of the Onirique range uses: a near-black ground, a
// blood-dark panel, and a single ember accent that carries every active state.
inline const Colour night   { 0xff0d0809 };
inline const Colour deep    { 0xff1a080a };
inline const Colour deep2   { 0xff2a0a0e };
inline const Colour blood   { 0xff5c0008 };
inline const Colour ember   { 0xffe23444 };
inline const Colour emberLit{ 0xffff5a68 };
inline const Colour mist    { 0xfff6eeee };
inline const Colour bone    { 0xffffecee };
inline const Colour haze    { 0xffa88f92 };
inline const Colour line    { 0x21ffd2d6 };

/** A text written in the sources with its accents (the sources are UTF-8). */
inline juce::String fr (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::Font displayFont (float height, bool heavy = true)
{
    // Named outright: the "default sans-serif" placeholder does not survive
    // being put in FontOptions, and what comes out the other end is a serif.
   #if JUCE_WINDOWS
    const juce::String family ("Segoe UI");
   #elif JUCE_MAC
    const juce::String family ("Helvetica Neue");
   #else
    const juce::String family ("DejaVu Sans");
   #endif

    return juce::Font (juce::FontOptions (family, height, heavy ? juce::Font::bold : juce::Font::plain));
}

/** The wide, heavy capitals used for every control name across the range. */
inline juce::Font capsFont (float height)
{
    return displayFont (height).withExtraKerningFactor (0.14f);
}

inline juce::Font monoFont (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height,
                                          juce::Font::plain));
}

/** The ember-arc knob and segmented controls used across the range. */
class OniriqueLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    OniriqueLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool,
                       int, int, int, int, juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getLabelFont (juce::Label&) override;

    // Drop-down menus: a dark red card, a lit dot beside the current choice.
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int width, int height,
                                             const juce::PopupMenu::Options&) override;
    void drawPopupMenuItemWithOptions (juce::Graphics&, const juce::Rectangle<int>& area, bool highlighted,
                                       const juce::PopupMenu::Item&, const juce::PopupMenu::Options&) override;
    void drawPopupMenuSectionHeaderWithOptions (juce::Graphics&, const juce::Rectangle<int>& area,
                                                const juce::String& name, const juce::PopupMenu::Options&) override;
    void getIdealPopupMenuItemSizeWithOptions (const juce::String& text, bool isSeparator, int standardHeight,
                                               int& idealWidth, int& idealHeight,
                                               const juce::PopupMenu::Options&) override;
    int getPopupMenuBorderSizeWithOptions (const juce::PopupMenu::Options&) override;
    int getMenuWindowFlags() override;

    /** A label carrying the "pill" property is drawn as the value readout that
        sits under every knob of the range: a dark capsule with its LED. */
    void drawLabel (juce::Graphics&, juce::Label&) override;

    // The short description that appears when the pointer rests on a control.
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};

} // namespace onirique::theme
