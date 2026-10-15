#pragma once

#include <juce_graphics/juce_graphics.h>

//==============================================================================
//  Le potard ONIRIQUE, le même dans tous les plugins de la marque.
//
//  - Une couronne de graduations, qui s'allument de l'origine jusqu'à la valeur (depuis le milieu pour un
//    réglage bipolaire) : la position se lit d'un coup d'œil, même de loin.
//  - Un corps sombre, biseauté, au bord finement strié, éclairé d'en haut à gauche.
//  - Une aiguille effilée, de la couleur du réglage vers l'os pâle, et l'étoile à quatre branches
//    d'ONIRIQUE à sa pointe ; la même étoile, gravée, au cœur du potard.
//
//  Ce fichier se copie tel quel dans Source/ui/ de chaque plugin ONIRIQUE, et ne se modifie qu'ici
//  (plugin-ui/ du dépôt ONIRIQUE CENTRAL). Les plugins des partenaires (ABRASION, RELIC) gardent leur propre style.
//
//      onirique::ui::drawKnob (g, zone, position, angleDebut, angleFin, accent, bipolaire, survolé);
//==============================================================================
namespace onirique::ui
{

/** L'étoile à quatre branches d'ONIRIQUE, centrée en `c`, de rayon `r`. */
inline juce::Path starPath (juce::Point<float> c, float r)
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

    const auto size = jmin (area.getWidth(), area.getHeight());
    if (size < 8.0f)
        return;

    const auto centre = area.getCentre();
    const float R = size * 0.5f - 1.0f;
    const float angle = startAngle + jlimit (0.0f, 1.0f, position) * (endAngle - startAngle);
    const auto at = [centre] (float a, float r) { return Point<float> (centre.x + r * std::sin (a), centre.y - r * std::cos (a)); };

    const auto bone = Colour (0xffffecee), hot = accent.brighter (0.35f);
    const float origin = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float lo = jmin (origin, angle), hi = jmax (origin, angle);
    const bool small = size < 62.0f;

    // Graduations : 40 intervalles (20 sur un petit potard), un trait majeur tous les cinq.
    const int count = small ? 20 : 40;
    const float tickLong = jmax (4.0f, size * 0.075f), tickShort = tickLong * 0.62f;
    for (int i = 0; i <= count; ++i)
    {
        const float a = startAngle + (float) i / (float) count * (endAngle - startAngle);
        const bool major = i % 5 == 0;
        const bool lit = hi - lo > 0.001f && a >= lo - 1.0e-4f && a <= hi + 1.0e-4f;
        Line<float> tick (at (a, R - (major ? tickLong : tickShort)), at (a, R));

        if (lit)
        {
            // Plus vif près de l'aiguille, plus doux vers l'origine.
            const float t = (a - lo) / jmax (1.0e-4f, hi - lo);
            const float near = bipolar && angle < origin ? 1.0f - t : t;
            g.setColour (accent.withAlpha (0.18f));
            g.drawLine (tick, major ? 4.6f : 3.6f);
            g.setColour ((near > 0.85f ? hot : accent).withAlpha (0.55f + 0.45f * near));
            g.drawLine (tick, major ? 2.0f : 1.5f);
        }
        else
        {
            g.setColour (Colour (0xffffd2d6).withAlpha (major ? 0.30f : 0.13f));
            g.drawLine (tick, major ? 1.5f : 1.0f);
        }
    }

    // Le corps : une ombre portée, un biseau éclairé d'en haut, puis la face.
    const float body = R - tickLong - jmax (3.0f, size * 0.035f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (2.0f * (body + 1.0f), 2.0f * (body + 1.0f)).withCentre (centre.translated (0.0f, 2.0f)));

    g.setGradientFill (ColourGradient (Colour (0xff5a3439), centre.x, centre.y - body, Colour (0xff070304), centre.x, centre.y + body, false));
    g.fillEllipse (Rectangle<float> (2.0f * body, 2.0f * body).withCentre (centre));

    const float face = body - jmax (2.0f, size * 0.027f);
    const auto faceBounds = Rectangle<float> (2.0f * face, 2.0f * face).withCentre (centre);
    ColourGradient shade (Colour (0xff3b2226), centre.x - face * 0.35f, centre.y - face * 0.45f,
                          Colour (0xff0b0506), centre.x + face * 0.65f, centre.y + face * 0.55f, true);
    shade.addColour (0.55, Colour (0xff170a0c));
    g.setGradientFill (shade);
    g.fillEllipse (faceBounds);

    // Le bord de la face, finement strié : plus clair en haut à gauche.
    if (! small)
    {
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.0f * MathConstants<float>::twoPi;
            const float light = jmax (0.0f, -std::cos (a + 0.6f));
            g.setColour (Colour (0xffffdce0).withAlpha (0.05f + 0.05f * light));
            g.drawLine (Line<float> (at (a, face - jmax (2.5f, face * 0.07f)), at (a, face - 0.8f)), 1.0f);
        }
    }
    g.setColour (Colour (0xffffd2d6).withAlpha (0.10f));
    g.drawEllipse (faceBounds, 1.0f);

    // Le reflet, un voile clair qui s'efface vers le centre.
    {
        Graphics::ScopedSaveState state (g);
        Path clip;
        clip.addEllipse (faceBounds.reduced (1.0f));
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (Colour (0xffffe1e4).withAlpha (hover ? 0.14f : 0.10f), centre.x - face, centre.y - face,
                                           Colour (0x00ffe1e4), centre.x + face * 0.2f, centre.y + face * 0.2f, false));
        g.fillRect (faceBounds);
    }

    // L'aiguille : une lame effilée, de la couleur du réglage vers l'os, avec sa lueur.
    const float half = jmax (1.4f, face * 0.05f);
    const auto tip = at (angle, face - jmax (3.0f, face * 0.09f));
    const auto base = at (angle, face * 0.30f);
    const auto side = at (angle - MathConstants<float>::halfPi, half) - centre;
    Path blade;
    blade.startNewSubPath (tip);
    blade.lineTo (base + side);
    blade.lineTo (base - side);
    blade.closeSubPath();

    DropShadow (accent.withAlpha (0.85f), jmax (4, roundToInt (face * (hover ? 0.22f : 0.16f))), {}).drawForPath (g, blade);
    g.setGradientFill (ColourGradient (accent.withAlpha (0.4f), base, bone, tip, false));
    g.fillPath (blade);

    // L'étoile à la pointe, puis le cœur gravé.
    const float starRadius = jmax (2.6f, face * 0.11f);
    const auto spark = starPath (tip, starRadius);
    DropShadow (hot, jmax (3, roundToInt (starRadius * 2.2f)), {}).drawForPath (g, spark);
    g.setColour (Colour (0xfffff4f5));
    g.fillPath (spark);

    const float core = face * 0.2f;
    g.setColour (Colour (0xff0a0405));
    g.fillEllipse (Rectangle<float> (2.0f * core, 2.0f * core).withCentre (centre));
    g.setColour (Colour (0xffffd2d6).withAlpha (0.12f));
    g.drawEllipse (Rectangle<float> (2.0f * core, 2.0f * core).withCentre (centre), 1.0f);
    g.setColour (accent.withAlpha (0.55f));
    g.fillPath (starPath (centre, core * 0.66f));
}

} // namespace onirique::ui
