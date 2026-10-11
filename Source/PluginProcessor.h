// Onirique ResoBox — plugin entry point
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "License/LicenseGuard.h"
#include "dsp/ResonanceSuppressor.h"

namespace onirique
{

using namespace juce;

/** The nodes of the sensitivity curve, in the order the editor numbers them:
    the bells, then the two cuts. */
constexpr int kLowCutNode = rb::kNumBands;
constexpr int kHighCutNode = rb::kNumBands + 1;
constexpr int kNumNodes = rb::kNumBands + 2;

/** Parameter identifiers for one node. A bell that is not `used` is not on the
    curve at all; one that is used but not `on` is bypassed and keeps its
    place. used, gain, q, shape and place are empty for the cuts, which are always there. */
struct NodeIds { String used, on, freq, gain, q, shape, place; };
NodeIds nodeIds (int node);

/** The plugin instance: parameters in, a block through the suppressor, and a
    copy of what it saw for the editor to draw. It also keeps what is not a
    parameter: the two settings being compared, and the presets. */
class ResoBoxProcessor final : public AudioProcessor,
                               private AsyncUpdater
{
public:
    ResoBoxProcessor();
    ~ResoBoxProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (AudioBuffer<float>&, MidiBuffer&) override;

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override     { return false; }
    bool producesMidi() const override    { return false; }
    bool isMidiEffect() const override    { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // The plugin's own bypass keeps the latency, so switching it in a host
    // does not shift the track.
    AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    int getNumPrograms() override            { return 1; }
    int getCurrentProgram() override         { return 0; }
    void setCurrentProgram (int) override    {}
    const String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** The settings as the parameters stand now. */
    rb::Settings readSettings() const noexcept;

    /** The last frame the suppressor worked on, one value per FFT bin, for the
        display. Returns false until there has been one. */
    bool getVisual (std::vector<float>& inputDb, std::vector<float>& outputDb,
                    std::vector<float>& reductionDb, double& binHz);

    //==============================================================================
    // A and B: two settings held side by side. Everything below is for the
    // message thread.
    int getActiveSlot() const noexcept { return activeSlot; }

    /** Puts the current settings away and brings the other ones back. */
    void switchToSlot (int slot);

    /** Makes the other slot the same as the one being heard. */
    void copyToOtherSlot();

    //==============================================================================
    // Presets. The stock ones are built in; the user's are files, one each.
    StringArray getStockPresets() const;
    StringArray getUserPresets() const;
    String getPresetName() const;
    bool isUserPreset (const String& name) const;

    void loadPreset (const String& name);

    /** Writes the current settings under a name, and returns the name it was
        given: a stock preset's name cannot be taken. */
    String saveUserPreset (const String& name);
    void deleteUserPreset (const String& name);

    static File getUserPresetFolder();

    AudioProcessorValueTreeState apvts;

private:
    void handleAsyncUpdate() override;

    /** The values of everything a preset or a slot holds, by parameter. */
    ValueTree capture() const;
    void apply (const ValueTree&);
    void setPresetName (const String&);

    rb::ResonanceSuppressor suppressor;

    struct Raw;
    std::unique_ptr<Raw> raw;
    AudioProcessorParameter* bypassParameter = nullptr;

    std::atomic<int> reportedFrame { -1 };

    CriticalSection slotLock;
    ValueTree slots[2];
    int activeSlot = 0;

    SpinLock visualLock;
    std::vector<float> visualIn, visualOut, visualRed;
    int visualBins = 0;
    double visualBinHz = 0.0;
    uint32_t visualFrame = 0;

public:
    /** The ONIRIQUE licence of this plugin on this machine: bought, on trial, or
        neither. See License/LicenseGuard.h. */
    license::Guard license { "onirique-resobox" };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResoBoxProcessor)
};

} // namespace onirique
