#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
//  Le potard ONIRIQUE « GRADUATIONS » (nouvelle DA, celle de VOCALBOX), le même dans tous les plugins de la marque.
//
//  - 21 repères autour (un grand tous les 5) qui s'allument en braise jusqu'à la valeur (depuis le milieu pour un
//    réglage bipolaire) ; les autres restent en trait pâle. Pas de pavé ni de halo carré : seulement le trait.
//  - Un capuchon mat (dégradé sombre du haut vers le bas), liseré rosé, ombre douce dessous.
//  - Un trait blanc pour la position. Au survol, une lueur braise très douce autour du capuchon.
//
//  Ce fichier se copie tel quel dans chaque plugin ONIRIQUE (sauf RELIC et BLOOM), et ne se modifie qu'à un endroit.
//
//      onirique::ui::drawKnob (g, zone, position, angleDebut, angleFin, accent, bipolaire, survolé);
//==============================================================================
namespace onirique::ui
{

/** L'étoile à quatre branches d'ONIRIQUE, centrée en `c`, de rayon `r`. */
inline juce::Path knobStar (juce::Point<float> c, float r)
{
    juce::Path p;
    const float w = r * 0.085f;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + w, c.y - w, c.x + r, c.y);
    p.quadraticTo (c.x + w, c.y + w, c.x, c.y + r);
    p.quadraticTo (c.x - w, c.y + w, c.x - r, c.y);
    p.quadraticTo (c.x - w, c.y - w, c.x, c.y - r);
    p.closeSubPath();
    return p;
}

/** Dessine le potard dans `area` (un carré y est pris, centré). `position` : 0 à 1. Les angles sont ceux de JUCE
    (radians, 0 en haut, sens des aiguilles d'une montre). */
inline void drawKnob (juce::Graphics& g, juce::Rectangle<float> area, float position, float startAngle, float endAngle,
                      juce::Colour accent, bool bipolar, bool hover)
{
    using namespace juce;
    const float d = jmin (area.getWidth(), area.getHeight());
    if (d < 8.0f) return;
    const auto c = area.getCentre();
    const float pos = jlimit (0.0f, 1.0f, position);
    const float from = bipolar ? 0.5f : 0.0f;
    const float lo = jmin (from, pos), hi = jmax (from, pos);
    const bool small = d < 48.0f;

    // ---- les repères : 21 (11 sur un petit potard), un grand tous les 5
    const int count = small ? 10 : 20;
    for (int i = 0; i <= count; ++i)
    {
        const float t = (float) i / (float) count, a = startAngle + (endAngle - startAngle) * t;
        const bool big = small ? (i % 5 == 0) : (i % 5 == 0);
        const bool lit = t >= lo - 1.0e-4f && t <= hi + 1.0e-4f && (hi - lo > 0.002f || std::abs (t - from) < 1.0e-4f);
        const float r0 = d * (big ? 0.385f : 0.415f), r1 = d * 0.475f;
        Path p;
        p.startNewSubPath (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a));
        p.lineTo (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a));
        g.setColour (lit ? accent : Colour (255, 236, 238).withAlpha (0.18f));
        g.strokePath (p, PathStrokeType (jmax (1.0f, d * (big ? 0.024f : 0.016f)), PathStrokeType::curved, PathStrokeType::rounded));
    }

    // ---- le capuchon : lueur de survol, ombre douce, dégradé mat, liseré
    const float br = d * 0.32f;
    const auto cap = Rectangle<float> (2.0f * br, 2.0f * br).withCentre (c);
    if (hover)
        for (int q = 3; q >= 1; --q)
        {
            g.setColour (accent.withAlpha (0.07f / (float) q));
            g.fillEllipse (cap.expanded (d * 0.025f * (float) q));
        }
    for (int q = 3; q >= 1; --q)
    {
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillEllipse (cap.translated (0.0f, d * 0.025f).expanded (d * 0.01f * (float) q));
    }
    g.setGradientFill (ColourGradient (Colour (0xff1e1614), c.x, cap.getY(), Colour (0xff110b0c), c.x, cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (Colour (255, 220, 225).withAlpha (0.10f));
    g.drawEllipse (cap.reduced (0.5f), jmax (0.8f, d * 0.006f));
    g.setColour (Colours::white.withAlpha (0.05f));
    g.drawEllipse (cap.reduced (d * 0.02f), jmax (0.6f, d * 0.005f));

    // ---- le trait de position
    const float a = startAngle + (endAngle - startAngle) * pos;
    Path ptr;
    ptr.startNewSubPath (c.x + d * 0.12f * std::sin (a), c.y - d * 0.12f * std::cos (a));
    ptr.lineTo (c.x + d * 0.27f * std::sin (a), c.y - d * 0.27f * std::cos (a));
    g.setColour (Colour (0xffF4ECEC));
    g.strokePath (ptr, PathStrokeType (jmax (1.2f, d * 0.026f), PathStrokeType::curved, PathStrokeType::rounded));
}

} // namespace onirique::ui
