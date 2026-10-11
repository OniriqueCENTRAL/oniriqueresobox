#pragma once

#include "License.h"

#include <atomic>

namespace onirique::license
{

//==============================================================================
/** Le bruit blanc du mode démo, à poser en fin de processBlock.

    - plugin acheté : rien ;
    - essai gratuit : l'écoute en temps réel reste intacte, un export (rendu hors temps réel)
      ne contient que du bruit ;
    - pas de licence, ou essai terminé : l'export est du bruit, et l'écoute en temps réel
      est coupée par deux secondes de bruit toutes les trente secondes. */
class DemoNoise
{
public:
    /** L'état lu dans la licence. Depuis n'importe quel thread. */
    void setState (State newState) noexcept { state.store ((int) newState); }

    /** Thread audio. Rend vrai quand le bloc a été remplacé par du bruit. */
    template <typename Sample>
    bool process (Sample* const* channels, int numChannels, int numSamples, double sampleRate, bool offline) noexcept
    {
        const auto current = (State) state.load();
        if (current == State::licensed)
            return false;

        bool noisy = offline;

        if (! offline && current == State::unlicensed)
        {
            const auto period = (juce::int64) (sampleRate * periodSeconds);
            noisy = period > 0 && position % period >= period - (juce::int64) (sampleRate * burstSeconds);
            position += numSamples;
        }

        if (! noisy)
            return false;

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                channels[ch][i] = (Sample) ((random.nextFloat() * 2.0f - 1.0f) * 0.25f);

        return true;
    }

    static constexpr double periodSeconds = 30.0, burstSeconds = 2.0;

private:
    std::atomic<int> state { (int) State::unlicensed };
    juce::int64 position = 0;
    juce::Random random;
};

} // namespace onirique::license
