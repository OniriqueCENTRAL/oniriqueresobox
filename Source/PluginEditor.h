// Onirique ResoBox — editor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseOverlay.h"
#include "PluginProcessor.h"
#include "ui/Controls.h"
#include "ui/SpectrumDisplay.h"
#include "ui/Theme.h"

namespace onirique
{

/** Four zones, each with one job: the header is which settings are loaded,
    the column on the left is how the suppressor behaves, the display is where
    it works, and the foot is what becomes of the result. */
class ResoBoxEditor final : public AudioProcessorEditor,
                            private Timer
{
public:
    explicit ResoBoxEditor (ResoBoxProcessor&);
    ~ResoBoxEditor() override;

    void paint (Graphics&) override;
    void resized() override;

    SpectrumDisplay& getDisplay() noexcept { return display; }

    /** Shows, and keeps, the description of whatever is at a point, without
        the wait: for looking at one from the snapshot tool. */
    void pinDescription (Point<int> at);

    /** Turns the preset menu into the field its new name is typed in. */
    void beginSaving();

private:
    using ButtonAttachment = AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void paintHeader (Graphics&, Rectangle<int> header);
    int titleRight() const;

    //==============================================================================
    /** The description of whatever the pointer is resting on.

        Shown only once the pointer has been still on a control for two full
        seconds. It then stays for as long as the pointer is on that control,
        moving or not, and goes when it leaves. A control here can be several
        components - a knob, its name and its readout all say the same thing -
        and the display answers for everything inside it. */
    struct HoverTip final : Component
    {
        void paint (Graphics& g) override
        {
            getLookAndFeel().drawTooltip (g, text, getWidth(), getHeight());
        }

        String text;
    };

    struct Described
    {
        Component* component;
        String text;
    };

    void describe (std::initializer_list<Component*>, const String& text);
    String descriptionAt (Point<int>) const;
    void updateHoverTip();

    std::vector<Described> described;
    HoverTip hoverTip;
    String restingOn;
    Point<int> restingAt;
    uint32 restingSince = 0;
    bool pinned = false;

    /** Fills the preset menu from what is built in and what is on disk. */
    void refreshPresets();
    void stepPreset (int direction);
    void deleteClicked();

    ResoBoxProcessor& processor;

    SpectrumDisplay display;

    // The header: presets, the two slots, and the two ways of listening.
    TextButton previousButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, deleteButton { "DELETE" };
    ComboBox presetBox;
    Label presetNameEditor;
    StringArray presetNames;
    int stockCount = 0;
    int deleteArmed = 0;    // ticks left in which a second click deletes

    TextButton slotAButton { "A" }, slotBButton { "B" }, copyButton;
    TextButton deltaButton { "DELTA" }, bypassButton { "BYPASS" };

    // The column.
    TextButton softButton { "SOFT" }, hardButton { "HARD" };
    std::unique_ptr<ParameterAttachment> modeAttachment;
    Knob depth { "DEPTH" }, detail { "DETAIL" }, tilt { "TILT" }, attack { "ATTACK" }, release { "RELEASE" };

    // The foot.
    ValueField mix { "MIX" }, output { "OUT" }, link { "LINK" }, balance { "BAL" };
    ComboBox stereoBox, qualityBox;
    TextButton autoGainButton { "AUTO GAIN" }, sidechainButton { "SIDECHAIN" }, lowLatencyButton { "LOW LATENCY" };

    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::vector<std::unique_ptr<ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;

    /** The sigil with its ember glow already baked in, at twice the size it is
        drawn so it stays sharp on a scaled display. */
    Image logo;

    /** When the editor opened: the light that runs along the foot of the
        header, and the stars beside the name, keep time from here. */
    double openedAt = Time::getMillisecondCounterHiRes();

    // Last, so that it is destroyed first and drawn over everything else.
    std::unique_ptr<license::Overlay> licenseOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResoBoxEditor)
};

} // namespace onirique
