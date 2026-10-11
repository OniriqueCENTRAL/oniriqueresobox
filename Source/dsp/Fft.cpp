#include "Fft.h"

#include <cassert>
#include <cmath>

namespace rb
{

Fft::Fft (int sizePowerOfTwo)
    : n (sizePowerOfTwo)
{
    assert (n > 0 && (n & (n - 1)) == 0);

    bitReverse.resize ((size_t) n);
    int bits = 0;
    while ((1 << bits) < n)
        ++bits;

    for (int i = 0; i < n; ++i)
    {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b))
                r |= 1 << (bits - 1 - b);
        bitReverse[(size_t) i] = r;
    }

    // Twiddles laid out stage by stage: for len = 2,4,8,... store len/2 factors.
    twiddles.reserve ((size_t) n);
    for (int len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * M_PI / (double) len;
        for (int k = 0; k < len / 2; ++k)
            twiddles.push_back (std::complex<float> ((float) std::cos (ang * k),
                                                     (float) std::sin (ang * k)));
    }
}

void Fft::transform (std::complex<float>* data, bool invert) const noexcept
{
    for (int i = 0; i < n; ++i)
    {
        const int j = bitReverse[(size_t) i];
        if (i < j)
            std::swap (data[i], data[j]);
    }

    size_t twBase = 0;

    for (int len = 2; len <= n; len <<= 1)
    {
        const int half = len / 2;

        for (int start = 0; start < n; start += len)
        {
            for (int k = 0; k < half; ++k)
            {
                std::complex<float> w = twiddles[twBase + (size_t) k];
                if (invert)
                    w = std::conj (w);

                const auto u = data[start + k];
                const auto v = data[start + k + half] * w;

                data[start + k]        = u + v;
                data[start + k + half] = u - v;
            }
        }

        twBase += (size_t) half;
    }

    if (invert)
    {
        const float scale = 1.0f / (float) n;
        for (int i = 0; i < n; ++i)
            data[i] *= scale;
    }
}

} // namespace rb
