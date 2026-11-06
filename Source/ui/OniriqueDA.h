#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../../License/OniriqueTheme.h"   // la couleur de l'interface

//==============================================================================
//  La DA ONIRIQUE (celle de VOCALBOX), en un seul fichier copié tel quel dans chaque plugin de la marque (sauf RELIC
//  et BLOOM), à côté d'OniriqueKnob.h :
//
//  - les couleurs (nuit, braise, os, brume) ;
//  - le fond : nuit #09080A et trois taches de braise floues (à calculer une fois par taille : backdrop()) ;
//  - le verre « liquide » des panneaux (glass), le bouton / onglet allumé (seg), la gélule (pill) ;
//  - les lueurs douces, floues (softGlow) : jamais de couches en escalier ni de pavé.
//
//  Toutes les mesures se donnent en unités u (u = largeur de la fenêtre / largeur de référence du plugin).
//==============================================================================
namespace onirique::da
{
inline const juce::Colour night { 0xff09080A }, deep { 0xff120c0e }, bone { 0xffF4ECEC }, haze { 0xffa8939a }, mist { 0xffd9c6c9 },
                          line { 0x1affdcE1 };
inline const juce::Colour& ember = onirique::look::ember;      // la couleur de l'interface (menu ⋯)
inline const juce::Colour& pink = onirique::look::emberPink;

/** Une lueur floue et douce autour d'une forme (box-shadow / drop-shadow). */
inline void softGlow (juce::Graphics& g, const juce::Path& p, juce::Colour c, float radius, float dy = 0.0f)
{
    if (c.getAlpha() == 0 || radius < 0.5f || p.isEmpty()) return;
    juce::DropShadow (c, std::max (1, juce::roundToInt (radius)), { 0, juce::roundToInt (dy) }).drawForPath (g, p);
}
inline void softGlowRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour c, float radius, float dy = 0.0f)
{
    juce::Path p; p.addRoundedRectangle (r, corner); softGlow (g, p, c, radius, dy);
}

/** Le fond d'une fenêtre : nuit et trois taches de braise floues. Coûteux : le garder dans une image par taille. */
inline juce::Image backdrop (int width, int height, float u)
{
    juce::Image img (juce::Image::RGB, std::max (1, width), std::max (1, height), true, juce::SoftwareImageType());
    juce::Graphics bg (img);
    bg.fillAll (night);
    const float W = (float) width / u, H = (float) height / u;
    auto blob = [&] (float x, float y, float w, float h, juce::Colour c)
    {
        const float cx = (x + w * 0.5f) * u, cy = (y + h * 0.5f) * u, rx = (w * 0.5f + 90.0f) * u, ry = (h * 0.5f + 90.0f) * u;
        juce::ColourGradient gr (c, cx, cy, c.withAlpha (0.0f), cx + rx, cy, true);
        gr.addColour (0.55, c.withMultipliedAlpha (0.55f));
        juce::Graphics::ScopedSaveState s (bg);
        bg.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, cx, cy));
        bg.setGradientFill (gr);
        bg.fillEllipse (cx - rx, cy - rx, 2.0f * rx, 2.0f * rx);
    };
    blob (-160.0f, -260.0f, 0.45f * W, 0.55f * H, onirique::look::emberBlood.withAlpha (0.55f));
    blob (0.6f * W, 0.62f * H, 0.4f * W, 0.55f * H, onirique::look::emberDeep.withAlpha (0.45f));
    blob (0.4f * W, 0.3f * H, 0.26f * W, 0.34f * H, onirique::look::emberLit.withAlpha (0.10f));
    return img;
}

/** Le verre des panneaux : dégradé blanc très léger à 160°, liseré rosé, reflet en haut, ombre douce dessous. */
inline void glass (juce::Graphics& g, juce::Rectangle<float> r, float corner, float u, bool shadow = true)
{
    if (shadow)
        for (int k = 4; k >= 1; --k)
        {
            const float e = 10.0f * u * (float) k;
            g.setColour (juce::Colours::black.withAlpha (0.10f / (float) k));
            g.fillRoundedRectangle (r.translated (0.0f, 18.0f * u * 0.6f).expanded (e * 0.5f), corner + e * 0.5f);
        }
    g.setColour (juce::Colour (0xff0f0b0d).withAlpha (0.55f));
    g.fillRoundedRectangle (r, corner);
    const auto a = r.getTopLeft(), b = r.getBottomRight();
    juce::ColourGradient gr (juce::Colours::white.withAlpha (0.075f), a.x + r.getWidth() * 0.32f, a.y,
                             juce::Colours::white.withAlpha (0.035f), b.x - r.getWidth() * 0.32f, b.y, false);
    gr.addColour (0.4, juce::Colours::white.withAlpha (0.02f));
    g.setGradientFill (gr);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colour (0xffffebee).withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip; clip.addRoundedRectangle (r, corner);
    g.reduceClipRegion (clip);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillRect (r.withHeight (std::max (1.0f, u)).translated (0.0f, 1.0f));
}

/** Un bouton / onglet : allumé (lit 0..1) en dégradé braise, liseré rose ; survolé (hover 0..1) en blanc très léger. */
inline void seg (juce::Graphics& g, juce::Rectangle<float> r, float lit, float hover, float u, float corner = -1.0f)
{
    const float rad = corner >= 0.0f ? corner : 10.0f * u;
    if (hover > 0.01f && lit < 0.99f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f * hover * (1.0f - lit)));
        g.fillRoundedRectangle (r, rad);
    }
    if (lit <= 0.01f) return;
    g.setGradientFill (juce::ColourGradient (onirique::look::ember.withAlpha (0.6f * lit), 0.0f, r.getY(),
                                             onirique::look::emberDeep.withAlpha (0.6f * lit), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (onirique::look::emberLit.withAlpha (0.5f * lit));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
}

/** Une gélule (bouton principal) : la forme de seg aux bords tout ronds. */
inline void pill (juce::Graphics& g, juce::Rectangle<float> r, float lit, float hover, float u)
{
    glass (g, r, r.getHeight() * 0.5f, u, false);
    seg (g, r.reduced (3.0f * u), lit, hover, u, (r.getHeight() - 6.0f * u) * 0.5f);
}

/** Un interrupteur (gélule et pastille qui glisse) : on 0..1. */
inline void toggle (juce::Graphics& g, juce::Rectangle<float> r, float on, float u)
{
    const float rad = r.getHeight() * 0.5f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, rad);
    if (on > 0.01f)
    {
        g.setGradientFill (juce::ColourGradient (onirique::look::ember.withAlpha (0.75f * on), 0.0f, r.getY(),
                                                 onirique::look::emberDeep.withAlpha (0.75f * on), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    const float d = r.getHeight() - 8.0f * u;
    const auto dot = juce::Rectangle<float> (d, d).withPosition (r.getX() + 4.0f * u + (r.getWidth() - d - 8.0f * u) * on, r.getY() + 4.0f * u);
    g.setColour (mist.interpolatedWith (juce::Colours::white, on));
    g.fillEllipse (dot);
}

/** Un témoin (LED) braise allumé, gris éteint. */
inline void led (juce::Graphics& g, juce::Point<float> c, float size, float on)
{
    const auto r = juce::Rectangle<float> (size, size).withCentre (c);
    if (on > 0.01f) { juce::Path p; p.addEllipse (r); softGlow (g, p, ember.withAlpha (0.8f * on), size); }
    g.setColour (ember.interpolatedWith (juce::Colour (0xff4a3a3e), 1.0f - on));
    g.fillEllipse (r);
}
} // namespace onirique::da
