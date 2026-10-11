// Onirique ResoBox — the two kinds of control every panel is made of
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

namespace onirique
{

/** A dial with its name above and its value below. The value can be clicked
    and typed into, for the times a number is known and dragging to it is not
    the quickest way there. */
class Knob final : public Component
{
public:
    explicit Knob (const String& titleToUse) : title (titleToUse)
    {
        slider.setSliderStyle (Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { refresh(); };
        addAndMakeVisible (slider);

        name.setText (title, dontSendNotification);
        name.setJustificationType (Justification::centred);
        name.setColour (Label::textColourId, theme::mist);
        name.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (name);

        value.getProperties().set ("pill", true);
        value.setJustificationType (Justification::centred);
        value.setEditable (true);
        value.onTextChange = [this]
        {
            slider.setValue (slider.getValueFromText (value.getText()), sendNotificationSync);
            refresh();
        };
        addAndMakeVisible (value);

        // For the compact form, where the name is also where the value shows.
        slider.addMouseListener (this, false);

        name.onEditorShow = [this]
        {
            if (auto* editor = name.getCurrentTextEditor())
            {
                editor->setJustification (Justification::centred);
                editor->setText (slider.getTextFromValue (slider.getValue()), false);
                editor->selectAll();
            }
        };

        name.onTextChange = [this]
        {
            if (compact && name.getText() != title)
                slider.setValue (slider.getValueFromText (name.getText()), sendNotificationSync);

            updateText();
        };

        name.onEditorHide = [this] { updateText(); };
    }

    /** No capsule: the name gives way to the value for as long as the dial
        is under the pointer or being turned, and a double click on it lets
        the value be typed. */
    void setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        nameBelow = nameBelow || compact;

        value.setVisible (! compact);
        name.setInterceptsMouseClicks (compact, compact);
        name.setEditable (false, compact);
        resized();
        updateText();
    }

    void mouseEnter (const MouseEvent&) override { updateText(); }
    void mouseExit (const MouseEvent&) override  { updateText(); }
    void mouseUp (const MouseEvent&) override    { updateText(); }

    void attach (AudioProcessorValueTreeState& state, const String& parameterId)
    {
        attachment.reset();
        attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
        refresh();
    }

    void detach() { attachment.reset(); }

    /** Makes the dial speak a parameter's language without following it: the
        dial runs from 0 to 1 as the parameter does, shows the parameter's
        text and reads it back. Whoever calls this moves the parameter. */
    void speak (RangedAudioParameter& parameter, bool bipolar = false)
    {
        attachment.reset();

        slider.setRange (0.0, 1.0);
        slider.textFromValueFunction = [&parameter] (double v) { return parameter.getText ((float) v, 0); };
        slider.valueFromTextFunction = [&parameter] (const String& t) { return (double) parameter.getValueForText (t); };
        slider.setDoubleClickReturnValue (true, parameter.getDefaultValue());

        if (bipolar)
            slider.getProperties().set ("bipolar", true);
        else
            slider.getProperties().remove ("bipolar");
    }

    void updateText()
    {
        const auto text = slider.getTextFromValue (slider.getValue());

        value.setText (text, dontSendNotification);

        if (compact && ! name.isBeingEdited())
            name.setText (slider.isMouseOverOrDragging() ? text : title, dontSendNotification);
    }

    /** The name under the dial instead of over it. */
    void setNameBelow (bool below) { nameBelow = below; resized(); }

    /** Takes some off the dial all round, to make it the smaller of a set. */
    void setDialInset (int inset) { dialInset = inset; resized(); }

    /** Written small at the two ends of the arc. */
    void setEnds (const String& low, const String& high)
    {
        slider.getProperties().set ("low", low);
        slider.getProperties().set ("high", high);
    }

    void setAccent (Colour colour)
    {
        slider.getProperties().set ("accent", (int64) colour.getARGB());
        slider.repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (! nameBelow)
            name.setBounds (area.removeFromTop (15));

        if (! compact)
            value.setBounds (area.removeFromBottom (20).withSizeKeepingCentre (jmin (92, getWidth() - 6), 20));

        if (nameBelow)
        {
            area.removeFromBottom (2);
            name.setBounds (area.removeFromBottom (14));
        }

        slider.setBounds (area.reduced (dialInset));
    }

    Slider slider;
    Label name, value;

private:
    void refresh() { updateText(); }

    String title;
    bool nameBelow = false, compact = false;
    int dialInset = 0;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A value in a capsule with its name beside it, dragged up and down: the
    controls of the foot of the window, which are set once and left. */
class ValueField final : public Slider
{
public:
    explicit ValueField (const String& name) : title (name)
    {
        setSliderStyle (Slider::RotaryVerticalDrag);
        setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        setMouseDragSensitivity (220);
    }

    void paint (Graphics& g) override
    {
        using namespace theme;

        auto bounds = getLocalBounds().toFloat();
        const auto font = capsFont (10.0f);
        const auto titleWidth = GlyphArrangement::getStringWidth (font, title) + 10.0f;

        g.setColour (haze);
        g.setFont (font);
        g.drawText (title, bounds.removeFromLeft (titleWidth), Justification::centredLeft, false);

        const auto pill = bounds.reduced (0.5f, 3.5f);
        const auto radius = pill.getHeight() * 0.5f;

        g.setColour (Colour (0x73000000));
        g.fillRoundedRectangle (pill, radius);
        g.setColour (isMouseOverOrDragging() ? ember.withAlpha (0.7f) : Colour (0x33ffd2d6));
        g.drawRoundedRectangle (pill, radius, 1.0f);

        g.setColour (mist);
        g.setFont (monoFont (11.0f).boldened());
        g.drawText (getTextFromValue (getValue()), pill, Justification::centred, false);
    }

    void mouseEnter (const MouseEvent& e) override { Slider::mouseEnter (e); repaint(); }
    void mouseExit (const MouseEvent& e) override  { Slider::mouseExit (e); repaint(); }

private:
    String title;
};

} // namespace onirique
