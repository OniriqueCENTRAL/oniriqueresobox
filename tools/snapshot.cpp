// Renders the editor to a PNG without a host, with a ringing signal running
// through it, so a change to the interface can be looked at straight from the
// build.
//
//   resobox_snapshot out.png [width height]
//   resobox_snapshot out.png --states    several nodes selected, a preset being named
//   resobox_snapshot out.png --tip x y   the description shown for the control at that point
//   resobox_snapshot --selftest      the slots, the presets and the saved state, without a window
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "TestSignal.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

/** Sets the plugin up, plays into it on the message thread one timer tick at
    a time so the display can follow, then takes the picture and ends the
    program. */
struct Director final : juce::Timer
{
    Director (juce::File destination, int width, int height, bool states = false, juce::Point<int> tip = { -1, -1 })
        : file (std::move (destination)), showStates (states), tipAt (tip)
    {
        processor.reset (static_cast<onirique::ResoBoxProcessor*> (createPluginFilter()));
        processor->setPlayConfigDetails (2, 2, kSr, kBlock);
        processor->prepareToPlay (kSr, kBlock);

        set ("depth", 6.0f);
        set ("b2_gain", -5.0f);
        set ("b2_freq", 620.0f);
        set ("b3_gain", 7.5f);
        set ("b3_freq", 3300.0f);
        set ("b3_q", 0.8f);
        set ("b4_used", 1.0f);
        set ("b4_freq", 9000.0f);
        set ("b4_gain", 3.0f);
        set ("b4_place", 3.0f);
        set ("b2_place", 1.0f);
        set ("lc_freq", 110.0f);
        set ("hc_on", 1.0f);
        set ("hc_freq", 15000.0f);

        signal = rbtest::makeResonantNoise (4.0, kSr, { { 240.0, 12.0, 2.5f }, { 1150.0, 20.0, 3.0f },
                                                         { 2900.0, 25.0, 4.0f }, { 3700.0, 30.0, 2.5f },
                                                         { 7200.0, 30.0, 3.0f } });

        editor.reset (processor->createEditor());
        editor->setSize (width, height);

        if (auto* resoBox = dynamic_cast<onirique::ResoBoxEditor*> (editor.get()))
        {
            resoBox->getDisplay().select (2);

            if (showStates)
            {
                processor->loadPreset ("Piano Boxiness");
                resoBox->getDisplay().select (0);
                resoBox->getDisplay().setSelected (1, true);
                resoBox->beginSaving();
            }
        }

        startTimer (30);
    }

    void set (const juce::String& id, float value)
    {
        if (auto* p = processor->apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void timerCallback() override
    {
        juce::AudioBuffer<float> buffer (2, kBlock);
        juce::MidiBuffer midi;

        // About a tick's worth of audio per tick.
        for (int i = 0; i < 3 && position + kBlock <= signal.size(); ++i)
        {
            buffer.copyFrom (0, 0, signal.data() + position, kBlock);
            buffer.copyFrom (1, 0, signal.data() + position, kBlock);
            processor->processBlock (buffer, midi);
            position += kBlock;
        }

        if (++ticks > 60)
            finish();
    }

    void finish()
    {
        stopTimer();

        if (tipAt.x >= 0)
            if (auto* resoBox = dynamic_cast<onirique::ResoBoxEditor*> (editor.get()))
                resoBox->pinDescription (tipAt);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

        file.deleteFile();

        if (auto stream = file.createOutputStream())
            juce::PNGImageFormat().writeImageToStream (image, *stream);

        juce::MessageManager::getInstance()->stopDispatchLoop();
    }

    juce::File file;
    bool showStates = false;
    juce::Point<int> tipAt;
    std::unique_ptr<onirique::ResoBoxProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::vector<float> signal;
    size_t position = 0;
    int ticks = 0;
};
}

/** What the processor keeps that is not audio: the two slots, the presets,
    and the state a host saves. Returns the number of things found wrong. */
int selfTest()
{
    int failures = 0;

    auto check = [&failures] (bool ok, const char* what)
    {
        std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);
        failures += ok ? 0 : 1;
    };

    std::unique_ptr<onirique::ResoBoxProcessor> processor (static_cast<onirique::ResoBoxProcessor*> (createPluginFilter()));

    auto get = [&] (const char* id) { return processor->apvts.getRawParameterValue (id)->load(); };
    auto set = [&] (const char* id, float value)
    {
        auto* p = processor->apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };

    // A and B.
    set ("depth", 3.0f);
    processor->switchToSlot (1);
    check (std::abs (get ("depth") - 3.0f) < 0.01f, "B starts as a copy of A");
    set ("depth", 8.0f);
    set ("b4_used", 1.0f);
    processor->switchToSlot (0);
    check (std::abs (get ("depth") - 3.0f) < 0.01f && get ("b4_used") < 0.5f, "back on A, A is as it was left");
    processor->switchToSlot (1);
    check (std::abs (get ("depth") - 8.0f) < 0.01f && get ("b4_used") > 0.5f, "and B as it was left, added band included");
    processor->copyToOtherSlot();
    processor->switchToSlot (0);
    check (std::abs (get ("depth") - 8.0f) < 0.01f, "B > A makes A the same as B");

    set ("bypass", 1.0f);
    set ("depth", 2.0f);
    processor->switchToSlot (1);
    check (get ("bypass") > 0.5f, "switching slots leaves bypass alone");
    set ("bypass", 0.0f);

    // The saved state carries both slots.
    juce::MemoryBlock saved;
    processor->getStateInformation (saved);

    std::unique_ptr<onirique::ResoBoxProcessor> second (static_cast<onirique::ResoBoxProcessor*> (createPluginFilter()));
    second->setStateInformation (saved.getData(), (int) saved.getSize());

    auto get2 = [&] (const char* id) { return second->apvts.getRawParameterValue (id)->load(); };
    check (second->getActiveSlot() == 1 && std::abs (get2 ("depth") - 8.0f) < 0.01f, "a restored state is on the same slot, with its settings");
    second->switchToSlot (0);
    check (std::abs (get2 ("depth") - 2.0f) < 0.01f, "and has the other slot too");

    // The sidechain, through the plugin as a host drives it: four channels
    // in, the last two being the key.
    {
        std::unique_ptr<onirique::ResoBoxProcessor> keyed (static_cast<onirique::ResoBoxProcessor*> (createPluginFilter()));

        const auto* keyBus = keyed->getBus (true, 1);
        check (keyBus != nullptr && keyBus->isEnabled() && keyBus->getNumberOfChannels() == 2,
               "the sidechain input is there, and switched on, without the host having to ask");

        auto setKeyed = [&] (const char* id, float value)
        {
            auto* p = keyed->apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (value));
        };

        setKeyed ("depth", 10.0f);
        setKeyed ("lc_on", 0.0f);

        const auto quiet = rbtest::makeNoise (2.0, kSr, 0.1f);
        const auto ringing = rbtest::makeResonantNoise (2.0, kSr, { { 3000.0, 20.0, 4.0f } });

        // What is taken out of plain noise, in dB below the noise itself.
        auto removed = [&] (bool sidechainOn, bool keyPlaying)
        {
            setKeyed ("sidechain", sidechainOn ? 1.0f : 0.0f);
            keyed->setRateAndBufferSizeDetails (kSr, kBlock);
            keyed->prepareToPlay (kSr, kBlock);

            const auto latency = (size_t) keyed->getLatencySamples();
            juce::AudioBuffer<float> buffer (keyed->getTotalNumInputChannels(), kBlock);
            juce::MidiBuffer midi;
            double difference = 0.0, level = 0.0;

            for (size_t at = 0; at + kBlock <= quiet.size(); at += kBlock)
            {
                buffer.clear();

                for (int c = 0; c < 2; ++c)
                {
                    buffer.copyFrom (c, 0, quiet.data() + at, kBlock);

                    if (keyPlaying && buffer.getNumChannels() >= 4)
                        buffer.copyFrom (2 + c, 0, ringing.data() + at, kBlock);
                }

                keyed->processBlock (buffer, midi);

                // The second half only, once everything has settled.
                for (int i = 0; i < kBlock; ++i)
                {
                    const auto n = at + (size_t) i;

                    if (n < quiet.size() / 2 + latency)
                        continue;

                    const auto dry = (double) quiet[n - latency];
                    difference += (buffer.getSample (0, i) - dry) * (buffer.getSample (0, i) - dry);
                    level += dry * dry;
                }
            }

            return 10.0 * std::log10 (difference / level + 1.0e-12);
        };

        check (keyed->getTotalNumInputChannels() == 4, "the plugin takes four channels in");

        const auto self = removed (false, true);
        const auto keyedOn = removed (true, true);
        const auto keyedSilent = removed (true, false);

        std::printf ("    removed from plain noise: %.1f dB unkeyed, %.1f dB keyed by a ringing signal, %.1f dB keyed by silence\n",
                     self, keyedOn, keyedSilent);
        check (keyedOn > self + 3.0, "with the sidechain on, a ringing key cuts the signal where the key rings");
        check (keyedSilent < -80.0, "and a silent key cuts nothing");
    }

    // Presets.
    check (processor->getStockPresets().size() >= 5, "there are stock presets");
    processor->loadPreset ("Sibilance");
    check (processor->getPresetName() == "Sibilance" && get ("hard") > 0.5f && get ("b1_used") < 0.5f
               && std::abs (get ("lc_freq") - 3500.0f) < 1.0f, "a stock preset sets what it names");
    processor->loadPreset ("Default");
    check (get ("hard") < 0.5f && get ("b1_used") > 0.5f && get ("b4_used") < 0.5f, "and Default puts everything back");

    const juce::String name ("zz selftest " + juce::String (juce::Time::currentTimeMillis()));
    set ("depth", 7.25f);
    set ("b5_used", 1.0f);
    set ("b5_freq", 5000.0f);
    check (processor->saveUserPreset (name) == name && processor->getUserPresets().contains (name), "a user preset is saved under its name");
    check (processor->saveUserPreset ("Default") == "Default (user)", "a stock preset's name cannot be taken");
    processor->deleteUserPreset ("Default (user)");

    processor->loadPreset ("Default");
    processor->loadPreset (name);
    check (std::abs (get ("depth") - 7.25f) < 0.01f && get ("b5_used") > 0.5f && std::abs (get ("b5_freq") - 5000.0f) < 1.0f,
           "and comes back as it was saved");

    processor->deleteUserPreset (name);
    check (! processor->getUserPresets().contains (name) && processor->getPresetName() == "Default", "a user preset can be deleted");
    processor->deleteUserPreset ("Smooth Vocal");
    check (processor->getStockPresets().contains ("Smooth Vocal"), "a stock preset cannot");

    std::printf (failures == 0 ? "\nall passed\n" : "\n%d FAILED\n", failures);
    return failures;
}

int main (int argc, char** argv)
{
    if (argc < 2)
        return 2;

    const juce::ScopedJuceInitialiser_GUI gui;

    if (juce::String (argv[1]) == "--selftest")
        return selfTest();

    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    const auto states = argc > 2 && juce::String (argv[2]) == "--states";
    const auto tip = argc > 4 && juce::String (argv[2]) == "--tip"
                         ? juce::Point<int> (juce::String (argv[3]).getIntValue(), juce::String (argv[4]).getIntValue())
                         : juce::Point<int> (-1, -1);
    const auto sized = argc > 3 && tip.x < 0;
    const auto width = sized ? juce::String (argv[2]).getIntValue() : 1290;
    const auto height = sized ? juce::String (argv[3]).getIntValue() : 740;

    {
        Director director (file, width, height, states, tip);
        juce::MessageManager::getInstance()->runDispatchLoop();
    }

    return file.existsAsFile() ? 0 : 1;
}
