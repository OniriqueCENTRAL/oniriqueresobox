#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace onirique
{

NodeIds nodeIds (int node)
{
    if (node == kLowCutNode)
        return { {}, "lc_on", "lc_freq", {}, {}, {}, {} };

    if (node == kHighCutNode)
        return { {}, "hc_on", "hc_freq", {}, {}, {}, {} };

    const auto prefix = "b" + String (node + 1) + "_";
    return { prefix + "used", prefix + "on", prefix + "freq", prefix + "gain", prefix + "q", prefix + "shape",
             prefix + "place" };
}

namespace
{
    using Attributes = AudioParameterFloatAttributes;

    constexpr int kDefaultBands = 3;
    const float kBandHz[rb::kNumBands] = { 250.0f, 1000.0f, 4000.0f, 500.0f, 2000.0f, 8000.0f, 125.0f, 12000.0f };

    const Identifier kPresetProperty ("preset");
    const Identifier kSlotProperty ("slot");
    const Identifier kOtherSlot ("OtherSlot");
    const Identifier kSnapshot ("Snapshot");
    const char* const kPresetExtension = ".resobox";

    /** What a preset or an A/B slot leaves as it is: how the plugin is being
        listened to and looked at, not what it does to the sound. */
    bool isStored (const String& id)
    {
        return id != "bypass" && id != "delta" && id != "quality" && id != "lowlat" && id != "scale";
    }

    /** A range that is even to the ear: each octave, or each doubling of a
        time, takes the same travel. */
    NormalisableRange<float> logRange (float low, float high)
    {
        return { low, high,
                 [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                 [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); },
                 [] (float a, float b, float v) { return jlimit (a, b, v); } };
    }

    float parseNumber (const String& text)
    {
        return text.retainCharacters ("0123456789.,-").replaceCharacter (',', '.').getFloatValue();
    }

    Attributes plain (int decimals, const String& unit = {})
    {
        return Attributes()
            .withStringFromValueFunction ([decimals, unit] (float v, int)
                                          { return String (v, decimals) + (unit.isEmpty() ? String() : " " + unit); })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });
    }

    Attributes hertz()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz"
                                                                  : String (roundToInt (v)) + " Hz";
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              const auto v = parseNumber (t);
                                              return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
                                          });
    }

    Attributes milliseconds()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          { return String (v, v < 10.0f ? 1 : 0) + " ms"; })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });
    }

    Attributes percent()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });
    }

    Attributes decibels (const String& unit = "dB")
    {
        return Attributes()
            .withStringFromValueFunction ([unit] (float v, int)
                                          { return (v > 0.04f ? "+" : "") + String (v, 1) + " " + unit; })
            .withValueFromStringFunction ([] (const String& t) { return parseNumber (t); });
    }

    /** Which way the balance leans, in the words of whichever pair is in use. */
    Attributes balance()
    {
        return Attributes()
            .withStringFromValueFunction ([] (float v, int)
                                          {
                                              const auto amount = roundToInt (std::abs (v));
                                              return amount == 0 ? String ("C") : String (amount) + (v < 0.0f ? " L" : " R");
                                          })
            .withValueFromStringFunction ([] (const String& t)
                                          {
                                              const auto v = std::abs (parseNumber (t));
                                              return t.containsIgnoreCase ("l") || t.contains ("-") ? -v : v;
                                          });
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        AudioProcessorValueTreeState::ParameterLayout layout;
        const rb::Settings defaults;

        auto number = [&layout] (const String& id, const String& name, NormalisableRange<float> range,
                                 float value, const Attributes& attributes)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, value, attributes));
        };

        auto toggle = [&layout] (const String& id, const String& name, bool value, bool automatable = true)
        {
            layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, value,
                                                              AudioParameterBoolAttributes().withAutomatable (automatable)));
        };

        auto choice = [&layout] (const String& id, const String& name, const StringArray& choices, int value,
                                 bool automatable = true)
        {
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, value,
                                                                AudioParameterChoiceAttributes().withAutomatable (automatable)));
        };

        number ("depth", "Depth", { 0.0f, 10.0f, 0.01f }, defaults.depth, plain (1));
        number ("detail", "Detail", { 0.0f, 10.0f, 0.01f }, defaults.detail, plain (1));
        number ("attack", "Attack", logRange (0.5f, 200.0f), defaults.attackMs, milliseconds());
        number ("release", "Release", logRange (5.0f, 1000.0f), defaults.releaseMs, milliseconds());
        toggle ("hard", "Hard", defaults.hard);
        number ("tilt", "Tilt", { -6.0f, 6.0f, 0.01f }, 0.0f, decibels ("/oct"));

        choice ("mode", "Stereo Mode", { "Stereo", "Mid / Side", "Mono" }, 0);
        number ("link", "Stereo Link", { 0.0f, 100.0f, 1.0f }, defaults.link * 100.0f, percent());
        number ("balance", "Balance", { -100.0f, 100.0f, 1.0f }, 0.0f, balance());

        toggle ("lc_on", "Low Cut", defaults.lowCutOn);
        number ("lc_freq", "Low Cut Frequency", logRange (20.0f, 20000.0f), defaults.lowCutHz, hertz());
        toggle ("hc_on", "High Cut", defaults.highCutOn);
        number ("hc_freq", "High Cut Frequency", logRange (20.0f, 20000.0f), defaults.highCutHz, hertz());

        for (int i = 0; i < rb::kNumBands; ++i)
        {
            const auto ids = nodeIds (i);
            const auto name = "Band " + String (i + 1);

            // Whether a band exists is part of the layout of the curve, not
            // something to draw automation for.
            toggle (ids.used, name + " Used", i < kDefaultBands, false);
            toggle (ids.on, name, true);
            number (ids.freq, name + " Frequency", logRange (20.0f, 20000.0f), kBandHz[i], hertz());
            number (ids.gain, name + " Sensitivity", { -12.0f, 12.0f, 0.01f }, 0.0f, decibels());
            number (ids.q, name + " Q", logRange (0.3f, 8.0f), 1.0f, plain (2));
            choice (ids.shape, name + " Shape", { "Bell", "Low Shelf", "High Shelf", "Plateau" }, 0);
            choice (ids.place, name + " Placement", { "Stereo", "Left", "Right", "Mid", "Side" }, 0);
        }

        number ("mix", "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, percent());
        number ("out", "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, decibels());
        toggle ("autogain", "Auto Gain", false);
        toggle ("delta", "Delta", false);
        toggle ("bypass", "Bypass", false);
        toggle ("sidechain", "Sidechain", false);
        choice ("quality", "Quality", { "Normal", "High", "Ultra" }, 0);
        toggle ("lowlat", "Low Latency", false);

        // How many dB of reduction the height of the display stands for.
        choice ("scale", "Display Scale", { "6", "9", "12", "18", "24" }, 2, false);

        return layout;
    }

    //==============================================================================
    struct StockPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // whatever differs from the defaults
    };

    // Starting points, set by reasoning rather than by ear: each one is the
    // curve one would draw first for that source.
    const std::vector<StockPreset>& stockPresets()
    {
        static const std::vector<StockPreset> presets
        {
            { "Default", {} },

            { "Smooth Vocal", { { "depth", 4.5f }, { "detail", 6.0f }, { "attack", 8.0f }, { "release", 80.0f },
                                { "lc_freq", 180.0f },
                                { "b1_freq", 400.0f }, { "b1_gain", -3.0f },
                                { "b2_freq", 3200.0f }, { "b2_gain", 4.0f }, { "b2_q", 0.8f },
                                { "b3_freq", 8000.0f }, { "b3_gain", 2.0f } } },

            { "Dialogue", { { "depth", 4.0f }, { "detail", 5.0f }, { "attack", 10.0f }, { "release", 100.0f },
                            { "lc_freq", 120.0f },
                            { "b1_freq", 300.0f }, { "b1_gain", 2.0f },
                            { "b2_freq", 2500.0f }, { "b2_gain", 3.0f },
                            { "b3_freq", 7000.0f } } },

            { "Sibilance", { { "depth", 6.0f }, { "detail", 4.0f }, { "hard", 1.0f }, { "attack", 1.0f }, { "release", 30.0f },
                             { "lc_freq", 3500.0f },
                             { "b1_used", 0.0f }, { "b2_used", 0.0f },
                             { "b3_freq", 7000.0f }, { "b3_gain", 4.0f }, { "b3_q", 0.7f } } },

            { "Harsh Guitars", { { "depth", 5.5f }, { "detail", 6.5f }, { "attack", 5.0f }, { "release", 60.0f },
                                 { "lc_freq", 250.0f }, { "hc_on", 1.0f }, { "hc_freq", 9000.0f },
                                 { "b1_freq", 600.0f }, { "b1_gain", -4.0f },
                                 { "b2_freq", 3000.0f }, { "b2_gain", 5.0f },
                                 { "b3_freq", 5500.0f }, { "b3_gain", 2.0f } } },

            { "Cymbals & Overheads", { { "depth", 5.0f }, { "detail", 7.5f }, { "attack", 2.0f }, { "release", 40.0f },
                                       { "lc_freq", 1500.0f },
                                       { "b1_used", 0.0f },
                                       { "b2_freq", 4000.0f }, { "b2_gain", 2.0f },
                                       { "b3_freq", 8000.0f }, { "b3_gain", 3.0f } } },

            { "Piano Boxiness", { { "depth", 5.0f }, { "detail", 5.5f }, { "attack", 12.0f }, { "release", 120.0f },
                                  { "lc_freq", 150.0f }, { "hc_on", 1.0f }, { "hc_freq", 2500.0f },
                                  { "b1_freq", 350.0f }, { "b1_gain", 4.0f }, { "b1_q", 0.8f },
                                  { "b2_freq", 900.0f }, { "b2_gain", 1.5f },
                                  { "b3_used", 0.0f } } },

            { "Bass Mud", { { "depth", 5.0f }, { "detail", 4.0f }, { "attack", 15.0f }, { "release", 150.0f },
                            { "lc_freq", 60.0f }, { "hc_on", 1.0f }, { "hc_freq", 700.0f },
                            { "b1_freq", 220.0f }, { "b1_gain", 4.0f },
                            { "b2_used", 0.0f }, { "b3_used", 0.0f } } },

            { "Mix Bus Gentle", { { "depth", 2.5f }, { "detail", 5.0f }, { "attack", 15.0f }, { "release", 150.0f },
                                  { "lc_freq", 150.0f }, { "tilt", 1.0f },
                                  { "b1_freq", 300.0f }, { "b2_freq", 3000.0f }, { "b2_gain", 2.0f },
                                  { "b3_freq", 9000.0f } } },
        };

        return presets;
    }

    const StockPreset* findStock (const String& name)
    {
        for (const auto& preset : stockPresets())
            if (name.equalsIgnoreCase (preset.name))
                return &preset;

        return nullptr;
    }

    File presetFile (const String& name)
    {
        return ResoBoxProcessor::getUserPresetFolder()
                   .getChildFile (File::createLegalFileName (name) + kPresetExtension);
    }
}

/** The parameters' values, looked up once. */
struct ResoBoxProcessor::Raw
{
    explicit Raw (AudioProcessorValueTreeState& state)
    {
        auto get = [&state] (const String& id) { return state.getRawParameterValue (id); };

        depth = get ("depth");       detail = get ("detail");        attack = get ("attack");
        release = get ("release");   hard = get ("hard");            tilt = get ("tilt");
        mode = get ("mode");         link = get ("link");            balance = get ("balance");
        mix = get ("mix");           out = get ("out");              delta = get ("delta");
        autoGain = get ("autogain");
        bypass = get ("bypass");     sidechain = get ("sidechain");  quality = get ("quality");
        lowLatency = get ("lowlat");

        for (int i = 0; i < kNumNodes; ++i)
        {
            const auto ids = nodeIds (i);
            const auto isBand = i < rb::kNumBands;

            nodeUsed[i] = isBand ? get (ids.used) : nullptr;
            nodeOn[i] = get (ids.on);
            nodeFreq[i] = get (ids.freq);
            nodeGain[i] = isBand ? get (ids.gain) : nullptr;
            nodeQ[i] = isBand ? get (ids.q) : nullptr;
            nodeShape[i] = isBand ? get (ids.shape) : nullptr;
            nodePlace[i] = isBand ? get (ids.place) : nullptr;
        }
    }

    std::atomic<float> *depth, *detail, *attack, *release, *hard, *tilt, *mode, *link, *balance,
                       *mix, *out, *delta, *bypass, *sidechain, *quality, *lowLatency, *autoGain;
    std::atomic<float> *nodeUsed[kNumNodes], *nodeOn[kNumNodes], *nodeFreq[kNumNodes],
                       *nodeGain[kNumNodes], *nodeQ[kNumNodes], *nodeShape[kNumNodes],
                       *nodePlace[kNumNodes];
};

//==============================================================================
ResoBoxProcessor::ResoBoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ResoBox", createLayout()),
      raw (std::make_unique<Raw> (apvts))
{
    bypassParameter = apvts.getParameter ("bypass");
}

ResoBoxProcessor::~ResoBoxProcessor()
{
    cancelPendingUpdate();
}

bool ResoBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != AudioChannelSet::mono() && out != AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != out)
        return false;

    const auto side = layouts.getChannelSet (true, 1);
    return side.isDisabled() || side == AudioChannelSet::mono() || side == AudioChannelSet::stereo();
}

rb::Settings ResoBoxProcessor::readSettings() const noexcept
{
    rb::Settings s;
    const auto& r = *raw;

    s.depth = r.depth->load();
    s.detail = r.detail->load();
    s.attackMs = r.attack->load();
    s.releaseMs = r.release->load();
    s.hard = r.hard->load() > 0.5f;
    s.tiltDbPerOctave = r.tilt->load();

    s.stereoMode = (rb::StereoMode) jlimit (0, 2, roundToInt (r.mode->load()));
    s.link = r.link->load() * 0.01f;
    s.balance = r.balance->load() * 0.01f;

    s.lowCutOn = r.nodeOn[kLowCutNode]->load() > 0.5f;
    s.lowCutHz = r.nodeFreq[kLowCutNode]->load();
    s.highCutOn = r.nodeOn[kHighCutNode]->load() > 0.5f;
    s.highCutHz = r.nodeFreq[kHighCutNode]->load();

    for (int i = 0; i < rb::kNumBands; ++i)
    {
        s.bands[i].on = r.nodeUsed[i]->load() > 0.5f && r.nodeOn[i]->load() > 0.5f;
        s.bands[i].freq = r.nodeFreq[i]->load();
        s.bands[i].gainDb = r.nodeGain[i]->load();
        s.bands[i].q = r.nodeQ[i]->load();
        s.bands[i].shape = (rb::BandShape) jlimit (0, rb::kNumBandShapes - 1, roundToInt (r.nodeShape[i]->load()));
        s.bands[i].placement = (rb::BandPlacement) jlimit (0, rb::kNumPlacements - 1, roundToInt (r.nodePlace[i]->load()));
    }

    s.mix = r.mix->load() * 0.01f;
    s.outDb = r.out->load();
    s.autoGain = r.autoGain->load() > 0.5f;
    s.delta = r.delta->load() > 0.5f;
    s.bypass = r.bypass->load() > 0.5f;
    s.sidechain = r.sidechain->load() > 0.5f;
    s.quality = roundToInt (r.quality->load());
    s.lowLatency = r.lowLatency->load() > 0.5f;

    return s;
}

void ResoBoxProcessor::prepareToPlay (double sampleRate, int)
{
    // Many hosts call this just before a bounce: the licence is read again then.
    license.refresh();

    const auto settings = readSettings();

    suppressor.setSettings (settings);
    suppressor.prepare (sampleRate, getMainBusNumOutputChannels());

    const auto maxBins = (size_t) rb::ResonanceSuppressor::latencyFor (sampleRate, rb::kNumFrameSizes, false) / 2 + 1;

    {
        const SpinLock::ScopedLockType lock (visualLock);
        visualIn.assign (maxBins, -120.0f);
        visualOut.assign (maxBins, -120.0f);
        visualRed.assign (maxBins, 0.0f);
        visualBins = 0;
    }

    reportedFrame.store (suppressor.latency());
    setLatencySamples (suppressor.latency());
}

void ResoBoxProcessor::reset()
{
    suppressor.reset();
}

void ResoBoxProcessor::handleAsyncUpdate()
{
    setLatencySamples (reportedFrame.load());
}

void ResoBoxProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const auto settings = readSettings();
    const auto numChannels = jmin (getMainBusNumOutputChannels(), (int) rb::kMaxChannels);
    const auto numSamples = buffer.getNumSamples();

    auto side = getBusCount (true) > 1 ? getBusBuffer (buffer, true, 1) : AudioBuffer<float>();

    float* io[rb::kMaxChannels] = {};
    const float* key[rb::kMaxChannels] = {};

    for (int c = 0; c < numChannels; ++c)
    {
        io[c] = buffer.getWritePointer (c);

        if (side.getNumChannels() > 0)
            key[c] = side.getReadPointer (jmin (c, side.getNumChannels() - 1));
    }

    suppressor.setSettings (settings);
    suppressor.process (io, side.getNumChannels() > 0 ? key : nullptr, numChannels, numSamples);

    // A change of frame is a change of latency: the host has to be told, and
    // not from here.
    if (reportedFrame.exchange (suppressor.latency()) != suppressor.latency())
        triggerAsyncUpdate();

    // The sidechain's channels are inputs only; whatever else the host gave
    // us beyond the main pair is silenced.
    for (int c = numChannels; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    if (suppressor.getFrameCount() != visualFrame)
    {
        const SpinLock::ScopedTryLockType lock (visualLock);

        if (lock.isLocked())
        {
            const auto bins = jmin (suppressor.getDisplayBins(), (int) visualIn.size());

            std::copy_n (suppressor.getInputDb(), bins, visualIn.begin());
            std::copy_n (suppressor.getOutputDb(), bins, visualOut.begin());
            std::copy_n (suppressor.getReductionDb(), bins, visualRed.begin());

            visualBins = bins;
            visualBinHz = suppressor.getDisplayBinHz();
            visualFrame = suppressor.getFrameCount();
        }
    }

    // Without a licence the output is replaced by noise: see License/DemoNoise.h.
    license.process (buffer, getSampleRate(), isNonRealtime());
}

bool ResoBoxProcessor::getVisual (std::vector<float>& inputDb, std::vector<float>& outputDb,
                                  std::vector<float>& reductionDb, double& binHz)
{
    const SpinLock::ScopedLockType lock (visualLock);

    if (visualBins <= 0)
        return false;

    inputDb.assign (visualIn.begin(), visualIn.begin() + visualBins);
    outputDb.assign (visualOut.begin(), visualOut.begin() + visualBins);
    reductionDb.assign (visualRed.begin(), visualRed.begin() + visualBins);
    binHz = visualBinHz;
    return true;
}

AudioProcessorEditor* ResoBoxProcessor::createEditor()
{
    return new ResoBoxEditor (*this);
}

//==============================================================================
ValueTree ResoBoxProcessor::capture() const
{
    ValueTree snapshot (kSnapshot);

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<RangedAudioParameter*> (parameter))
            if (isStored (ranged->paramID))
                snapshot.setProperty (ranged->paramID, ranged->convertFrom0to1 (ranged->getValue()), nullptr);

    return snapshot;
}

void ResoBoxProcessor::apply (const ValueTree& snapshot)
{
    for (auto* parameter : getParameters())
    {
        auto* ranged = dynamic_cast<RangedAudioParameter*> (parameter);

        if (ranged == nullptr || ! isStored (ranged->paramID))
            continue;

        // What a snapshot does not mention goes back to its default, so that
        // an old preset still means the same thing after a parameter is added.
        const auto wanted = snapshot.hasProperty (ranged->paramID)
                                ? ranged->convertTo0to1 ((float) snapshot[ranged->paramID])
                                : ranged->getDefaultValue();

        if (std::abs (wanted - ranged->getValue()) > 1.0e-6f)
        {
            ranged->beginChangeGesture();
            ranged->setValueNotifyingHost (wanted);
            ranged->endChangeGesture();
        }
    }
}

void ResoBoxProcessor::switchToSlot (int slot)
{
    slot = jlimit (0, 1, slot);

    if (slot == activeSlot)
        return;

    ValueTree wanted;

    {
        const ScopedLock lock (slotLock);
        slots[activeSlot] = capture();
        wanted = slots[slot];
        activeSlot = slot;
    }

    // A slot never visited starts out as a copy of the one just left.
    if (wanted.isValid())
        apply (wanted);
}

void ResoBoxProcessor::copyToOtherSlot()
{
    const ScopedLock lock (slotLock);
    slots[1 - activeSlot] = capture();
}

//==============================================================================
File ResoBoxProcessor::getUserPresetFolder()
{
    return File::getSpecialLocation (File::userApplicationDataDirectory)
               .getChildFile ("Onirique").getChildFile ("ResoBox").getChildFile ("Presets");
}

StringArray ResoBoxProcessor::getStockPresets() const
{
    StringArray names;

    for (const auto& preset : stockPresets())
        names.add (preset.name);

    return names;
}

StringArray ResoBoxProcessor::getUserPresets() const
{
    StringArray names;

    for (const auto& file : getUserPresetFolder().findChildFiles (File::findFiles, false,
                                                                  String ("*") + kPresetExtension))
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

String ResoBoxProcessor::getPresetName() const
{
    return apvts.state.getProperty (kPresetProperty, "Default").toString();
}

void ResoBoxProcessor::setPresetName (const String& name)
{
    apvts.state.setProperty (kPresetProperty, name, nullptr);
}

bool ResoBoxProcessor::isUserPreset (const String& name) const
{
    return findStock (name) == nullptr && presetFile (name).existsAsFile();
}

void ResoBoxProcessor::loadPreset (const String& name)
{
    if (const auto* stock = findStock (name))
    {
        ValueTree snapshot (kSnapshot);

        for (const auto& [id, value] : stock->values)
            snapshot.setProperty (id, value, nullptr);

        apply (snapshot);
        setPresetName (stock->name);
        return;
    }

    if (const auto xml = parseXML (presetFile (name)))
    {
        const auto snapshot = ValueTree::fromXml (*xml);

        if (snapshot.hasType (kSnapshot))
        {
            apply (snapshot);
            setPresetName (name);
        }
    }
}

String ResoBoxProcessor::saveUserPreset (const String& requested)
{
    auto name = File::createLegalFileName (requested.trim());

    if (name.isEmpty())
        return {};

    // The stock presets are not the user's to overwrite.
    if (findStock (name) != nullptr)
        name << " (user)";

    const auto file = presetFile (name);

    if (! file.getParentDirectory().createDirectory().wasOk())
        return {};

    if (const auto xml = capture().createXml())
    {
        if (xml->writeTo (file))
        {
            setPresetName (name);
            return name;
        }
    }

    return {};
}

void ResoBoxProcessor::deleteUserPreset (const String& name)
{
    if (! isUserPreset (name))
        return;

    presetFile (name).deleteFile();

    // The settings stay as they are; they are just no longer anyone's.
    if (getPresetName() == name)
        setPresetName ("Default");
}

//==============================================================================
void ResoBoxProcessor::getStateInformation (MemoryBlock& destination)
{
    auto state = apvts.copyState();

    {
        const ScopedLock lock (slotLock);

        state.setProperty (kSlotProperty, activeSlot, nullptr);
        state.removeChild (state.getChildWithName (kOtherSlot), nullptr);

        if (slots[1 - activeSlot].isValid())
        {
            ValueTree other (kOtherSlot);
            other.copyPropertiesFrom (slots[1 - activeSlot], nullptr);
            state.appendChild (other, nullptr);
        }
    }

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void ResoBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = ValueTree::fromXml (*xml);
    const auto other = state.getChildWithName (kOtherSlot);

    {
        const ScopedLock lock (slotLock);

        activeSlot = jlimit (0, 1, (int) state.getProperty (kSlotProperty, 0));
        slots[activeSlot] = {};
        slots[1 - activeSlot] = {};

        if (other.isValid())
        {
            ValueTree snapshot (kSnapshot);
            snapshot.copyPropertiesFrom (other, nullptr);
            slots[1 - activeSlot] = snapshot;
        }
    }

    state.removeChild (other, nullptr);
    apvts.replaceState (state);
}

} // namespace onirique

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new onirique::ResoBoxProcessor();
}
