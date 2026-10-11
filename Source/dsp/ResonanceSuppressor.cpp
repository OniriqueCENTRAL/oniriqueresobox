#include "ResonanceSuppressor.h"

#include <algorithm>
#include <cmath>

namespace rb
{

namespace
{
    constexpr float kKneeDb = 6.0f;
    constexpr float kLn10Over20 = 0.115129255f;
    constexpr float kDetectorSeconds = 0.025f;

    // Auto gain follows the level slowly enough not to pump, and never gives
    // back more than this.
    constexpr float kAutoGainSeconds = 0.4f;
    constexpr float kMaxMakeup = 4.0f;      // +12 dB

    // Which of the frame sizes the detector reads, in every mode: the "high" one.
    constexpr int kDetectorFrame = 2;

    // The scale the spectrum is read at, at the two ends of the detail knob.
    constexpr float kBroadestOctaves = 1.0f / 3.0f;
    constexpr float kFinestOctaves = 1.0f / 24.0f;

    // How far from a point its surroundings begin, and how far they reach, in
    // multiples of that scale; and the least of each in bins.
    constexpr float kGapScales = 1.0f;
    constexpr float kSideScales = 4.0f;
    constexpr int kMinGapBins = 3;
    constexpr int kMinSideBins = 4;

    constexpr float kThresholdDb = 4.0f;
    constexpr float kThresholdPerDepth = 0.3f;

    /** The frame grows with the sample rate so that it always covers about
        the same stretch of time, and so the same resolution in Hz. */
    int baseOrderFor (double sampleRate) noexcept
    {
        return sampleRate > 100000.0 ? 13 : sampleRate > 50000.0 ? 12 : 11;
    }

    /** Which of the frame sizes a pair of settings asks for. */
    int frameFor (int quality, bool lowLatency) noexcept
    {
        return lowLatency ? 0 : 1 + std::max (0, std::min (kNumFrameSizes - 2, quality));
    }
}

float bandDb (const Band& band, float hz) noexcept
{
    // Everything is laid out in octaves: a bell is about 1.4 of them wide at
    // Q = 1, and a shelf takes about as long to get from one level to the other.
    const float width = 0.6f / std::max (0.1f, band.q);
    const float d = (std::log2 (std::max (1.0f, hz)) - std::log2 (std::max (1.0f, band.freq))) / width;

    switch (band.shape)
    {
        case BandShape::lowShelf:   return band.gainDb * 0.5f * (1.0f - std::tanh (1.5f * d));
        case BandShape::highShelf:  return band.gainDb * 0.5f * (1.0f + std::tanh (1.5f * d));

        // A bell with its top cut flat: the same level across the band, and
        // steep sides.
        case BandShape::plateau:
        {
            const float x = d / 1.6f;
            return band.gainDb * std::exp (-0.5f * x * x * x * x * x * x);
        }

        case BandShape::bell:
        default:                    return band.gainDb * std::exp (-0.5f * d * d);
    }
}

float bandShape (const Band& band, float hz) noexcept
{
    auto unit = band;
    unit.gainDb = 1.0f;
    return bandDb (unit, hz);
}

float sensitivityDb (const Settings& s, float hz, int detector) noexcept
{
    hz = std::max (1.0f, hz);

    const float position = std::log2 (hz);
    float db = s.tiltDbPerOctave * (position - std::log2 (1000.0f));

    for (const auto& band : s.bands)
    {
        const auto here = band.placement == BandPlacement::both || detector < 0
                          || (int) band.placement - 1 == detector;

        if (band.on && here)
            db += bandDb (band, hz);
    }

    // 24 dB per octave either side.
    if (s.lowCutOn)
        db -= 10.0f * std::log10 (1.0f + std::pow (s.lowCutHz / hz, 8.0f));

    if (s.highCutOn)
        db -= 10.0f * std::log10 (1.0f + std::pow (hz / std::max (1.0f, s.highCutHz), 8.0f));

    return db;
}

int ResonanceSuppressor::latencyFor (double sampleRate, int quality, bool lowLatency) noexcept
{
    return 1 << (baseOrderFor (sampleRate) - 1 + frameFor (quality, lowLatency));
}

void ResonanceSuppressor::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    baseOrder = baseOrderFor (sampleRate);

    const auto maxSize = (size_t) 1 << (baseOrder + kNumFrameSizes - 2);
    const auto maxBins = maxSize / 2 + 1;

    for (int i = 0; i < kNumFrameSizes; ++i)
        ffts[i] = std::make_unique<Fft> (1 << (baseOrder - 1 + i));

    // The detector always reads the same length of signal, whatever frame
    // the processing runs on: what is found must not depend on the latency.
    detFft = ffts[kDetectorFrame].get();
    detSize = detFft->size();
    historyMask = (int) maxSize - 1;

    window.assign (maxSize, 0.0f);
    detWindow.assign ((size_t) detSize, 0.0f);

    for (int i = 0; i < detSize; ++i)
        detWindow[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * M_PI * (double) i / (double) detSize);

    for (int c = 0; c < kMaxChannels; ++c)
    {
        inRing[c].assign (maxSize, 0.0f);
        outRing[c].assign (maxSize, 0.0f);
        inHistory[c].assign (maxSize, 0.0f);
        scHistory[c].assign (maxSize, 0.0f);
        spec[c].assign (maxSize, {});
        detSpec[c].assign ((size_t) detSize, {});
        scSpec[c].assign ((size_t) detSize, {});
        midSideSpec[c].assign ((size_t) detSize, {});
    }

    for (int d = 0; d < kNumDetectors; ++d)
    {
        target[d].assign (maxBins, 0.0f);
        state[d].assign (maxBins, 0.0f);
        detTarget[d].assign (maxBins, 0.0f);
        follower[d].assign (maxBins, 0.0f);
    }

    for (auto& w : weights)
        w.assign (maxBins, 0.0f);

    for (auto& v : separation)
        v.assign (maxBins, 0.0f);

    for (auto* v : { &fine, &scratch, &crossing, &detCrossing, &visIn, &visOut, &visRed })
        v->assign (maxBins, 0.0f);

    std::fill (visIn.begin(), visIn.end(), -120.0f);
    std::fill (visOut.begin(), visOut.end(), -120.0f);

    prefix.assign (maxBins + 1, 0.0);

    smoothing = 1.0f - std::exp (-1.0f / (0.02f * (float) sampleRate));
    powerSmoothing = 1.0f - std::exp (-1.0f / (kAutoGainSeconds * (float) sampleRate));

    activeFrame = -1;
    configure (frameFor (settings.quality, settings.lowLatency));
}

void ResonanceSuppressor::configure (int frame) noexcept
{
    activeFrame = frame;
    fft = ffts[activeFrame].get();
    fftSize = fft->size();
    hop = fftSize / 4;

    // The detector keeps its own pace, a quarter of its own frame, unless the
    // processing frames come less often than that.
    detInterval = std::max (hop, detSize / 4);

    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * M_PI * (double) i / (double) fftSize);

    weightsDirty = true;
    reset();
}

void ResonanceSuppressor::reset() noexcept
{
    for (int c = 0; c < kMaxChannels; ++c)
    {
        std::fill (inRing[c].begin(), inRing[c].end(), 0.0f);
        std::fill (outRing[c].begin(), outRing[c].end(), 0.0f);
        std::fill (inHistory[c].begin(), inHistory[c].end(), 0.0f);
        std::fill (scHistory[c].begin(), scHistory[c].end(), 0.0f);
    }

    for (int d = 0; d < kNumDetectors; ++d)
    {
        std::fill (target[d].begin(), target[d].end(), 0.0f);
        std::fill (state[d].begin(), state[d].end(), 0.0f);
        std::fill (detTarget[d].begin(), detTarget[d].end(), 0.0f);
        std::fill (follower[d].begin(), follower[d].end(), 0.0f);
    }

    std::fill (crossing.begin(), crossing.end(), 0.0f);
    std::fill (visRed.begin(), visRed.end(), 0.0f);

    dryPower = processedPower = 0.0;
    makeupNow = 1.0f;

    pos = 0;
    hopCount = 0;
    historyPos = 0;
    detCount = 0;
    snap = true;
}

void ResonanceSuppressor::setSettings (const Settings& s) noexcept
{
    auto curveChanged = s.stereoMode != settings.stereoMode || s.tiltDbPerOctave != settings.tiltDbPerOctave || s.lowCutOn != settings.lowCutOn || s.lowCutHz != settings.lowCutHz
                        || s.highCutOn != settings.highCutOn || s.highCutHz != settings.highCutHz;

    for (int i = 0; i < kNumBands && ! curveChanged; ++i)
    {
        const auto& a = s.bands[i];
        const auto& b = settings.bands[i];
        curveChanged = a.on != b.on || a.freq != b.freq || a.gainDb != b.gainDb || a.q != b.q || a.shape != b.shape
                       || a.placement != b.placement;
    }

    weightsDirty = weightsDirty || curveChanged;
    settings = s;
}

void ResonanceSuppressor::updateWeights() noexcept
{
    const int bins = detSize / 2 + 1;
    const auto binHz = (float) (sampleRate / (double) detSize);

    // A curve for each detector, which differ where a band is placed on one
    // of them only, and one with every band in it for the mono detector.
    for (int d = 0; d <= kNumDetectors; ++d)
    {
        const int detector = d < kNumDetectors ? d : -1;

        for (int k = 0; k < bins; ++k)
        {
            const float db = std::max (-80.0f, std::min (18.0f, sensitivityDb (settings, (float) k * binHz, detector)));
            weights[d][(size_t) k] = std::exp (db * kLn10Over20);
        }

        // Nothing useful can be said about DC.
        weights[d][0] = 0.0f;
    }

    // Where the work crosses over to the other pair of signals: under every
    // band placed on mid or side while the mode is left and right, or the
    // other way about.
    const bool midSide = settings.stereoMode == StereoMode::midSide;

    anyCrossing = false;
    std::fill (detCrossing.begin(), detCrossing.end(), 0.0f);

    for (auto& v : separation)
        std::fill (v.begin(), v.end(), 0.0f);

    for (const auto& band : settings.bands)
    {
        const bool onMidSide = band.placement == BandPlacement::mid || band.placement == BandPlacement::side;
        const bool onLeftRight = band.placement == BandPlacement::left || band.placement == BandPlacement::right;

        if (! band.on)
            continue;

        // A band for one signal of a pair is no use if the link then hands
        // its cut to the other one: under it, the two go their own ways.
        if (onLeftRight || onMidSide)
        {
            auto& apart = separation[onMidSide ? 1 : 0];

            for (int k = 0; k < bins; ++k)
                apart[(size_t) k] = std::max (apart[(size_t) k], std::min (1.0f, bandShape (band, (float) k * binHz)));
        }

        if (! (midSide ? onLeftRight : onMidSide))
            continue;

        anyCrossing = true;

        for (int k = 0; k < bins; ++k)
            detCrossing[(size_t) k] = std::max (detCrossing[(size_t) k], std::min (1.0f, bandShape (band, (float) k * binHz)));
    }

    weightsDirty = false;
}

//==============================================================================
void ResonanceSuppressor::process (float* const* io, const float* const* sidechain,
                                   int numChannels, int numSamples) noexcept
{
    numChannels = std::min (numChannels, kMaxChannels);

    if (frameFor (settings.quality, settings.lowLatency) != activeFrame)
        configure (frameFor (settings.quality, settings.lowLatency));

    const bool useSidechain = settings.sidechain && sidechain != nullptr;
    const int mask = fftSize - 1;

    const float mixTarget = std::max (0.0f, std::min (1.0f, settings.mix));
    const float gainTarget = std::exp (settings.outDb * kLn10Over20);
    const float deltaTarget = settings.delta ? 1.0f : 0.0f;
    const float bypassTarget = settings.bypass ? 1.0f : 0.0f;

    if (snap)
    {
        mixNow = mixTarget;
        gainNow = gainTarget;
        deltaNow = deltaTarget;
        bypassNow = bypassTarget;
        snap = false;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        mixNow += (mixTarget - mixNow) * smoothing;
        gainNow += (gainTarget - gainNow) * smoothing;
        deltaNow += (deltaTarget - deltaNow) * smoothing;
        bypassNow += (bypassTarget - bypassNow) * smoothing;

        float dry[kMaxChannels] = {}, processed[kMaxChannels] = {};
        double dryNow = 0.0, processedNow = 0.0;

        for (int c = 0; c < numChannels; ++c)
        {
            // What sits at the write position is the input of one frame ago:
            // the dry signal, already lined up with what comes out wet.
            dry[c] = inRing[c][(size_t) pos];
            const float wet = outRing[c][(size_t) pos];
            const float in = io[c][i];

            inRing[c][(size_t) pos] = in;
            outRing[c][(size_t) pos] = 0.0f;
            inHistory[c][(size_t) historyPos] = in;
            scHistory[c][(size_t) historyPos] = useSidechain && sidechain[c] != nullptr ? sidechain[c][i] : 0.0f;

            processed[c] = dry[c] + mixNow * (wet - dry[c]);

            dryNow += (double) dry[c] * dry[c];
            processedNow += (double) processed[c] * processed[c];
        }

        // What the cuts cost in level, measured on both channels together so
        // that the gain given back is the same for both.
        dryPower += (double) powerSmoothing * (dryNow - dryPower);
        processedPower += (double) powerSmoothing * (processedNow - processedPower);

        const float makeup = settings.autoGain
                                 ? std::max (1.0f, std::min (kMaxMakeup, (float) std::sqrt ((dryPower + 1.0e-12) / (processedPower + 1.0e-12))))
                                 : 1.0f;

        makeupNow += (makeup - makeupNow) * smoothing;

        for (int c = 0; c < numChannels; ++c)
        {
            // Delta is what was taken away, and stays that: the gain given
            // back is not part of it.
            const float removed = dry[c] - processed[c];
            const float level = processed[c] * makeupNow;
            const float heard = (level + deltaNow * (removed - level)) * gainNow;

            io[c][i] = heard + bypassNow * (dry[c] - heard);
        }

        pos = (pos + 1) & mask;
        historyPos = (historyPos + 1) & historyMask;
        ++detCount;

        if (++hopCount >= hop)
        {
            hopCount = 0;

            if (detCount >= detInterval)
            {
                detCount = 0;
                runDetector (numChannels, useSidechain);
            }

            processFrame (numChannels);
        }
    }
}

void ResonanceSuppressor::runDetector (int numChannels, bool useSidechain) noexcept
{
    const int size = detSize;
    const int bins = size / 2 + 1;
    const bool stereo = numChannels == 2;
    const bool mono = stereo && settings.stereoMode == StereoMode::mono;
    const int mainPair = stereo && settings.stereoMode == StereoMode::midSide ? 1 : 0;

    if (weightsDirty)
        updateWeights();

    const bool crossed = stereo && ! mono && anyCrossing;

    // The detector's frame ends at the newest sample when the processing
    // frame is the shorter of the two, and sits in the middle of it when it
    // is the longer.
    const int start = historyPos - std::max (0, (fftSize - size) / 2) - size;

    for (int c = 0; c < numChannels; ++c)
    {
        auto* x = detSpec[c].data();
        auto* key = scSpec[c].data();

        for (int i = 0; i < size; ++i)
        {
            const auto from = (size_t) ((start + i) & historyMask);
            x[i] = { inHistory[c][from] * detWindow[(size_t) i], 0.0f };
            key[i] = { scHistory[c][from] * detWindow[(size_t) i], 0.0f };
        }

        detFft->forward (x);

        if (useSidechain)
            detFft->forward (key);
    }

    // What the detectors read: the signal, or the sidechain in its place.
    const auto* source = useSidechain ? scSpec : detSpec;

    // Mid and side are sums and differences, and so are their spectra.
    if (stereo)
    {
        for (int k = 0; k < bins; ++k)
        {
            const auto l = source[0][(size_t) k], r = source[1][(size_t) k];
            midSideSpec[0][(size_t) k] = 0.5f * (l + r);
            midSideSpec[1][(size_t) k] = 0.5f * (l - r);
        }
    }

    // Balance leans the work towards one channel by sparing the other.
    const float balance = std::max (-1.0f, std::min (1.0f, settings.balance));
    const float amount[kMaxChannels] = { std::min (1.0f, 1.0f - balance), std::min (1.0f, 1.0f + balance) };

    if (mono)
    {
        // One detector on the sum, one cut for both sides: the image cannot
        // move, whatever is found.
        detect (midSideSpec[0].data(), follower[0].data(), detTarget[0].data(), 1.0f, weights[kNumDetectors].data());
        std::copy (detTarget[0].begin(), detTarget[0].begin() + bins, detTarget[1].begin());
        std::fill (detTarget[2].begin(), detTarget[2].begin() + bins, 0.0f);
        std::fill (detTarget[3].begin(), detTarget[3].begin() + bins, 0.0f);
    }
    else
    {
        for (int pair = 0; pair < 2; ++pair)
        {
            // The pair the mode names always runs; the other one only when a
            // band has been placed on it.
            const bool active = pair == mainPair || crossed;

            for (int c = 0; c < kMaxChannels; ++c)
            {
                const int d = 2 * pair + c;

                if (active && c < numChannels)
                    detect (pair == 0 ? source[c].data() : midSideSpec[c].data(), follower[d].data(), detTarget[d].data(),
                            pair == mainPair ? amount[c] : 1.0f, weights[d].data());
                else
                    std::fill (detTarget[d].begin(), detTarget[d].begin() + bins, 0.0f);
            }

            if (active && stereo && settings.link > 0.0f)
            {
                const float link = std::min (1.0f, settings.link);
                auto& a = detTarget[2 * pair];
                auto& b = detTarget[2 * pair + 1];

                for (int k = 0; k < bins; ++k)
                {
                    const float deepest = std::max (a[(size_t) k], b[(size_t) k]);
                    const float held = link * (1.0f - separation[pair][(size_t) k]);

                    a[(size_t) k] += held * (deepest - a[(size_t) k]);
                    b[(size_t) k] += held * (deepest - b[(size_t) k]);
                }
            }
        }
    }

    // What was found, handed to the processing frame in bins of its own size.
    const int frameBins = fftSize / 2 + 1;

    auto toFrame = [this, bins, frameBins, size] (const float* found, float* frame)
    {
        if (fftSize <= size)
        {
            // Coarser bins: each takes the most that falls in it, so that a
            // narrow cut is applied wider rather than shallower.
            const int ratio = size / fftSize;

            for (int k = 0; k < frameBins; ++k)
            {
                const int from = std::max (0, k * ratio - ratio / 2);
                const int to = std::min (bins - 1, k * ratio + ratio / 2);
                float most = 0.0f;

                for (int d = from; d <= to; ++d)
                    most = std::max (most, found[d]);

                frame[k] = most;
            }
        }
        else
        {
            // Finer bins: read between the detector's.
            const float ratio = (float) size / (float) fftSize;

            for (int k = 0; k < frameBins; ++k)
            {
                const float at = std::min ((float) (bins - 1), (float) k * ratio);
                const int d = std::min (bins - 2, (int) at);

                frame[k] = found[d] + (at - (float) d) * (found[d + 1] - found[d]);
            }
        }
    };

    for (int d = 0; d < kNumDetectors; ++d)
        toFrame (detTarget[d].data(), target[d].data());

    if (crossed)
        toFrame (detCrossing.data(), crossing.data());
    else
        std::fill (crossing.begin(), crossing.begin() + frameBins, 0.0f);

    // For the display: the spectrum as the detector saw it, which is the same
    // in every mode, and the reduction as it stands in the processing frame,
    // which is not.
    const float toFullScale = 16.0f / ((float) size * (float) size);   // a full-scale sine reads 0 dB
    const float toFrameBin = (float) fftSize / (float) size;

    for (int k = 0; k < bins; ++k)
    {
        const float at = std::min ((float) (frameBins - 1), (float) k * toFrameBin);
        const int f = std::min (frameBins - 2, (int) at);

        auto between = [f, at] (const std::vector<float>& v)
        {
            return v[(size_t) f] + (at - (float) f) * (v[(size_t) f + 1] - v[(size_t) f]);
        };

        // The deepest cut on this bin, each pair counting for its share of it.
        const float share = between (crossing);
        float reduction = 0.0f;

        for (int d = 0; d < kNumDetectors; ++d)
            reduction = std::max (reduction, between (state[d]) * (d / 2 == mainPair ? 1.0f - share : share));

        float in = -200.0f;

        for (int c = 0; c < numChannels; ++c)
            in = std::max (in, 10.0f * std::log10 (std::norm (detSpec[c][(size_t) k]) * toFullScale + 1.0e-12f));

        visIn[(size_t) k] = in;
        visOut[(size_t) k] = in - reduction;
        visRed[(size_t) k] = reduction;
    }

    ++frameCount;
}

void ResonanceSuppressor::processFrame (int numChannels) noexcept
{
    const int size = fftSize;
    const int bins = size / 2 + 1;
    const int mask = size - 1;
    const bool stereo = numChannels == 2;
    const bool mainIsMidSide = stereo && settings.stereoMode == StereoMode::midSide;

    for (int c = 0; c < numChannels; ++c)
    {
        auto* x = spec[c].data();

        for (int i = 0; i < size; ++i)
            x[i] = { inRing[c][(size_t) ((pos + i) & mask)] * window[(size_t) i], 0.0f };

        fft->forward (x);
    }

    const auto hopSeconds = (float) ((double) hop / sampleRate);
    const float attack = 1.0f - std::exp (-hopSeconds / (0.001f * std::max (0.05f, settings.attackMs)));
    const float release = 1.0f - std::exp (-hopSeconds / (0.001f * std::max (0.05f, settings.releaseMs)));

    // Each of the four cuts moves towards what its detector asked for.
    for (int d = 0; d < kNumDetectors; ++d)
    {
        for (int k = 0; k < bins; ++k)
        {
            float g = state[d][(size_t) k];
            const float wanted = target[d][(size_t) k];

            g += (wanted - g) * (wanted > g ? attack : release);
            state[d][(size_t) k] = g;
        }
    }

    auto gainOf = [this] (int d, int k)
    {
        const float g = state[d][(size_t) k];
        return g > 1.0e-4f ? std::exp (-g * kLn10Over20) : 1.0f;
    };

    if (! stereo)
    {
        auto* x = spec[0].data();

        for (int k = 0; k < bins; ++k)
        {
            const float gain = gainOf (0, k);
            x[k] *= gain;

            if (k > 0 && k < size / 2)
                x[size - k] *= gain;
        }
    }
    else
    {
        auto* xl = spec[0].data();
        auto* xr = spec[1].data();

        for (int k = 0; k < bins; ++k)
        {
            const auto l = xl[k], r = xr[k];

            // The same bin, cut once as left and right...
            const auto leftRightL = l * gainOf (0, k);
            const auto leftRightR = r * gainOf (1, k);

            // ...and once as mid and side.
            const auto m = 0.5f * (l + r) * gainOf (2, k);
            const auto s = 0.5f * (l - r) * gainOf (3, k);
            const auto midSideL = m + s;
            const auto midSideR = m - s;

            // The mode says which of the two it is; a band placed on the
            // other pair takes its own share of the bins under it.
            const float share = crossing[(size_t) k];
            const float asMidSide = mainIsMidSide ? 1.0f - share : share;

            xl[k] = leftRightL + asMidSide * (midSideL - leftRightL);
            xr[k] = leftRightR + asMidSide * (midSideR - leftRightR);

            if (k > 0 && k < size / 2)
            {
                xl[size - k] = std::conj (xl[k]);
                xr[size - k] = std::conj (xr[k]);
            }
        }
    }

    // Hann in, Hann out, four frames over every sample: they sum to 1.5.
    const float scale = 1.0f / 1.5f;

    for (int c = 0; c < numChannels; ++c)
    {
        auto* x = spec[c].data();

        fft->inverse (x);

        for (int i = 0; i < size; ++i)
            outRing[c][(size_t) ((pos + i) & mask)] += x[i].real() * window[(size_t) i] * scale;
    }
}

//==============================================================================
void ResonanceSuppressor::detect (const std::complex<float>* spectrum, float* memory, float* reduction,
                                  float amount, const float* weight) noexcept
{
    const int bins = detSize / 2 + 1;
    const float depth = std::max (0.0f, std::min (10.0f, settings.depth)) * amount;

    // A single frame of anything noisy is ragged: bins stand clear of their
    // neighbours by chance alone. The detector looks at a level averaged over
    // the last few frames, which a real resonance survives and chance does
    // not. It runs even when nothing is to be done, so that it is up to date
    // the moment something is.
    const auto hopSeconds = (float) ((double) detInterval / sampleRate);
    const float follow = 1.0f - std::exp (-hopSeconds / kDetectorSeconds);

    for (int k = 0; k < bins; ++k)
        memory[k] += follow * (std::norm (spectrum[k]) - memory[k]);

    if (depth <= 0.0f)
    {
        std::fill (reduction, reduction + bins, 0.0f);
        return;
    }

    // Detail is the scale at which the spectrum is read: a third of an octave
    // finds the broad humps and answers with broad cuts, a twenty-fourth
    // finds single whistles and answers with notches.
    const float scale = kBroadestOctaves * std::pow (kFinestOctaves / kBroadestOctaves, settings.detail * 0.1f);

    // Two passes of a box are a triangle, which leaves no corners.
    smoothLog (memory, scratch.data(), scale * 0.5f, 1);
    smoothLog (scratch.data(), fine.data(), scale * 0.5f, 1);

    prefix[0] = 0.0;

    for (int k = 0; k < bins; ++k)
        prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) fine[(size_t) k];

    auto mean = [this] (int from, int to)
    {
        return (float) ((prefix[(size_t) to + 1] - prefix[(size_t) from]) / (double) (to - from + 1));
    };

    // What a point is measured against: a stretch of spectrum on each side of
    // it, starting clear of the peak it may be the top of.
    const float nearDown = std::pow (2.0f, -kGapScales * scale);
    const float farDown = std::pow (2.0f, -(kGapScales + kSideScales) * scale);
    const float nearUp = 1.0f / nearDown;
    const float farUp = 1.0f / farDown;

    const float threshold = kThresholdDb - kThresholdPerDepth * depth;
    const float slope = depth * 0.1f * (settings.hard ? 2.0f : 1.0f);
    const float knee = settings.hard ? 0.0f : kKneeDb;

    // -120 dBFS: below it there is nothing to judge, and nothing is done.
    const float floor = (float) detSize * (float) detSize * (1.0e-12f / 16.0f);

    for (int k = 0; k < bins; ++k)
    {
        const int leftTo = std::min (k - kMinGapBins, (int) std::floor ((float) k * nearDown));
        const int leftFrom = std::max (1, std::min (leftTo - kMinSideBins + 1, (int) std::floor ((float) k * farDown)));
        const int rightFrom = std::max (k + kMinGapBins, (int) std::ceil ((float) k * nearUp));
        const int rightTo = std::min (bins - 1, std::max (rightFrom + kMinSideBins - 1, (int) std::ceil ((float) k * farUp)));

        float reductionHere = 0.0f;

        // Below the first few bins there is no left side to compare with, and
        // one side is not enough: every bass note would pass for a resonance.
        if (leftTo >= leftFrom)
        {
            // The higher of the two sides is the one that counts. A peak
            // clears both; a slope or an edge clears only one.
            float around = mean (leftFrom, leftTo);

            if (rightTo >= rightFrom)
                around = std::max (around, mean (rightFrom, rightTo));

            const float prominence = 10.0f * std::log10 ((fine[(size_t) k] + floor) / (around + floor));
            const float over = prominence - threshold;

            float bent = 0.0f;

            if (over >= knee * 0.5f)
                bent = over;
            else if (over > -knee * 0.5f)
                bent = (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

            // Fading to nothing as the level reaches the floor: silence, or a
            // sidechain with nothing in it, asks for no cut at all.
            const float present = fine[(size_t) k] / (fine[(size_t) k] + floor);

            reductionHere = std::min (kMaxReductionDb, slope * weight[k] * bent * present);
        }

        scratch[(size_t) k] = reductionHere;
    }

    // The cut has the shape of what it answers; this only rounds its corners.
    smoothLog (scratch.data(), reduction, scale * 0.5f, 1);
}

void ResonanceSuppressor::smoothLog (const float* in, float* out, float widthOctaves, int minHalfWidth) noexcept
{
    const int bins = detSize / 2 + 1;
    const float down = std::pow (2.0f, -0.5f * widthOctaves);
    const float up = 1.0f / down;

    prefix[0] = 0.0;

    for (int k = 0; k < bins; ++k)
        prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) in[k];

    for (int k = 0; k < bins; ++k)
    {
        const int lo = std::max (0, std::min (k - minHalfWidth, (int) std::floor ((float) k * down)));
        const int hi = std::min (bins - 1, std::max (k + minHalfWidth, (int) std::ceil ((float) k * up)));

        out[k] = (float) ((prefix[(size_t) hi + 1] - prefix[(size_t) lo]) / (double) (hi - lo + 1));
    }
}

} // namespace rb
