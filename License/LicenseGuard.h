#pragma once

#include "DemoNoise.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

namespace onirique::license
{

//==============================================================================
/** Tout ce qu'un plugin doit garder de sa licence : un membre de l'AudioProcessor.

        onirique::license::Guard license { "identifiant-du-plugin" };

    - prepareToPlay : license.refresh();   (beaucoup de DAW le rappellent juste avant un export)
    - fin de processBlock : license.process (buffer, getSampleRate(), isNonRealtime());

    La licence est relue toutes les cinq secondes : une activation faite dans ONIRIQUE CENTRAL
    est prise en compte sans recharger le plugin. */
class Guard final : private juce::Timer
{
public:
    explicit Guard (juce::String productSlug) : slug (std::move (productSlug))
    {
        refresh();
        startTimer (5000);
    }

    ~Guard() override { stopTimer(); }

    /** État de la licence sur ce PC (acheté, essai, démo). */
    Status status() const
    {
        const juce::ScopedLock lock (statusLock);
        return current;
    }

    /** Relit le fichier de licence écrit par ONIRIQUE CENTRAL. Jamais depuis le thread audio. */
    void refresh()
    {
        const auto status = read (slug);
        noise.setState (status.state);

        const juce::ScopedLock lock (statusLock);
        current = status;
    }

    /** Thread audio, une fois le son du plugin calculé. Rend vrai quand le bloc a été remplacé par du bruit. */
    template <typename Sample>
    bool process (juce::AudioBuffer<Sample>& buffer, double sampleRate, bool offline) noexcept
    {
        return noise.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples(), sampleRate, offline);
    }

private:
    void timerCallback() override { refresh(); }

    juce::String slug;
    juce::CriticalSection statusLock;
    Status current;
    DemoNoise noise;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Guard)
};

} // namespace onirique::license
