#ifndef ONIRIQUE_THEME_H   // (une copie dans License/ aussi : la même garde pour les deux)
#define ONIRIQUE_THEME_H

#include <juce_graphics/juce_graphics.h>
#include <atomic>

//==============================================================================
//  La couleur de l'interface (menu ⋯ → INTERFACE COLOUR), la même dans tous les plugins ONIRIQUE : la teinte choisie
//  remplace la braise partout (potards, verre allumé, lueurs, courbes), avec ses dérivées (claire, profonde, rose).
//  Elle est gardée dans AppData/Onirique/interface-colour.txt : un choix dans un plugin s'applique aux autres (relu une
//  fois par seconde par chaque fenêtre ouverte, poll()).
//
//  Ce fichier se copie tel quel dans chaque plugin, à côté d'OniriqueBar.h.
//==============================================================================
namespace onirique::look
{
struct Swatch { const char* name; juce::uint32 argb; };
inline constexpr Swatch swatches[] { { "Ember", 0xffE5384A }, { "Rose", 0xffFF4F8B }, { "Violet", 0xff9B6BFF }, { "Blue", 0xff3D8BFF },
                                     { "Lagoon", 0xff22C7D6 }, { "Mint", 0xff2FD08A }, { "Gold", 0xffF0B43C }, { "Silver", 0xffC9C2C4 } };
inline constexpr int numSwatches = (int) (sizeof (swatches) / sizeof (swatches[0]));

// la teinte et ses dérivées (thread de l'interface seulement)
inline juce::Colour ember { 0xffE5384A }, emberBar { 0xffE23444 }, emberLit { 0xffff5a68 }, emberPink { 0xffff8e9a },
                    emberDeep { 0xff6e0a18 }, emberBlood { 0xff960a1e }, litTop { 0xff5a0d17 }, litBottom { 0xff33060b };
inline juce::Colour deepTint { 0xff1a080a }, deepTint2 { 0xff2a0a0e };   // fonds très sombres teintés (page de licence)
inline std::atomic<int> version { 0 };   // change à chaque nouvelle teinte : les fenêtres se redessinent
inline juce::int64 seenStamp = 0;

inline juce::File file()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Onirique/interface-colour.txt");
}

inline void apply (juce::Colour c)
{
    if (c == ember && version.load() > 0) return;
    ember = c;
    emberBar = c.withMultipliedBrightness (0.97f);
    emberLit = c.interpolatedWith (juce::Colours::white, 0.22f);
    emberPink = c.interpolatedWith (juce::Colours::white, 0.42f);
    emberDeep = c.withMultipliedBrightness (0.48f).withMultipliedSaturation (1.1f);
    emberBlood = c.withMultipliedBrightness (0.62f);
    litTop = c.withMultipliedBrightness (0.36f);
    litBottom = c.withMultipliedBrightness (0.2f);
    deepTint = juce::Colour::fromHSV (c.getHue(), juce::jmin (0.75f, c.getSaturation()), 0.102f, 1.0f);
    deepTint2 = juce::Colour::fromHSV (c.getHue(), juce::jmin (0.75f, c.getSaturation()), 0.165f, 1.0f);
    ++version;
}

/** Relit la teinte gardée (à l'ouverture d'une fenêtre, et une fois par seconde : poll). */
inline void load()
{
    const auto f = file();
    seenStamp = f.existsAsFile() ? f.getLastModificationTime().toMilliseconds() : 0;
    const auto hex = f.existsAsFile() ? f.loadFileAsString().trim() : juce::String();
    apply (hex.length() >= 6 ? juce::Colour ((juce::uint32) (0xff000000u | (hex.getHexValue32() & 0xffffffu))) : juce::Colour (swatches[0].argb));
}

/** Un autre plugin a changé la teinte : rend true si elle a changé (la fenêtre se redessine). */
inline bool poll()
{
    const auto f = file();
    const auto stamp = f.existsAsFile() ? f.getLastModificationTime().toMilliseconds() : 0;
    if (stamp == seenStamp) return false;
    const auto before = ember;
    load();
    return ember != before;
}

/** À appeler depuis le minuteur de chaque fenêtre (seen : un entier gardé par la fenêtre, -1 au départ) : relit le choix
    au plus une fois par seconde et rend true quand la fenêtre doit se redessiner dans la nouvelle teinte. */
inline juce::uint32 lastPoll = 0;
inline bool changedSince (int& seen)
{
    const auto now = juce::Time::getMillisecondCounter();
    if (seen < 0 || now - lastPoll >= 1000) { lastPoll = now; if (seen < 0) load(); else poll(); }
    const int v = version.load();
    if (v == seen) return false;
    seen = v;
    return true;
}

/** La teinte choisie dans le menu : appliquée ici et gardée pour les autres plugins. */
inline void choose (juce::uint32 argb)
{
    apply (juce::Colour (argb));
    const auto f = file();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::String::toHexString ((int) (argb & 0xffffffu)).paddedLeft ('0', 6));
    seenStamp = f.getLastModificationTime().toMilliseconds();
}
/** Une couleur en dur de la charte (rouge, bordeaux, rose) ramenée à la couleur de l'interface : même luminosité, saturation
    à l'échelle de la teinte choisie. Teinte d'origine : la couleur reste exactement la même. */
inline juce::Colour tint (juce::uint32 argb)
{
    const juce::Colour c (argb);
    const juce::Colour base (swatches[0].argb);
    if (ember == base) return c;
    const float s = juce::jlimit (0.0f, 1.0f, c.getSaturation() * ember.getSaturation() / juce::jmax (0.01f, base.getSaturation()));
    return juce::Colour::fromHSV (ember.getHue(), s, c.getBrightness(), c.getFloatAlpha());
}

} // namespace onirique::look

#endif
