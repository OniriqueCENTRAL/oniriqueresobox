// Onirique ResoBox — dynamic resonance suppressor (no dependency)
#pragma once

#include <complex>
#include <cstdint>
#include <memory>
#include <vector>

#include "Fft.h"

namespace rb
{

constexpr int kNumBands = 8;
constexpr int kMaxChannels = 2;
constexpr int kNumFrameSizes = 4;         // low latency, then the three qualities
constexpr float kMaxReductionDb = 24.0f;

/** One bell of the sensitivity curve: how much harder (or softer) the
    suppressor works around a frequency. */
enum class BandShape { bell, lowShelf, highShelf, plateau };
constexpr int kNumBandShapes = 4;

/** Where in the stereo picture a band is: everywhere, on one side, or on the
    middle or the sides. The last two can be asked for while the suppressor
    works on left and right, and the first two while it works on mid and
    side: around such a band, the work is done in the band's own terms. */
enum class BandPlacement { both, left, right, mid, side };
constexpr int kNumPlacements = 5;

/** The four signals a band can be placed on, and so the four detectors. */
constexpr int kNumDetectors = 4;    // left, right, mid, side

struct Band
{
    bool on = false;
    float freq = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    BandShape shape = BandShape::bell;
    BandPlacement placement = BandPlacement::both;
};

/** The height of one band's own curve at a frequency, in dB. */
float bandDb (const Band&, float hz) noexcept;

enum class StereoMode { stereo, midSide, mono };

struct Settings
{
    float depth = 5.0f;         // 0..10, how much of what sticks out is taken away
    float detail = 5.0f;        // 0..10, from broad and gentle to narrow and exact
    float attackMs = 5.0f;
    float releaseMs = 60.0f;
    bool hard = false;

    float link = 1.0f;          // 0 = each channel on its own, 1 = both get the deeper cut
    float balance = 0.0f;       // -1 = only left (or mid) is worked on, +1 = only right (or side)
    StereoMode stereoMode = StereoMode::stereo;

    float tiltDbPerOctave = 0.0f;   // leans the whole sensitivity curve about 1 kHz
    bool lowCutOn = true;
    float lowCutHz = 80.0f;
    bool highCutOn = false;
    float highCutHz = 16000.0f;
    Band bands[kNumBands];

    float mix = 1.0f;
    float outDb = 0.0f;
    bool autoGain = false;      // give back the level the cuts took away
    bool delta = false;         // hear only what is being removed
    bool bypass = false;
    bool sidechain = false;     // detect on the sidechain input instead of the signal
    int quality = 0;            // 0 = normal, 1 = high, 2 = ultra: longer frames
    bool lowLatency = false;    // the shortest frame, whatever the quality
};

/** The sensitivity curve drawn in the editor, in dB, at one frequency. 0 means
    the suppressor works as set; the cuts take it towards minus infinity.
    With a detector given (0 left, 1 right, 2 mid, 3 side), only the bands
    placed on it, and those placed everywhere, count. */
float sensitivityDb (const Settings&, float hz, int detector = -1) noexcept;

/** A band's own shape, from 0 to 1, whatever its height. */
float bandShape (const Band&, float hz) noexcept;

/** Finds what sticks out of the spectrum and turns it down, bin by bin.

    A resonance is a place where the spectrum stands above what lies on both
    sides of it. A slope, a shelf or the edge of a band-limited signal stands
    above one side only, and is left alone: that is the difference between
    this and a compressor with many bands. What is found is brought back
    towards its surroundings with a cut of its own shape. Only the magnitude
    is touched, never the phase, and because the reference is the signal's own
    spectrum, none of it depends on level.
*/
class ResonanceSuppressor
{
public:
    /** Allocates. Everything after this is free of allocation. */
    void prepare (double sampleRate, int numChannels);
    void reset() noexcept;

    void setSettings (const Settings&) noexcept;

    /** The delay through process(), which is also the delay of the dry path. */
    int latency() const noexcept { return fftSize; }
    static int latencyFor (double sampleRate, int quality, bool lowLatency) noexcept;

    /** In place. sidechain may be null, as may any of its channels. */
    void process (float* const* io, const float* const* sidechain, int numChannels, int numSamples) noexcept;

    // What the editor draws. Written once per frame on the audio thread.
    // One value per bin of the detector's frame, which does not change with
    // the quality: the picture keeps its resolution in every mode.
    int getDisplayBins() const noexcept      { return detSize / 2 + 1; }
    double getDisplayBinHz() const noexcept  { return sampleRate / (double) detSize; }
    uint32_t getFrameCount() const noexcept  { return frameCount; }
    const float* getInputDb() const noexcept     { return visIn.data(); }
    const float* getOutputDb() const noexcept    { return visOut.data(); }
    const float* getReductionDb() const noexcept { return visRed.data(); }

private:
    void configure (int frame) noexcept;
    void updateWeights() noexcept;
    void runDetector (int numChannels, bool useSidechain) noexcept;
    void processFrame (int numChannels) noexcept;
    void detect (const std::complex<float>* spectrum, float* memory, float* reduction, float amount,
                 const float* weight) noexcept;
    void smoothLog (const float* in, float* out, float widthOctaves, int minHalfWidth) noexcept;

    Settings settings;
    double sampleRate = 48000.0;
    int baseOrder = 11;
    int activeFrame = -1;
    int fftSize = 2048, hop = 512;
    int pos = 0, hopCount = 0;
    uint32_t frameCount = 0;
    bool weightsDirty = true;

    std::unique_ptr<Fft> ffts[kNumFrameSizes];
    const Fft* fft = nullptr;

    // The processing frame: its length is the latency, and the precision
    // with which a cut can be applied.
    std::vector<float> window;
    std::vector<float> inRing[kMaxChannels], outRing[kMaxChannels];
    std::vector<std::complex<float>> spec[kMaxChannels];
    std::vector<float> target[kNumDetectors], state[kNumDetectors];

    // How much of each bin is worked on in the other pair of signals than
    // the one the mode names: 0 nowhere near a band placed there, 1 under it.
    std::vector<float> crossing;

    // The detector: one frame length for every mode, read from a history of
    // the input long enough to hold it.
    const Fft* detFft = nullptr;
    int detSize = 4096, detInterval = 1024, detCount = 0;
    int historyPos = 0, historyMask = 0;
    std::vector<float> detWindow;
    std::vector<float> inHistory[kMaxChannels], scHistory[kMaxChannels];
    std::vector<std::complex<float>> detSpec[kMaxChannels], scSpec[kMaxChannels], midSideSpec[kMaxChannels];
    std::vector<float> detTarget[kNumDetectors], follower[kNumDetectors];
    std::vector<float> fine, scratch;
    std::vector<float> weights[kNumDetectors + 1];  // one curve per detector, and one with every band in it
    std::vector<float> detCrossing;
    bool anyCrossing = false;

    // Where a band is placed on one signal of a pair only, the link between
    // the two is let go: 0 away from such a band, 1 under it. One per pair.
    std::vector<float> separation[2];
    std::vector<double> prefix;
    std::vector<float> visIn, visOut, visRed;

    // Output stage, smoothed so that nothing clicks when a control moves.
    float mixNow = 1.0f, gainNow = 1.0f, deltaNow = 0.0f, bypassNow = 0.0f;
    float smoothing = 0.001f;
    bool snap = true;

    // Auto gain: the level going in and the level coming out, each averaged
    // over a few hundred milliseconds, and the gain that makes them equal.
    double dryPower = 0.0, processedPower = 0.0;
    float makeupNow = 1.0f, powerSmoothing = 0.0001f;
};

} // namespace rb
