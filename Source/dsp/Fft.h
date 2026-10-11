// Onirique ResoBox — minimal radix-2 FFT (no external dependency)
#pragma once

#include <complex>
#include <vector>

namespace rb
{

/** Iterative in-place radix-2 complex FFT. Size is a power of two. */
class Fft
{
public:
    explicit Fft (int sizePowerOfTwo);

    int size() const noexcept { return n; }

    /** In-place forward transform of size() values. */
    void forward (std::complex<float>* data) const noexcept { transform (data, false); }

    /** In-place inverse transform, scaled by 1/N. */
    void inverse (std::complex<float>* data) const noexcept { transform (data, true); }

private:
    void transform (std::complex<float>* data, bool invert) const noexcept;

    int n = 0;
    std::vector<int> bitReverse;
    std::vector<std::complex<float>> twiddles; // forward twiddles, one block per stage
};

} // namespace rb
