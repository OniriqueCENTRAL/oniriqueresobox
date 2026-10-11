// The whole audio path, offline: what the plugin does to a signal is decided
// in ResonanceSuppressor, and this is where it is held to it.
#include "dsp/ResonanceSuppressor.h"
#include "TestSignal.h"

#include <chrono>
#include <cstdio>

namespace
{
constexpr double kSr = 48000.0;
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what);

    if (! ok)
        ++failures;
}

/** Runs a signal through, both channels alike unless a second one is given. */
std::vector<float> run (const rb::Settings& settings, const std::vector<float>& left, int block = 512,
                        const std::vector<float>* sidechain = nullptr, std::vector<float>* rightInOut = nullptr)
{
    rb::ResonanceSuppressor suppressor;
    suppressor.setSettings (settings);
    suppressor.prepare (kSr, 2);

    auto l = left;
    auto r = rightInOut != nullptr ? *rightInOut : left;
    auto sc = sidechain != nullptr ? *sidechain : std::vector<float>();

    for (size_t at = 0; at < l.size(); at += (size_t) block)
    {
        const auto count = (int) std::min ((size_t) block, l.size() - at);
        float* io[2] = { l.data() + at, r.data() + at };
        const float* side[2] = { sc.empty() ? nullptr : sc.data() + at, sc.empty() ? nullptr : sc.data() + at };

        suppressor.process (io, sc.empty() ? nullptr : side, 2, count);
    }

    if (rightInOut != nullptr)
        *rightInOut = r;

    return l;
}

int latencyOf (const rb::Settings& s)
{
    return rb::ResonanceSuppressor::latencyFor (kSr, s.quality, s.lowLatency);
}

/** The largest difference between the output and the input delayed, in dB. */
double worstErrorDb (const std::vector<float>& in, const std::vector<float>& out, int delay)
{
    double worst = 0.0;

    for (size_t i = (size_t) delay; i < out.size(); ++i)
        worst = std::max (worst, (double) std::abs (out[i] - in[i - (size_t) delay]));

    return 20.0 * std::log10 (worst + 1.0e-12);
}

std::vector<float> withTone (std::vector<float> signal, double hz, float amplitude)
{
    for (size_t i = 0; i < signal.size(); ++i)
        signal[i] += amplitude * (float) std::sin (2.0 * M_PI * hz * (double) i / kSr);

    return signal;
}

/** The power between two frequencies over the last second of a signal, in dB. */
double bandDb (const std::vector<float>& x, double fromHz, double toHz)
{
    constexpr int size = 8192;
    static const rb::Fft fft (size);

    std::vector<std::complex<float>> frame ((size_t) size);
    const size_t start = x.size() - (size_t) kSr;
    double sum = 0.0;

    for (size_t at = start; at + size <= x.size(); at += size / 2)
    {
        for (int i = 0; i < size; ++i)
            frame[(size_t) i] = { x[at + (size_t) i] * (0.5f - 0.5f * (float) std::cos (2.0 * M_PI * i / size)), 0.0f };

        fft.forward (frame.data());

        for (int k = (int) (fromHz * size / kSr); k <= (int) (toHz * size / kSr); ++k)
            sum += std::norm (frame[(size_t) k]);
    }

    return 10.0 * std::log10 (sum + 1.0e-20);
}

/** A steep low-pass, for building spectra with an edge in them. */
std::vector<float> lowPass (const std::vector<float>& x, double hz)
{
    constexpr int half = 200;
    std::vector<float> taps (2 * half + 1);
    const double fc = hz / kSr;

    for (int i = -half; i <= half; ++i)
    {
        const double sinc = i == 0 ? 2.0 * fc : std::sin (2.0 * M_PI * fc * i) / (M_PI * i);
        taps[(size_t) (i + half)] = (float) (sinc * (0.5 + 0.5 * std::cos (M_PI * i / (half + 1.0))));
    }

    std::vector<float> out (x.size(), 0.0f);

    for (size_t n = (size_t) half; n + half < x.size(); ++n)
    {
        float sum = 0.0f;

        for (int i = -half; i <= half; ++i)
            sum += taps[(size_t) (i + half)] * x[n - (size_t) half + (size_t) (i + half)];

        out[n] = sum;
    }

    return out;
}

rb::Settings frame (int index)
{
    rb::Settings s;
    s.lowCutOn = false;
    s.lowLatency = index == 0;
    s.quality = std::max (0, index - 1);
    return s;
}

const char* frameName (int index)
{
    static const char* names[] = { "low latency", "normal", "high", "ultra" };
    return names[index];
}
}

int main()
{
    const auto noise = rbtest::makeNoise (3.0, kSr, 0.1f);
    const size_t tail = (size_t) kSr;                       // measure on the last second
    const auto* noiseTail = noise.data() + noise.size() - tail;

    // Noise with a ringing peak at 3 kHz about 12 dB proud of it: the thing
    // the plugin exists for.
    const auto ringing = rbtest::makeResonantNoise (3.0, kSr, { { 3000.0, 20.0, 2.0f } });
    const auto ringBefore = bandDb (ringing, 2900.0, 3100.0);
    const auto besideBefore = bandDb (ringing, 1000.0, 1800.0);

    auto ringChange = [&] (const rb::Settings& s)
    {
        return bandDb (run (s, ringing), 2900.0, 3100.0) - ringBefore;
    };

    std::printf ("transparency\n");
    {
        for (int i = 0; i < rb::kNumFrameSizes; ++i)
        {
            auto s = frame (i);
            s.depth = 0.0f;

            const auto error = worstErrorDb (noise, run (s, noise), latencyOf (s));

            std::printf ("    %-11s: latency %5d, worst error %.1f dB\n", frameName (i), latencyOf (s), error);
            check (error < -100.0, "depth 0 returns the input, delayed by the reported latency");
        }

        rb::Settings s;
        s.bypass = true;
        s.depth = 10.0f;
        check (worstErrorDb (noise, run (s, noise), latencyOf (s)) < -100.0, "bypass returns the input, delayed alike");
    }

    std::printf ("block size\n");
    {
        rb::Settings s;
        s.depth = 7.0f;

        const auto reference = run (s, ringing, 512);
        check (run (s, ringing, 64) == reference, "64 samples at a time gives the same samples as 512");
        check (run (s, ringing, 333) == reference, "so does 333");
        check (run (s, ringing, 4096) == reference, "so does 4096");
    }

    std::printf ("a resonance is turned down, and only it\n");
    {
        rb::Settings s;
        s.lowCutOn = false;

        double atDefault = 0.0;

        for (const float depth : { 2.0f, 5.0f, 8.0f, 10.0f })
        {
            s.depth = depth;
            const auto out = run (s, ringing);
            const auto ring = bandDb (out, 2900.0, 3100.0) - ringBefore;
            const auto beside = bandDb (out, 1000.0, 1800.0) - besideBefore;
            const auto plain = run (s, noise);
            const auto flat = rbtest::rmsDb (plain.data() + plain.size() - tail, tail) - rbtest::rmsDb (noiseTail, tail);

            std::printf ("    depth %4.1f: resonance %+6.2f dB, beside it %+5.2f dB, plain noise %+5.2f dB\n",
                         depth, ring, beside, flat);

            if (depth == 5.0f)
            {
                atDefault = ring;
                check (ring < -3.0, "the default depth takes more than 3 dB off the resonance");
                check (beside > -0.5, "and less than half a dB off the spectrum beside it");
                check (flat > -0.5, "and less than half a dB off noise with nothing in it");
            }

            if (depth == 10.0f)
                check (flat > -1.5, "even at full depth, plain noise loses less than 1.5 dB");
        }

        s.depth = 5.0f;
        s.hard = true;
        const auto hard = ringChange (s);
        std::printf ("    hard: resonance %+.2f dB\n", hard);
        check (hard < atDefault - 1.0, "hard goes deeper than soft");
    }

    std::printf ("the frame size does not move the threshold\n");
    {
        double least = 0.0, most = -100.0, flatLeast = 0.0, flatMost = -100.0;

        for (int i = 0; i < rb::kNumFrameSizes; ++i)
        {
            auto s = frame (i);
            s.depth = 6.0f;

            const auto ring = ringChange (s);
            const auto plain = run (s, noise);
            const auto flat = rbtest::rmsDb (plain.data() + plain.size() - tail, tail) - rbtest::rmsDb (noiseTail, tail);

            std::printf ("    %-11s: resonance %+6.2f dB, plain noise %+5.2f dB\n", frameName (i), ring, flat);

            least = std::min (least, ring);
            most = std::max (most, ring);
            flatLeast = std::min (flatLeast, flat);
            flatMost = std::max (flatMost, flat);
        }

        check (most - least < 0.5, "the same resonance loses the same within half a dB, whatever the frame");
        check (flatMost - flatLeast < 0.5, "and plain noise the same within half a dB");
    }

    std::printf ("nor what is found in a note with close harmonics\n");
    {
        // A low note, harmonics 150 Hz apart, the sixth of them 12 dB too
        // loud. A short frame cannot tell one harmonic from the next; what is
        // found must not depend on it all the same.
        std::vector<float> note (noise.size());

        for (size_t i = 0; i < note.size(); ++i)
        {
            double sum = 0.0;

            for (int h = 1; h <= 40; ++h)
                sum += (h == 6 ? 4.0 : 1.0) / h * std::sin (2.0 * M_PI * 150.0 * h * (double) i / kSr + 0.7 * h * h);

            note[i] = (float) (0.08 * sum) + 0.01f * noise[i];
        }

        auto level = [&] (const std::vector<float>& x, double hz)
        {
            return rbtest::toneLevelDb (x.data() + x.size() - tail, tail, hz, kSr);
        };

        double least = 0.0, most = -100.0, besideLeast = 0.0, besideMost = -100.0;

        for (int i = 0; i < rb::kNumFrameSizes; ++i)
        {
            auto s = frame (i);
            s.depth = 6.0f;
            s.detail = 7.0f;

            const auto out = run (s, note);
            const auto loud = level (out, 900.0) - level (note, 900.0);
            const auto beside = level (out, 1200.0) - level (note, 1200.0);

            std::printf ("    %-11s: the loud harmonic %+6.2f dB, the one two above it %+5.2f dB\n",
                         frameName (i), loud, beside);

            least = std::min (least, loud);
            most = std::max (most, loud);
            besideLeast = std::min (besideLeast, beside);
            besideMost = std::max (besideMost, beside);
        }

        check (most < -3.0, "the harmonic that sticks out is turned down in every frame");
        check (most - least < 0.75, "by the same amount within 0.75 dB");
        check (besideMost - besideLeast < 1.5, "and its neighbours are treated alike within 1.5 dB");
    }

    std::printf ("what is not a resonance is left alone\n");
    {
        rb::Settings s;
        s.lowCutOn = false;
        s.depth = 8.0f;

        // A shelf: everything above 2 kHz is 14 dB louder. A compressor with
        // many bands would pull it down; there is nothing here to find.
        const auto low = lowPass (noise, 2000.0);
        auto shelf = noise;

        for (size_t i = 0; i < shelf.size(); ++i)
            shelf[i] = low[i] + 5.0f * (noise[i] - low[i]);

        auto out = run (s, shelf);
        const auto above = bandDb (out, 2200.0, 6000.0) - bandDb (shelf, 2200.0, 6000.0);
        const auto below = bandDb (out, 300.0, 1800.0) - bandDb (shelf, 300.0, 1800.0);
        std::printf ("    a 14 dB shelf at 2 kHz: above %+.2f dB, below %+.2f dB\n", above, below);
        check (above > -1.0 && below > -1.0, "a shelf is not pulled down");

        // The edge of a band-limited signal, as an MP3 or a telephone leaves.
        const auto limited = lowPass (noise, 8000.0);
        out = run (s, limited);
        const auto edge = bandDb (out, 6000.0, 7900.0) - bandDb (limited, 6000.0, 7900.0);
        std::printf ("    under an edge at 8 kHz: %+.2f dB\n", edge);
        check (edge > -1.0, "nor is the top of a band-limited signal");

        // A single sine well clear of everything is as resonant as it gets.
        const auto whistle = withTone (noise, 3000.0, 0.03f);
        const auto before = rbtest::toneLevelDb (whistle.data() + whistle.size() - tail, tail, 3000.0, kSr);

        for (const float detail : { 2.0f, 5.0f, 9.0f })
        {
            s.detail = detail;
            out = run (s, whistle);
            const auto change = rbtest::toneLevelDb (out.data() + out.size() - tail, tail, 3000.0, kSr) - before;
            std::printf ("    a whistle at 3 kHz, detail %.0f: %+.2f dB\n", detail, change);

            if (detail == 9.0f)
                check (change < -5.0, "a whistle is found at high detail");
        }
    }

    std::printf ("the sensitivity curve\n");
    {
        rb::Settings s;
        s.lowCutOn = false;
        const auto plain = ringChange (s);

        s.highCutOn = true;
        s.highCutHz = 1000.0f;
        auto change = ringChange (s);
        std::printf ("    behind a cut at 1 kHz: %+.2f dB\n", change);
        check (change > -0.3, "a cut below the resonance leaves it alone");

        s.highCutOn = false;
        s.bands[2] = { true, 3000.0f, 12.0f, 1.0f };
        change = ringChange (s);
        std::printf ("    under a +12 dB band: %+.2f dB (plain %+.2f)\n", change, plain);
        check (change < plain - 2.0, "a raised band works it harder");

        s.bands[2].gainDb = -12.0f;
        change = ringChange (s);
        std::printf ("    under a -12 dB band: %+.2f dB\n", change);
        check (change > plain + 2.0, "a lowered band spares it");

        // The other shapes of a band. The resonance is at 3 kHz.
        s.bands[2] = { true, 1000.0f, 12.0f, 1.0f, rb::BandShape::highShelf };
        const auto aboveShelf = ringChange (s);
        s.bands[2].shape = rb::BandShape::lowShelf;
        const auto pastShelf = ringChange (s);
        std::printf ("    +12 dB shelf at 1 kHz: high %+.2f dB, low %+.2f dB\n", aboveShelf, pastShelf);
        check (aboveShelf < plain - 2.0 && std::abs (pastShelf - plain) < 0.3,
               "a shelf raises its own side and leaves the other alone");

        s.bands[2] = { true, 2200.0f, 12.0f, 1.0f, rb::BandShape::bell };
        const auto offBell = ringChange (s);
        s.bands[2].shape = rb::BandShape::plateau;
        const auto onPlateau = ringChange (s);
        std::printf ("    +12 dB at 2.2 kHz: bell %+.2f dB, plateau %+.2f dB\n", offBell, onPlateau);
        check (onPlateau < offBell - 1.0, "a plateau keeps its full height further out than a bell");

        s.bands[2].on = false;
        s.tiltDbPerOctave = -6.0f;
        change = ringChange (s);
        std::printf ("    with the curve tilted -6 dB per octave: %+.2f dB\n", change);
        check (change > plain + 1.0, "tilting away from the highs spares it");
    }

    std::printf ("output stage\n");
    {
        rb::Settings s;
        s.depth = 8.0f;
        const int delay = latencyOf (s);
        const auto wet = run (s, ringing);
        s.delta = true;
        const auto removed = run (s, ringing);

        auto sum = wet;
        for (size_t i = 0; i < sum.size(); ++i)
            sum[i] += removed[i];

        check (worstErrorDb (ringing, sum, delay) < -100.0, "delta plus the output is the input");

        s.delta = false;
        s.mix = 0.0f;
        check (worstErrorDb (ringing, run (s, ringing), delay) < -100.0, "mix at 0 is the input");

        s.mix = 1.0f;
        s.outDb = -6.0f;
        const auto quieter = run (s, ringing);
        const auto change = rbtest::rmsDb (quieter.data() + quieter.size() - tail, tail)
                            - rbtest::rmsDb (wet.data() + wet.size() - tail, tail);
        check (std::abs (change + 6.0) < 0.01, "output gain is in dB");
    }

    std::printf ("auto gain\n");
    {
        // Several loud resonances, so that taking them away costs real level.
        const auto loud = rbtest::makeResonantNoise (3.0, kSr, { { 700.0, 15.0, 4.0f }, { 1900.0, 20.0, 5.0f },
                                                                 { 3000.0, 20.0, 5.0f }, { 5200.0, 25.0, 5.0f } });
        const auto before = rbtest::rmsDb (loud.data() + loud.size() - tail, tail);

        rb::Settings s;
        s.depth = 10.0f;
        s.lowCutOn = false;

        auto out = run (s, loud);
        const auto without = rbtest::rmsDb (out.data() + out.size() - tail, tail) - before;

        s.autoGain = true;
        out = run (s, loud);
        const auto with = rbtest::rmsDb (out.data() + out.size() - tail, tail) - before;

        std::printf ("    level after the cuts: %+.2f dB without, %+.2f dB with\n", without, with);
        check (without < -1.0, "the cuts cost level");
        check (std::abs (with) < 0.3, "auto gain gives it back to within 0.3 dB");

        s.mix = 0.5f;
        out = run (s, loud);
        const auto half = rbtest::rmsDb (out.data() + out.size() - tail, tail) - before;
        std::printf ("    at 50 %% mix, with: %+.2f dB\n", half);
        check (std::abs (half) < 0.3, "and does so whatever the mix");

        s = {};
        s.depth = 0.0f;
        s.autoGain = true;
        check (worstErrorDb (loud, run (s, loud), latencyOf (s)) < -100.0, "with nothing cut, it gives nothing back");
    }

    std::printf ("stereo\n");
    {
        // The resonance is on the left. The right is plain noise: what happens
        // to it at 3 kHz shows only what the left's cut did to it.
        const auto other = rbtest::makeNoise (3.0, kSr, 0.1f, 99);
        const auto otherBefore = bandDb (other, 2900.0, 3100.0);

        auto sides = [&] (rb::Settings s, double& left, double& right)
        {
            s.depth = 8.0f;
            s.lowCutOn = false;

            auto r = other;
            const auto l = run (s, ringing, 512, nullptr, &r);
            left = bandDb (l, 2900.0, 3100.0) - ringBefore;
            right = bandDb (r, 2900.0, 3100.0) - otherBefore;
        };

        double left = 0.0, right = 0.0, linkedRight = 0.0;
        rb::Settings s;

        s.link = 0.0f;
        sides (s, left, right);
        std::printf ("    unlinked: left %+.2f dB, right %+.2f dB\n", left, right);
        check (left < -3.0 && right > -0.7, "unlinked, only the side with the resonance is cut");

        s.link = 1.0f;
        sides (s, left, linkedRight);
        std::printf ("    linked:   left %+.2f dB, right %+.2f dB\n", left, linkedRight);
        check (linkedRight < right - 2.0, "linked, the cut is carried across");

        s.stereoMode = rb::StereoMode::mono;
        sides (s, left, right);
        std::printf ("    mono:     left %+.2f dB, right %+.2f dB\n", left, right);
        check (left < -3.0 && std::abs (left - right) < 0.6, "mono gives both sides the same cut");

        s.stereoMode = rb::StereoMode::stereo;
        s.link = 0.0f;
        s.balance = 1.0f;
        sides (s, left, right);
        std::printf ("    balance fully right: left %+.2f dB\n", left);
        check (left > -0.1, "balance spares the side it leans away from");

        s = {};
        s.depth = 0.0f;
        s.stereoMode = rb::StereoMode::midSide;
        auto r = other;
        const auto l = run (s, noise, 512, nullptr, &r);
        check (worstErrorDb (noise, l, latencyOf (s)) < -100.0 && worstErrorDb (other, r, latencyOf (s)) < -100.0,
               "mid/side decodes back to left/right");
    }

    std::printf ("a band placed on one part of the stereo picture\n");
    {
        // The same ringing signal on both sides: all of it is in the middle.
        auto left = [&] (const rb::Settings& s)
        {
            auto r = ringing;
            return bandDb (run (s, ringing, 512, nullptr, &r), 2900.0, 3100.0) - ringBefore;
        };

        rb::Settings s;
        s.lowCutOn = false;
        s.link = 0.0f;
        const auto plain = left (s);

        auto with = [&] (rb::BandPlacement placement)
        {
            s.bands[0] = { true, 3000.0f, 12.0f, 1.0f, rb::BandShape::bell, placement };
            return left (s);
        };

        const auto onRight = with (rb::BandPlacement::right);
        const auto onLeft = with (rb::BandPlacement::left);
        std::printf ("    +12 dB band at 3 kHz: on the right %+.2f dB, on the left %+.2f dB (no band %+.2f)\n",
                     onRight, onLeft, plain);
        check (std::abs (onRight - plain) < 0.1 && onLeft < plain - 2.0, "a band placed on one side works on that side only");

        // The link is on by default, and would hand the left's deeper cut to
        // the right: under a placed band it must let go.
        {
            rb::Settings linked;
            linked.lowCutOn = false;
            linked.bands[0] = { true, 3000.0f, 12.0f, 1.0f, rb::BandShape::bell, rb::BandPlacement::left };

            auto r = ringing;
            const auto l = run (linked, ringing, 512, nullptr, &r);
            const auto cutLeft = bandDb (l, 2900.0, 3100.0) - ringBefore;
            const auto cutRight = bandDb (r, 2900.0, 3100.0) - ringBefore;

            std::printf ("    with the link at 100 %%, a band on the left: left %+.2f dB, right %+.2f dB\n", cutLeft, cutRight);
            check (cutLeft < cutRight - 2.0 && std::abs (cutRight - plain) < 0.3,
                   "the link does not carry a placed band's cut to the other side");
        }

        // Mid and side, asked for while the mode is left and right.
        const auto onMid = with (rb::BandPlacement::mid);
        const auto onSide = with (rb::BandPlacement::side);
        std::printf ("    the same on the mid %+.2f dB, on the side %+.2f dB\n", onMid, onSide);
        check (onMid < plain - 2.0, "a band on the mid works the middle harder");
        check (std::abs (onSide - plain) < 0.3, "a band on the side leaves a centred signal as it was");

        // And left, asked for while the mode is mid and side.
        s.stereoMode = rb::StereoMode::midSide;
        s.bands[0].on = false;
        const auto plainMidSide = left (s);
        const auto leftInMidSide = with (rb::BandPlacement::left);
        std::printf ("    in mid/side mode: no band %+.2f dB, a band on the left %+.2f dB\n", plainMidSide, leftInMidSide);
        check (leftInMidSide < plainMidSide - 2.0, "the other way about works too");

        // Nothing of this may cost the plain case its exactness.
        s = {};
        s.depth = 0.0f;
        s.bands[0] = { true, 3000.0f, 12.0f, 1.0f, rb::BandShape::bell, rb::BandPlacement::mid };
        auto r = rbtest::makeNoise (3.0, kSr, 0.1f, 99);
        const auto other = r;
        const auto l = run (s, noise, 512, nullptr, &r);
        check (worstErrorDb (noise, l, latencyOf (s)) < -100.0 && worstErrorDb (other, r, latencyOf (s)) < -100.0,
               "at depth 0 a crossing band still returns the input");
    }

    std::printf ("sidechain\n");
    {
        // The resonance is in the sidechain, not in the signal: the signal is
        // cut where the sidechain rings.
        rb::Settings s;
        s.depth = 10.0f;
        s.lowCutOn = false;
        s.sidechain = true;

        const auto out = run (s, noise, 512, &ringing);
        const auto change = bandDb (out, 2900.0, 3100.0) - bandDb (noise, 2900.0, 3100.0);
        std::printf ("    3 kHz in the signal: %+.2f dB\n", change);
        check (change < -3.0, "the sidechain decides where the cut goes");

        const std::vector<float> silence (noise.size(), 0.0f);
        check (worstErrorDb (noise, run (s, noise, 512, &silence), latencyOf (s)) < -100.0,
               "a silent sidechain cuts nothing, even at full depth");
    }

    std::printf ("speed\n");
    {
        const auto longer = rbtest::makeResonantNoise (10.0, kSr, { { 900.0, 30.0, 6.0f }, { 3200.0, 40.0, 6.0f } });

        for (int i = 0; i < rb::kNumFrameSizes; ++i)
        {
            auto s = frame (i);
            s.depth = 6.0f;

            const auto start = std::chrono::steady_clock::now();
            run (s, longer, 256);
            const std::chrono::duration<double> taken = std::chrono::steady_clock::now() - start;

            std::printf ("    %-11s, stereo, blocks of 256: %.0fx real time\n", frameName (i), 10.0 / taken.count());
        }
    }

    std::printf (failures == 0 ? "\nall passed\n" : "\n%d FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
