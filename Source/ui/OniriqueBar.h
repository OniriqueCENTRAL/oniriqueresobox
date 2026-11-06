#pragma once
#include "../../License/OniriqueTheme.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include "../../License/LicenseTheme.h"
#include "OniriquePresets.h"

//==============================================================================
//  La barre « Nuit » des plugins ONIRIQUE, au-dessus de leur façade : la marque, les presets (flèches, nom,
//  navigateur), IN, la comparaison A/B, copier/coller les réglages, la place de la pastille de licence et le menu ⋯
//  (taille de la fenêtre, import des presets). Le dessin suit la planche « C · Nuit » : des cadres noirs aux coins
//  de 9, des segments qui s'allument d'un dégradé rouge sombre, d'un filet braise et de l'étoile ONIRIQUE.
//
//  Dessinée en points, comme la façade, et zoomée avec elle. Étroite (moins de 700 points), elle passe sur deux
//  rangées et range copier/coller dans le menu.
//
//  Ce fichier se copie tel quel dans Source/ui/ de chaque plugin qui a la barre Nuit, avec OniriquePresets.h.
//
//      nuit::Bar bar { processor.presets, processor.apvts, "WEIGHT", sigil, kWidth, kWidth };
//      constructeur : addAndMakeVisible (bar), avant la pastille de licence ;
//      resized()    : bar.setPillWidth (licenseOverlay->pillWidth()); bar.place (scale, getLocalBounds());
//                     licenseOverlay->fit (getLocalBounds(), scale, bar.getPillArea(), Justification::centredRight);
//      bar.setBrand (largeur, dessin) : le titre du plugin (son logo, son nom) à la place de « ONIRIQUE » + nom.
//      bar.setWithoutBrand() : sans marque (le nom est déjà sur la façade) ; étroite, la barre tient sur une rangée.
//      bar.setWithoutZoom() : dans une application (pas un plugin), le menu ⋯ ne propose pas la taille de la fenêtre.
//==============================================================================
namespace onirique::nuit
{

using namespace juce;

namespace colours
{
    inline const Colour bar { 0xff120a0c }, card { 0xff1a0c0f }, mist { 0xfff6eeee }, chip { 0xffd9c3c6 }, haze { 0xffa88f92 },
                        rose { 0xffffd2d6 }, bone { 0xffffecee }, brandName { 0xffc9afb3 };
    // l'accent : la couleur de l'interface (OniriqueTheme.h)
    inline const Colour& ember = onirique::look::emberBar;
    inline const Colour& emberLit = onirique::look::emberLit;
    inline const Colour& starLit = onirique::look::emberPink;
    inline const Colour& litTop = onirique::look::litTop;
    inline const Colour& litBottom = onirique::look::litBottom;
}

using Fonts = license::theme::Fonts;

/** L'étoile à quatre branches d'ONIRIQUE, centrée en `c`, de rayon `r`. */
inline Path star (Point<float> c, float r)
{
    Path p;
    const float w = r * 0.085f;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + w, c.y - w, c.x + r, c.y);
    p.quadraticTo (c.x + w, c.y + w, c.x, c.y + r);
    p.quadraticTo (c.x - w, c.y + w, c.x - r, c.y);
    p.quadraticTo (c.x - w, c.y - w, c.x, c.y - r);
    p.closeSubPath();
    return p;
}

/** Le cadre où des segments sont rangés : noir à 45 %, filet rose ; braise quand ce qu'il ouvre est ouvert. */
inline void drawWell (Graphics& g, Rectangle<float> r, float radius = 9.0f, bool open = false)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (open ? colours::ember.withAlpha (0.6f) : colours::rose.withAlpha (0.14f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

/** Ce qui est allumé : un dégradé rouge sombre et un filet braise vive. */
inline void drawLit (Graphics& g, Rectangle<float> r, float radius)
{
    g.setGradientFill (ColourGradient (colours::litTop, 0.0f, r.getY(), colours::litBottom, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (colours::emberLit.withAlpha (0.75f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

/** Le voyant : l'étoile, allumée avec sa lueur, ou éteinte. */
inline void drawLed (Graphics& g, Point<float> c, float r, bool on)
{
    if (on)
    {
        g.setColour (colours::ember.withAlpha (0.35f));
        g.fillPath (star (c, r * 1.7f));
        g.setColour (colours::starLit);
    }
    else
    {
        g.setColour (colours::rose.withAlpha (0.28f));
    }

    g.fillPath (star (c, r));
}

/** Une petite étiquette en capitales espacées, et le filet qui la prolonge. */
inline void drawHeading (Graphics& g, const license::theme::FontSet& fonts, const String& text, Rectangle<float> area, bool rule = true)
{
    const auto font = fonts.monoFont (9.5f, 0.16f);
    g.setColour (colours::haze);
    g.setFont (font);
    g.drawText (text, area, Justification::centredLeft, false);

    if (rule)
    {
        const auto x = area.getX() + GlyphArrangement::getStringWidth (font, text) + 10.0f;
        g.setColour (colours::rose.withAlpha (0.1f));
        g.fillRect (Rectangle<float> (x, area.getCentreY(), jmax (0.0f, area.getRight() - x), 1.0f));
    }
}

enum class Icon { previous, next, chevronDown, chevronUp, dots, close, star, starOutline, copy, paste, import, folder };

inline void drawIcon (Graphics& g, Icon icon, Rectangle<float> area, Colour colour)
{
    const auto c = area.getCentre();
    const PathStrokeType stroke (1.5f, PathStrokeType::curved, PathStrokeType::rounded);
    Path p;
    g.setColour (colour);

    switch (icon)
    {
        case Icon::previous:    p.startNewSubPath (c.x + 1.8f, c.y - 3.6f); p.lineTo (c.x - 1.8f, c.y); p.lineTo (c.x + 1.8f, c.y + 3.6f); break;
        case Icon::next:        p.startNewSubPath (c.x - 1.8f, c.y - 3.6f); p.lineTo (c.x + 1.8f, c.y); p.lineTo (c.x - 1.8f, c.y + 3.6f); break;
        case Icon::chevronDown: p.startNewSubPath (c.x - 4.0f, c.y - 1.8f); p.lineTo (c.x, c.y + 1.8f); p.lineTo (c.x + 4.0f, c.y - 1.8f); break;
        case Icon::chevronUp:   p.startNewSubPath (c.x - 4.0f, c.y + 1.8f); p.lineTo (c.x, c.y - 1.8f); p.lineTo (c.x + 4.0f, c.y + 1.8f); break;
        case Icon::close:       p.startNewSubPath (c.x - 3.5f, c.y - 3.5f); p.lineTo (c.x + 3.5f, c.y + 3.5f);
                                p.startNewSubPath (c.x + 3.5f, c.y - 3.5f); p.lineTo (c.x - 3.5f, c.y + 3.5f); break;
        case Icon::starOutline: g.strokePath (star (c, 6.0f), PathStrokeType (1.1f)); return;
        case Icon::star:        g.fillPath (star (c, 6.5f)); return;

        case Icon::dots:
            for (int i = -1; i <= 1; ++i)
                g.fillEllipse (c.x + (float) i * 6.0f - 1.5f, c.y - 1.5f, 3.0f, 3.0f);
            return;

        case Icon::copy:
            p.addRoundedRectangle (c.x - 1.5f, c.y - 1.5f, 7.0f, 7.0f, 1.8f);
            p.startNewSubPath (c.x - 5.0f, c.y + 2.0f); p.lineTo (c.x - 5.0f, c.y - 3.5f);
            p.quadraticTo (c.x - 5.0f, c.y - 5.0f, c.x - 3.5f, c.y - 5.0f); p.lineTo (c.x + 2.0f, c.y - 5.0f);
            break;

        case Icon::paste:
            p.addRoundedRectangle (c.x - 4.5f, c.y - 4.0f, 9.0f, 10.0f, 1.8f);
            p.addRoundedRectangle (c.x - 2.0f, c.y - 5.5f, 4.0f, 3.0f, 1.0f);
            break;

        case Icon::import:
            p.startNewSubPath (c.x, c.y - 5.5f); p.lineTo (c.x, c.y + 2.0f);
            p.startNewSubPath (c.x - 3.0f, c.y - 1.0f); p.lineTo (c.x, c.y + 2.0f); p.lineTo (c.x + 3.0f, c.y - 1.0f);
            p.startNewSubPath (c.x - 5.5f, c.y + 3.5f); p.lineTo (c.x - 5.5f, c.y + 5.5f); p.lineTo (c.x + 5.5f, c.y + 5.5f); p.lineTo (c.x + 5.5f, c.y + 3.5f);
            break;

        case Icon::folder:
            p.startNewSubPath (c.x - 6.0f, c.y + 4.5f); p.lineTo (c.x - 6.0f, c.y - 4.5f); p.lineTo (c.x - 2.0f, c.y - 4.5f);
            p.lineTo (c.x - 0.5f, c.y - 3.0f); p.lineTo (c.x + 6.0f, c.y - 3.0f); p.lineTo (c.x + 6.0f, c.y + 4.5f); p.closeSubPath();
            break;
    }

    g.strokePath (p, stroke);
}

//==============================================================================
/** Les barres de défilement et les champs de texte de la barre et de ses panneaux. */
class Look final : public LookAndFeel_V4
{
public:
    Look()
    {
        setColour (TextEditor::backgroundColourId, Colours::transparentBlack);
        setColour (TextEditor::outlineColourId, Colours::transparentBlack);
        setColour (TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        setColour (TextEditor::textColourId, colours::mist);
        setColour (TextEditor::highlightColourId, colours::ember.withAlpha (0.45f));
        setColour (TextEditor::highlightedTextColourId, Colours::white);
        setColour (CaretComponent::caretColourId, colours::emberLit);
    }

    void fillTextEditorBackground (Graphics&, int, int, TextEditor&) override {}
    void drawTextEditorOutline (Graphics&, int, int, TextEditor&) override {}

    int getDefaultScrollbarWidth() override   { return 10; }

    void drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int width, int height, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override
    {
        const auto area = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto track = vertical ? area.withSizeKeepingCentre (4.0f, area.getHeight() - 4.0f)
                                    : area.withSizeKeepingCentre (area.getWidth() - 4.0f, 4.0f);

        g.setColour (colours::rose.withAlpha (0.12f));
        g.fillRoundedRectangle (track, 2.0f);

        if (thumbSize <= 0)
            return;

        // Plus large et plus vive sous le pointeur : on voit qu'elle se prend.
        const auto lit = over || down;
        const auto t = lit ? 6.0f : 4.0f;
        const auto thumb = vertical ? Rectangle<float> (area.getCentreX() - t * 0.5f, (float) thumbStart + 1.0f, t, (float) thumbSize - 2.0f)
                                    : Rectangle<float> ((float) thumbStart + 1.0f, area.getCentreY() - t * 0.5f, (float) thumbSize - 2.0f, t);
        g.setColour (lit ? colours::emberLit : colours::ember.withAlpha (0.85f));
        g.fillRoundedRectangle (thumb, t * 0.5f);
    }
};

//==============================================================================
/** Un segment, rangé dans un cadre avec ses voisins : des capitales espacées, et l'étoile qui s'allume. */
class Segment : public Button
{
public:
    explicit Segment (const String& text = {}, bool withLed = false, bool monoText = false)
        : Button (text), led (withLed), mono (monoText) {}

    /** Une action qui attend d'être confirmée (DELETE devenu SURE ?). */
    void setArmed (bool shouldBeArmed)   { if (armed != shouldBeArmed) { armed = shouldBeArmed; repaint(); } }
    bool isArmed() const                 { return armed; }
    void setMonoText (bool shouldBeMono) { mono = shouldBeMono; }

    int preferredWidth() const
    {
        return roundToInt (GlyphArrangement::getStringWidth (font(), getButtonText())) + 22 + (led ? 15 : 0);
    }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto on = getToggleState();

        if (armed)
        {
            g.setColour (colours::ember.withAlpha (0.22f));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (colours::ember.withAlpha (0.75f));
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        }
        else if (on)
        {
            drawLit (g, r, 7.0f);
        }
        else if ((over || down) && isEnabled())
        {
            g.setColour (colours::rose.withAlpha (down ? 0.12f : 0.07f));
            g.fillRoundedRectangle (r, 7.0f);
        }

        auto label = r;

        if (led)
        {
            drawLed (g, { r.getX() + 13.0f, r.getCentreY() }, 4.5f, on);
            label.removeFromLeft (17.0f);
        }

        const auto colour = on || armed ? Colours::white : over && isEnabled() ? colours::mist : colours::haze;
        g.setColour (colour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
        g.setFont (font());
        g.drawText (getButtonText(), label, Justification::centred, false);
    }

private:
    Font font() const   { return mono ? fonts->monoBoldFont (10.0f) : fonts->bodyFont (10.0f, 0.14f); }

    Fonts fonts;
    bool led, mono, armed = false;
};

/** Un bouton fait d'une icône : seul, il a son cadre (⋯, fermer) ; dans un cadre, juste l'icône (‹ ›). */
class IconButton : public Button
{
public:
    IconButton (Icon i, bool hasFrame) : Button ({}), icon (i), framed (hasFrame) {}

    void setIcon (Icon i)          { if (icon != i) { icon = i; repaint(); } }
    void setOpen (bool isOpen)     { if (open != isOpen) { open = isOpen; repaint(); } }
    void setLit (bool isLit)       { if (lit != isLit) { lit = isLit; repaint(); } }

    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();

        if (framed)
            drawWell (g, r, jmin (9.0f, r.getHeight() * 0.32f), open);

        if (lit)
        {
            g.setColour (colours::ember.withAlpha (0.12f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (colours::emberLit.withAlpha (0.4f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        else if (over || down)
        {
            g.setColour (colours::rose.withAlpha (down ? 0.12f : 0.07f));
            g.fillRoundedRectangle (r.reduced (framed ? 2.0f : 0.0f), 7.0f);
        }

        if (lit)
        {
            g.setColour (colours::ember.withAlpha (0.35f));
            g.fillPath (star (r.getCentre(), 11.0f));
            drawIcon (g, icon, r, colours::starLit);
            return;
        }

        drawIcon (g, icon, r, (over ? colours::mist : (framed ? colours::mist : colours::haze)).withMultipliedAlpha (isEnabled() ? 1.0f : 0.4f));
    }

private:
    Icon icon;
    bool framed, open = false, lit = false;
};

/** Une puce de tag : on en allume autant qu'on veut. */
class Chip : public Button
{
public:
    explicit Chip (const String& text, bool starIcon = false) : Button (text), withStar (starIcon)
    {
        setClickingTogglesState (true);
    }

    int preferredWidth() const
    {
        return roundToInt (GlyphArrangement::getStringWidth (fonts->bodyFont (11.5f), getButtonText())) + 22 + (withStar ? 16 : 0);
    }

    void paintButton (Graphics& g, bool over, bool) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto on = getToggleState();

        if (on)
        {
            drawLit (g, r, 8.0f);
        }
        else
        {
            g.setColour (over ? colours::rose.withAlpha (0.08f) : Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (colours::rose.withAlpha (over ? 0.26f : 0.14f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }

        auto label = r.reduced (10.0f, 0.0f);

        if (withStar)
        {
            const auto c = Point<float> (label.getX() + 5.0f, r.getCentreY());

            if (on)
                drawLed (g, c, 5.0f, true);
            else
            {
                g.setColour (colours::chip);
                g.strokePath (star (c, 5.0f), PathStrokeType (1.1f));
            }

            label.removeFromLeft (16.0f);
        }

        g.setColour (on || over ? Colours::white : colours::chip);
        g.setFont (fonts->bodyFont (11.5f));
        g.drawText (getButtonText(), label, Justification::centredLeft, false);
    }

private:
    Fonts fonts;
    bool withStar;
};

/** Une ligne du menu : son icône, son texte, teintée de braise sous le pointeur. */
class MenuItem : public Button
{
public:
    MenuItem (const String& text, Icon i) : Button (text), icon (i) {}

    void paintButton (Graphics& g, bool over, bool) override
    {
        const auto r = getLocalBounds().toFloat();
        const auto hot = over && isEnabled();

        if (hot)
        {
            g.setColour (colours::ember.withAlpha (0.14f));
            g.fillRoundedRectangle (r, 8.0f);
        }

        const auto alpha = isEnabled() ? 1.0f : 0.35f;
        drawIcon (g, icon, r.withWidth (34.0f), (hot ? colours::emberLit : colours::haze).withMultipliedAlpha (alpha));
        g.setColour ((hot ? Colours::white : colours::mist.withAlpha (0.88f)).withMultipliedAlpha (alpha));
        g.setFont (fonts->bodyFont (13.0f));
        g.drawText (getButtonText(), r.withTrimmedLeft (36.0f).withTrimmedRight (10.0f), Justification::centredLeft, true);
    }

private:
    Fonts fonts;
    Icon icon;
};

/** Le nom du preset chargé, d'où il vient, et la flèche qui ouvre le navigateur. */
class PresetField : public Button
{
public:
    PresetField() : Button ("preset") {}

    void show (const String& presetName, const String& origin, bool isOpen)
    {
        if (presetName == name && origin == source && isOpen == open)
            return;

        name = presetName;
        source = origin;
        open = isOpen;
        repaint();
    }

    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat();
        drawWell (g, r, 9.0f, open || over);

        r = r.reduced (13.0f, 0.0f).withTrimmedRight (-3.0f);
        drawIcon (g, open ? Icon::chevronUp : Icon::chevronDown, r.removeFromRight (12.0f), open ? colours::emberLit : colours::ember);

        if (source.isNotEmpty())
        {
            const auto font = fonts->monoFont (9.0f, 0.14f);
            const auto width = GlyphArrangement::getStringWidth (font, source);
            g.setColour (colours::haze);
            g.setFont (font);
            g.drawText (source, r.removeFromRight (width + 10.0f), Justification::centredLeft, false);
        }

        g.setColour (colours::mist);
        g.setFont (fonts->bodyFont (12.5f));
        g.drawText (name, r, Justification::centredLeft, true);
    }

private:
    Fonts fonts;
    String name, source;
    bool open = false;
};

//==============================================================================
/** Le navigateur de presets, une carte sous la barre. Un clic charge un preset, qu'on entend aussitôt ; un
    double-clic, Entrée, la croix ou un clic à côté le gardent ; Échap remet ce qu'il y avait à l'ouverture.
    Large, il se range en deux colonnes (les filtres, puis la liste) ; étroit, l'un sous l'autre. */
class Browser final : public Component
{
public:
    explicit Browser (PresetBank& b) : bank (b)
    {
        setWantsKeyboardFocus (true);

        closeButton.onClick = [this] { close (true); };
        closeButton.setTooltip ("Fermer : le preset choisi reste.");
        addAndMakeVisible (closeButton);

        search.setTextToShowWhenEmpty (String::fromUTF8 ("Find preset…"), colours::haze.withAlpha (0.8f));
        search.setFont (fonts->bodyFont (13.0f));
        search.setIndents (0, 8);
        search.onTextChange = [this] { rebuild(); };
        search.onEscapeKey = [this] { close (false); };
        search.onReturnKey = [this] { close (true); };
        addAndMakeVisible (search);

        const char* sourceNames[] = { "ALL", "FACTORY", "USER" };

        for (int i = 0; i < 3; ++i)
        {
            sources[i].setButtonText (sourceNames[i]);
            sources[i].onClick = [this, i] { bank.source = (PresetBank::Source) i; rebuild(); };
            addAndMakeVisible (sources[i]);
        }

        chips.add (new Chip ("Favorites", true));
        chips.getLast()->onClick = [this] { bank.favouritesOnly = chips[0]->getToggleState(); rebuild(); };

        for (int i = 0; i < bank.getTagNames().size(); ++i)
        {
            chips.add (new Chip (bank.getTagNames()[i]));
            chips.getLast()->onClick = [this, i]
            {
                bank.tagFilter ^= 1 << i;
                rebuild();
            };
        }

        for (auto* chip : chips)
            addAndMakeVisible (chip);

        favouriteButton.onClick = [this]
        {
            const auto name = bank.getCurrentName();
            bank.setFavourite (name, ! bank.isFavourite (name));
            rebuild();
        };
        addAndMakeVisible (favouriteButton);

        deleteButton.setTooltip (String::fromUTF8 ("Supprime ce preset, s'il est à toi. Un second clic confirme."));
        deleteButton.onClick = [this]
        {
            if (! deleteButton.isArmed())
            {
                deleteButton.setArmed (true);
                deleteButton.setButtonText ("SURE ?");
                SafePointer<Browser> safe (this);
                Timer::callAfterDelay (3000, [safe] { if (safe != nullptr) safe->disarm(); });
                return;
            }

            bank.remove (bank.getCurrentName());
            disarm();
            rebuild();
        };

        renameButton.setTooltip (String::fromUTF8 ("Renomme ce preset, s'il est à toi."));
        renameButton.onClick = [this] { beginNaming (true); };
        saveButton.setTooltip (String::fromUTF8 ("Garde les réglages comme un preset à toi : tape son nom, puis Entrée."));
        saveButton.onClick = [this] { beginNaming (false); };

        for (auto* button : { &deleteButton, &renameButton, &saveButton })
            addAndMakeVisible (button);

        nameEditor.setFont (fonts->displayFont (13.0f));
        nameEditor.setTextToShowWhenEmpty (String::fromUTF8 ("Preset name…"), colours::haze.withAlpha (0.8f));
        nameEditor.setIndents (0, 4);
        nameEditor.onReturnKey = [this] { finishNaming (true); };
        nameEditor.onEscapeKey = [this] { finishNaming (false); };
        nameEditor.onFocusLost = [this] { finishNaming (false); };
        addChildComponent (nameEditor);

        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarThickness (10);
        addAndMakeVisible (viewport);
    }

    std::function<void()> onClose;

    void open()
    {
        bank.reloadFavourites();
        before = bank.capture();
        beforeName = bank.getCurrentName();
        setVisible (true);
        rebuild();
        grabKeyboardFocus();
    }

    void close (bool keep)
    {
        if (! isVisible())
            return;

        if (! keep)
            bank.restore (before, beforeName);

        finishNaming (false);
        setVisible (false);

        if (onClose)
            onClose();
    }

    /** Ce que la liste montre, refait quand un filtre change ou qu'un preset est chargé. */
    void rebuild()
    {
        for (int i = 0; i < 3; ++i)
            sources[i].setToggleState (bank.source == i, dontSendNotification);

        chips[0]->setToggleState (bank.favouritesOnly, dontSendNotification);

        for (int i = 1; i < chips.size(); ++i)
            chips[i]->setToggleState (((bank.tagFilter >> (i - 1)) & 1) != 0, dontSendNotification);

        const auto current = bank.getCurrentName();
        const auto user = bank.isUser (current);
        deleteButton.setEnabled (user);
        renameButton.setEnabled (user);
        favouriteButton.setLit (bank.isFavourite (current));
        favouriteButton.setIcon (bank.isFavourite (current) ? Icon::star : Icon::starOutline);
        favouriteButton.setTooltip (bank.isFavourite (current) ? "Retirer des favoris" : "Ajouter aux favoris");

        // La liste : ceux d'usine, puis ceux de l'utilisateur, chacun sous son titre quand les deux sont montrés.
        auto matches = [this] (const String& name) { return search.isEmpty() || name.containsIgnoreCase (search.getText().trim()); };
        StringArray stock, mine;

        for (const auto& name : bank.getFiltered())
            if (matches (name))
                (bank.isStock (name) ? stock : mine).add (name);

        list.entries.clear();
        count = stock.size() + mine.size();

        if (bank.source != PresetBank::userOnly)
        {
            if (bank.source == PresetBank::anySource)
                list.entries.push_back ({ "FACTORY", List::heading });

            for (const auto& name : stock)
                list.entries.push_back ({ name, List::preset });
        }

        if (bank.source != PresetBank::stockOnly)
        {
            if (bank.source == PresetBank::anySource)
                list.entries.push_back ({ "USER", List::heading });

            for (const auto& name : mine)
                list.entries.push_back ({ name, List::preset });

            if (mine.isEmpty() && bank.getUserNames().isEmpty())
                list.entries.push_back ({ String::fromUTF8 ("Rien ici pour l'instant : SAVE range tes réglages."), List::hint });
        }

        layoutList();
        repaint();
    }

    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (colours::card);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (colours::rose.withAlpha (0.14f));
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        // Le titre, et combien de presets la liste montre.
        const auto t = titleArea.toFloat();
        g.setColour (colours::ember.withAlpha (0.4f));
        g.fillPath (star ({ t.getX() + 6.0f, t.getCentreY() }, 9.0f));
        g.setColour (colours::mist);
        g.fillPath (star ({ t.getX() + 6.0f, t.getCentreY() }, 6.0f));
        const auto titleFont = fonts->displayFont (12.0f, 0.06f);
        const auto titleWidth = GlyphArrangement::getStringWidth (titleFont, "PRESETS");
        g.setColour (Colours::white);
        g.setFont (titleFont);
        g.drawText ("PRESETS", t.withTrimmedLeft (21.0f), Justification::centredLeft, false);

        const auto countText = String (count);
        const auto countFont = fonts->monoFont (9.5f);
        const auto countBox = Rectangle<float> (GlyphArrangement::getStringWidth (countFont, countText) + 14.0f, 17.0f)
                                  .withPosition (t.getX() + 21.0f + titleWidth + 9.0f, t.getCentreY() - 8.5f);
        g.setColour (colours::rose.withAlpha (0.07f));
        g.fillRoundedRectangle (countBox, 6.0f);
        g.setColour (colours::haze);
        g.setFont (countFont);
        g.drawText (countText, countBox, Justification::centred, false);

        drawWell (g, searchArea.toFloat(), 10.0f);
        g.setColour (colours::haze);
        const auto lens = searchArea.toFloat().removeFromLeft (34.0f).getCentre();
        g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (lens.translated (-1.0f, -1.0f)), 1.5f);
        g.drawLine (lens.x + 2.4f, lens.y + 2.4f, lens.x + 5.0f, lens.y + 5.0f, 1.6f);

        drawWell (g, sourceWell.toFloat());
        drawHeading (g, *fonts, "TAGS", tagsHeading.toFloat(), false);

        // Le preset chargé : d'où il vient, son nom, et ce qu'on peut en faire.
        const auto c = currentArea.toFloat();
        g.setColour (colours::rose.withAlpha (0.04f));
        g.fillRoundedRectangle (c, 10.0f);
        g.setColour (colours::rose.withAlpha (0.1f));
        g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.0f);

        const auto current = bank.getCurrentName();
        const auto origin = bank.isStock (current) ? "FACTORY" : bank.isUser (current) ? "USER" : "";
        g.setColour (colours::haze);
        g.setFont (fonts->monoFont (9.0f, 0.16f));
        g.drawText (origin, originArea.toFloat(), Justification::centredLeft, false);

        if (! nameEditor.isVisible())
        {
            g.setColour (Colours::white);
            g.setFont (fonts->displayFont (13.0f));
            g.drawText (current, nameArea.toFloat(), Justification::centredLeft, true);
        }

        drawWell (g, actionWell.toFloat());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        const auto wide = getWidth() >= 700;
        auto left = wide ? r.removeFromLeft (310) : r;

        if (wide)
            r.removeFromLeft (16);

        // Le titre, et la croix dans le coin.
        auto top = left.removeFromTop (26);
        closeButton.setBounds (getLocalBounds().reduced (12).removeFromTop (26).removeFromRight (26));
        titleArea = top.withTrimmedRight (34).withTrimmedLeft (4);
        left.removeFromTop (12);

        searchArea = left.removeFromTop (34);
        search.setBounds (searchArea.withTrimmedLeft (34).withTrimmedRight (10));
        left.removeFromTop (12);

        sourceWell = left.removeFromTop (30);
        auto inner = sourceWell.reduced (2);
        const auto third = (inner.getWidth() - 4) / 3;

        for (int i = 0; i < 3; ++i)
        {
            sources[i].setBounds (inner.removeFromLeft (i < 2 ? third : inner.getWidth()));
            inner.removeFromLeft (2);
        }

        left.removeFromTop (12);
        tagsHeading = left.removeFromTop (12).withTrimmedLeft (4);
        left.removeFromTop (8);

        // Les puces, à la ligne quand la place manque.
        int x = left.getX(), y = left.getY();

        for (auto* chip : chips)
        {
            const auto w = chip->preferredWidth();

            if (x + w > left.getRight() && x > left.getX())
            {
                x = left.getX();
                y += 32;
            }

            chip->setBounds (x, y, w, 26);
            x += w + 6;
        }

        left.removeFromTop (y + 26 - left.getY());
        left.removeFromTop (12);

        // Le preset chargé : en tête de la colonne de droite, au large ; sous les filtres, à l'étroit.
        auto& column = wide ? r : left;
        currentArea = column.removeFromTop (wide ? 50 : 92);

        if (wide)
            currentArea.removeFromRight (36);   // la croix, dans le coin
        auto card = currentArea.reduced (10);

        if (wide)
        {
            actionWell = card.removeFromRight (250).withSizeKeepingCentre (250, 30);
            card.removeFromRight (10);
            favouriteButton.setBounds (card.removeFromRight (30).withSizeKeepingCentre (30, 30));
            card.removeFromRight (10);
        }
        else
        {
            actionWell = card.removeFromBottom (30);
            card.removeFromBottom (10);
            favouriteButton.setBounds (card.removeFromRight (30).withSizeKeepingCentre (30, 30));
            card.removeFromRight (10);
        }

        originArea = card.removeFromTop (card.getHeight() / 2).withTrimmedLeft (2);
        nameArea = card.withTrimmedLeft (2);
        nameEditor.setBounds (nameArea.withTrimmedTop (-4));

        inner = actionWell.reduced (2);
        const auto w3 = (inner.getWidth() - 4) / 3;

        for (auto* button : { &deleteButton, &renameButton, &saveButton })
        {
            button->setBounds (inner.removeFromLeft (button == &saveButton ? inner.getWidth() : w3));
            inner.removeFromLeft (2);
        }

        column.removeFromTop (12);
        viewport.setBounds (column);
        layoutList();
    }

    bool keyPressed (const KeyPress& key) override
    {
        if (key == KeyPress::escapeKey)   { close (false); return true; }
        if (key == KeyPress::returnKey)   { close (true); return true; }
        return false;
    }

    // Rien de ce qui est dessous ne doit se prendre à travers la carte.
    void mouseDown (const MouseEvent&) override {}

private:
    /** La liste elle-même : en colonnes au large, d'une seule colonne à l'étroit. */
    struct List final : Component
    {
        enum Kind { preset, heading, hint };
        struct Entry { String text; Kind kind; };

        explicit List (Browser& b) : browser (b) {}

        Rectangle<int> cell (int i) const
        {
            return columns ? Rectangle<int> ((i / rows) * columnWidth, (i % rows) * kStride, columnWidth - 10, kRow)
                           : Rectangle<int> (0, i * kStride, getWidth(), kRow);
        }

        int indexAt (Point<int> p) const
        {
            for (int i = 0; i < (int) entries.size(); ++i)
                if (entries[(size_t) i].kind == preset && cell (i).contains (p))
                    return i;

            return -1;
        }

        void paint (Graphics& g) override
        {
            const auto current = browser.bank.getCurrentName();

            for (int i = 0; i < (int) entries.size(); ++i)
            {
                const auto& e = entries[(size_t) i];
                const auto r = cell (i).toFloat();

                if (e.kind == heading)
                {
                    drawHeading (g, *fonts, e.text, r.reduced (8.0f, 0.0f));
                    continue;
                }

                if (e.kind == hint)
                {
                    g.setColour (colours::haze);
                    g.setFont (fonts->bodyFont (12.0f));
                    g.drawText (e.text, r.withTrimmedLeft (28.0f), Justification::centredLeft, true);
                    continue;
                }

                const auto chosen = e.text == current;

                if (chosen)
                {
                    g.setColour (colours::ember.withAlpha (0.16f));
                    g.fillRoundedRectangle (r, 8.0f);
                    g.setColour (colours::emberLit.withAlpha (0.55f));
                    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
                    drawLed (g, { r.getX() + 14.0f, r.getCentreY() }, 5.0f, true);
                }
                else if (i == hovered)
                {
                    g.setColour (colours::rose.withAlpha (0.06f));
                    g.fillRoundedRectangle (r, 8.0f);
                }

                auto text = r.withTrimmedLeft (28.0f).withTrimmedRight (10.0f);

                if (browser.bank.isFavourite (e.text))
                {
                    g.setColour (chosen ? colours::starLit : colours::starLit.withAlpha (0.7f));
                    g.fillPath (star ({ text.getRight() - 5.0f, r.getCentreY() }, 5.0f));
                    text.removeFromRight (16.0f);
                }

                g.setColour (chosen || i == hovered ? Colours::white : colours::mist.withAlpha (0.86f));
                g.setFont (fonts->bodyFont (13.0f));
                g.drawText (e.text, text, Justification::centredLeft, true);
            }
        }

        void mouseMove (const MouseEvent& e) override
        {
            const auto i = indexAt (e.getPosition());

            if (i != hovered)
            {
                hovered = i;
                repaint();
            }
        }

        void mouseExit (const MouseEvent&) override   { hovered = -1; repaint(); }

        void mouseDown (const MouseEvent& e) override
        {
            if (const auto i = indexAt (e.getPosition()); i >= 0)
            {
                browser.bank.load (entries[(size_t) i].text);
                browser.rebuild();
            }
        }

        void mouseDoubleClick (const MouseEvent& e) override
        {
            if (indexAt (e.getPosition()) >= 0)
                browser.close (true);
        }

        static constexpr int kRow = 30, kStride = 31;

        Browser& browser;
        Fonts fonts;
        std::vector<Entry> entries;
        bool columns = false;
        int rows = 1, columnWidth = 250, hovered = -1;
    };

    void layoutList()
    {
        const auto n = jmax (1, (int) list.entries.size());
        list.columns = getWidth() >= 700;

        if (list.columns)
        {
            list.rows = jmax (1, (viewport.getHeight() - viewport.getScrollBarThickness()) / List::kStride);
            const auto columnCount = (n + list.rows - 1) / list.rows;
            list.columnWidth = jmax (220, viewport.getWidth() / jmax (1, jmin (columnCount, viewport.getWidth() / 220)));
            list.setSize (columnCount * list.columnWidth, list.rows * List::kStride);
            viewport.setScrollBarsShown (false, true);
        }
        else
        {
            list.setSize (viewport.getWidth() - viewport.getScrollBarThickness() - 4, n * List::kStride);
            viewport.setScrollBarsShown (true, false);
        }

        list.repaint();
    }

    void disarm()
    {
        deleteButton.setArmed (false);
        deleteButton.setButtonText ("DELETE");
    }

    void beginNaming (bool renaming)
    {
        naming = renaming ? Naming::rename : Naming::save;
        nameEditor.setText (renaming ? bank.getCurrentName() : String(), dontSendNotification);
        nameEditor.setVisible (true);
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
        repaint();
    }

    void finishNaming (bool keep)
    {
        if (naming == Naming::none)
            return;

        const auto how = std::exchange (naming, Naming::none);
        const auto text = nameEditor.getText();
        nameEditor.setVisible (false);

        if (keep && text.trim().isNotEmpty())
        {
            if (how == Naming::rename)
                bank.rename (bank.getCurrentName(), text);
            else
                bank.save (text);
        }

        if (isVisible())
        {
            grabKeyboardFocus();
            rebuild();
        }
    }

    PresetBank& bank;
    Fonts fonts;

    IconButton closeButton { Icon::close, true };
    TextEditor search;
    Segment sources[3];
    OwnedArray<Chip> chips;
    IconButton favouriteButton { Icon::starOutline, false };
    Segment deleteButton { "DELETE" }, renameButton { "RENAME" }, saveButton { "SAVE" };
    TextEditor nameEditor;
    Viewport viewport;
    List list { *this };

    enum class Naming { none, save, rename } naming = Naming::none;

    Rectangle<int> titleArea, searchArea, sourceWell, tagsHeading, currentArea, originArea, nameArea, actionWell;
    int count = 0;

    ValueTree before;
    String beforeName;

    JUCE_DECLARE_NON_COPYABLE (Browser)
};

//==============================================================================
/** INTERFACE COLOUR : une pastille par teinte (OniriqueTheme.h) ; la choisie a un anneau blanc. */
class Swatches final : public Component
{
public:
    Swatches() { setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onChange;

    void paint (Graphics& g) override
    {
        const auto cur = onirique::look::ember.getARGB() | 0xff000000u;
        for (int i = 0; i < onirique::look::numSwatches; ++i)
        {
            const auto r = dot (i);
            const Colour c (onirique::look::swatches[i].argb);
            const bool sel = (onirique::look::swatches[i].argb | 0xff000000u) == cur;
            if (i == hover || sel)
            {
                g.setColour (c.withAlpha (sel ? 0.35f : 0.2f));
                g.fillEllipse (r.expanded (4.0f));
            }
            g.setGradientFill (ColourGradient (c.brighter (0.25f), r.getX(), r.getY(), c.darker (0.45f), r.getRight(), r.getBottom(), false));
            g.fillEllipse (r);
            g.setColour (sel ? Colours::white : Colours::white.withAlpha (0.12f));
            g.drawEllipse (r.expanded (sel ? 2.0f : 0.0f), sel ? 1.6f : 1.0f);
        }
    }
    void mouseMove (const MouseEvent& e) override { const int h = at (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = at (e.position);
        if (i < 0) return;
        onirique::look::choose (onirique::look::swatches[i].argb);
        repaint();
        if (onChange) onChange();
    }

private:
    Rectangle<float> dot (int i) const
    {
        const float n = (float) onirique::look::numSwatches, w = (float) getWidth() / n, d = jmin (18.0f, w - 10.0f);
        return Rectangle<float> (d, d).withCentre ({ w * ((float) i + 0.5f), (float) getHeight() * 0.5f });
    }
    int at (Point<float> p) const
    {
        for (int i = 0; i < onirique::look::numSwatches; ++i) if (dot (i).expanded (4.0f).contains (p)) return i;
        return -1;
    }
    int hover = -1;
};

//==============================================================================
/** Le menu ⋯ : comparer, importer, la taille de la fenêtre. */
class Menu final : public Component
{
public:
    Menu (PresetBank& b, bool withClipboard) : bank (b), clipboard (withClipboard)
    {
        copySlot.onClick = [this] { bank.copyToOtherSlot(); done(); };
        copySettings.onClick = [this] { bank.copyToClipboard(); done(); };
        pasteSettings.onClick = [this] { bank.paste(); done(); };

        importPresets.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Import Presets", File::getSpecialLocation (File::userDocumentsDirectory),
                                                     "*" + bank.getExtension());
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles
                                      | FileBrowserComponent::canSelectMultipleItems,
                                  [this] (const FileChooser& c) { bank.importFiles (c.getResults()); });
            done();
        };

        showFolder.onClick = [this]
        {
            bank.getFolder().createDirectory();
            bank.getFolder().startAsProcess();
            done();
        };

        for (auto* item : { &copySlot, &copySettings, &pasteSettings, &importPresets, &showFolder })
            addChildComponent (item);

        copySlot.setVisible (true);
        importPresets.setVisible (true);
        showFolder.setVisible (true);
        copySettings.setVisible (clipboard);
        pasteSettings.setVisible (clipboard);

        addAndMakeVisible (swatches);
        swatches.onChange = [this] { if (onTheme) onTheme(); };

        for (int i = 0; i < kNumZooms; ++i)
        {
            zooms[i].setButtonText (String (kZooms[i]));
            zooms[i].setMonoText (true);
            zooms[i].onClick = [this, i] { if (onZoom) onZoom ((float) kZooms[i] / 100.0f); refresh(); };
            addAndMakeVisible (zooms[i]);
        }
    }

    std::function<void()> onDone;
    std::function<void (float)> onZoom;          // la fenêtre à cette part de sa taille par défaut
    std::function<float()> getZoom;              // où elle en est
    std::function<bool (float)> fitsOnScreen;    // une taille plus grande que l'écran n'est pas proposée
    std::function<Point<int>()> getPixels;       // sa taille, en pixels
    std::function<void()> onTheme;                // une autre couleur d'interface : la fenêtre se redessine

    void refresh()
    {
        copySlot.setButtonText (bank.getSlot() == 0 ? "Copy A to B" : "Copy B to A");
        pasteSettings.setEnabled (bank.canPaste());

        const auto zoom = getZoom ? getZoom() : 1.0f;

        for (int i = 0; i < kNumZooms; ++i)
        {
            zooms[i].setToggleState (std::abs (zoom - (float) kZooms[i] / 100.0f) < 0.02f, dontSendNotification);
            zooms[i].setEnabled (! fitsOnScreen || fitsOnScreen ((float) kZooms[i] / 100.0f));
        }

        repaint();
    }

    int idealHeight() const   { return 6 + (clipboard ? 5 : 3) * 33 + (withZoom ? 11 + 70 : 0) + 11 + 62 + 6; }

    /** Sans la taille de la fenêtre (une application, qui a la sienne). */
    void setZoomVisible (bool shouldShow)
    {
        withZoom = shouldShow;

        for (auto& zoom : zooms)
            zoom.setVisible (withZoom);

        resized();
        repaint();
    }

    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (colours::card);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (colours::rose.withAlpha (0.14f));
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);

        if (! withZoom)
        {
            drawHeading (g, *fonts, "INTERFACE COLOUR", colourHeading.toFloat(), false);
            g.setColour (colours::rose.withAlpha (0.1f));
            g.fillRect (separator2.toFloat());
            return;
        }

        g.setColour (colours::rose.withAlpha (0.1f));
        g.fillRect (separator.toFloat());

        drawHeading (g, *fonts, "RESIZE", resizeHeading.toFloat(), false);
        drawHeading (g, *fonts, "INTERFACE COLOUR", colourHeading.toFloat(), false);
        g.setColour (colours::rose.withAlpha (0.1f));
        g.fillRect (separator2.toFloat());

        if (getPixels)
        {
            const auto p = getPixels();
            g.setColour (colours::emberLit);
            g.setFont (fonts->monoBoldFont (9.5f, 0.08f));
            g.drawText (String (p.x) + String::fromUTF8 (" × ") + String (p.y), resizeHeading.toFloat(), Justification::centredRight, false);
        }

        drawWell (g, zoomWell.toFloat());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);

        for (auto* item : { &copySlot, &copySettings, &pasteSettings, &importPresets, &showFolder })
            if (item->isVisible())
            {
                item->setBounds (r.removeFromTop (32));
                r.removeFromTop (1);
            }

        separator = r.removeFromTop (11).withSizeKeepingCentre (r.getWidth() - 16, 1);

        auto block = r.removeFromTop (70).reduced (10, 6);
        resizeHeading = block.removeFromTop (14);
        block.removeFromTop (8);
        zoomWell = block.removeFromTop (30);

        auto inner = zoomWell.reduced (2);
        const auto w = (inner.getWidth() - 2 * (kNumZooms - 1)) / kNumZooms;

        for (int i = 0; i < kNumZooms; ++i)
        {
            zooms[i].setBounds (inner.removeFromLeft (i < kNumZooms - 1 ? w : inner.getWidth()));
            inner.removeFromLeft (2);
        }

        // la couleur de l'interface, sous la taille de la fenêtre
        if (! withZoom) r = getLocalBounds().reduced (6).withTrimmedTop (separator.getY() - 6);
        separator2 = r.removeFromTop (11).withSizeKeepingCentre (r.getWidth() - 16, 1);
        auto cb = r.removeFromTop (62).reduced (10, 6);
        colourHeading = cb.removeFromTop (14);
        cb.removeFromTop (6);
        swatches.setBounds (cb.removeFromTop (30));
    }

    void mouseDown (const MouseEvent&) override {}

private:
    void done()   { if (onDone) onDone(); }

    static constexpr int kNumZooms = 6;
    static constexpr int kZooms[kNumZooms] = { 75, 100, 125, 150, 175, 200 };

    PresetBank& bank;
    Fonts fonts;
    bool clipboard, withZoom = true;

    MenuItem copySlot { "Copy A to B", Icon::copy }, copySettings { "Copy Settings", Icon::copy },
             pasteSettings { "Paste Settings", Icon::paste }, importPresets { String::fromUTF8 ("Import Presets…"), Icon::import },
             showFolder { "Show Presets Folder", Icon::folder };
    Segment zooms[kNumZooms];
    std::unique_ptr<FileChooser> chooser;

    Rectangle<int> separator, resizeHeading, zoomWell, separator2, colourHeading;
    Swatches swatches;

    JUCE_DECLARE_NON_COPYABLE (Menu)
};

//==============================================================================
/** La barre. Le plugin la pose en haut de sa fenêtre ; elle pose ses panneaux sur une feuille transparente qui
    couvre toute la fenêtre quand l'un d'eux est ouvert : un clic à côté le ferme. */
class Bar final : public Component,
                  private Timer
{
public:
    /** `unitsWidth` : la largeur de la façade en points. `defaultPixels` : la largeur de la fenêtre à 100 %. */
    Bar (PresetBank& b, AudioProcessorValueTreeState& s, const String& productName, const Image& sigilImage,
         int unitsWidth, int defaultPixels)
        : bank (b), state (s), product (productName.toUpperCase()), sigil (sigilImage),
          width (unitsWidth), defaultWidth (defaultPixels), compact (unitsWidth < 700),
          browser (b), menu (b, unitsWidth < 700)
    {
        setLookAndFeel (&look);
        sheet.setLookAndFeel (&look);

        previous.onClick = [this] { bank.step (-1); refresh(); };
        next.onClick = [this] { bank.step (1); refresh(); };
        field.onClick = [this] { if (browser.isVisible()) browser.close (true); else openBrowser(); };

        in.onClick = [this]
        {
            if (auto* bypass = state.getParameter ("bypass"))
            {
                bypass->beginChangeGesture();
                bypass->setValueNotifyingHost (bypass->getValue() > 0.5f ? 0.0f : 1.0f);
                bypass->endChangeGesture();
            }
        };

        slotA.onClick = [this] { bank.switchToSlot (0); refresh(); };
        slotB.onClick = [this] { bank.switchToSlot (1); refresh(); };
        copy.onClick = [this] { bank.copyToClipboard(); refresh(); };
        paste.onClick = [this] { bank.paste(); refresh(); };
        more.onClick = [this] { if (menu.isVisible()) closePanels(); else openMenu(); };

        // Ce que fait chaque bouton, pour un pointeur qui s'y pose.
        previous.setTooltip ("Le preset d'avant, parmi ceux que les filtres du navigateur laissent passer.");
        next.setTooltip (String::fromUTF8 ("Le preset d'après, parmi ceux que les filtres du navigateur laissent passer."));
        field.setTooltip (String::fromUTF8 ("Le preset chargé. Un clic ouvre le navigateur : chercher, filtrer, enregistrer."));
        in.setTooltip (String::fromUTF8 ("IN : allumé, le plugin est en service ; éteint, le son ressort tel qu'il est entré."));
        slotA.setTooltip (String::fromUTF8 ("A : un premier jeu de réglages, à comparer avec B."));
        slotB.setTooltip (String::fromUTF8 ("B : un second jeu de réglages. Le menu ⋯ copie l'un dans l'autre."));
        copy.setTooltip (String::fromUTF8 ("Copie les réglages, à coller dans une autre instance du plugin."));
        paste.setTooltip (String::fromUTF8 ("Colle les réglages copiés dans une autre instance du plugin."));
        more.setTooltip (String::fromUTF8 ("Plus : copier A dans B, importer des presets, la taille de la fenêtre."));

        for (Component* c : { (Component*) &previous, (Component*) &next, (Component*) &field, (Component*) &in,
                              (Component*) &slotA, (Component*) &slotB, (Component*) &more })
            addAndMakeVisible (c);

        addChildComponent (copy);
        addChildComponent (paste);
        copy.setVisible (! compact);
        paste.setVisible (! compact);

        browser.onClose = [this] { panelsChanged(); };
        menu.onDone = [this] { closePanels(); };
        menu.onZoom = [this] (float zoom) { setZoom (zoom); };
        menu.onTheme = [this] { repaint(); if (onThemeChanged) onThemeChanged(); };
        menu.getZoom = [this] { auto* e = editor(); return e != nullptr ? (float) e->getWidth() / (float) defaultWidth : 1.0f; };
        menu.getPixels = [this] { auto* e = editor(); return e != nullptr ? Point<int> (e->getWidth(), e->getHeight()) : Point<int>(); };
        menu.fitsOnScreen = [this] (float zoom)
        {
            auto* e = editor();
            const auto* display = e != nullptr ? Desktop::getInstance().getDisplays().getDisplayForRect (e->getScreenBounds()) : nullptr;
            return display == nullptr || (float) defaultWidth * zoom <= (float) display->userArea.getWidth();
        };

        sheet.onOutside = [this] { closePanels(); };
        sheet.addChildComponent (browser);
        sheet.addChildComponent (menu);

        refresh();
        startTimerHz (8);
    }

    ~Bar() override
    {
        sheet.setLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    /** La hauteur de la barre, en points, au-dessus d'une façade de cette largeur. */
    static int heightFor (int unitsWidth)   { return unitsWidth < 700 ? 84 : 50; }

    void setPillWidth (int w)   { if (pillWidth != w) { pillWidth = w; resized(); } }

    /** Où va la pastille de licence, en points. */
    Rectangle<int> getPillArea() const   { return pillArea; }

    /** Le titre du plugin à la place de la marque « ONIRIQUE » + nom : sa largeur en points, et ce qui le dessine
        (en points, dans la zone de la marque ; il peut déborder en hauteur sur toute la barre). */
    void setBrand (int widthInPoints, std::function<void (Graphics&, Rectangle<float>)> painter)
    {
        customBrandWidth = widthInPoints;
        brandPainter = std::move (painter);
        resized();
    }

    /** La zone de la marque, en points (pour la redessiner seule quand elle s'anime). */
    Rectangle<int> getBrandArea() const   { return brandArea; }

    /** Sans marque ni titre (le plugin écrit déjà son nom ailleurs) : étroite, la barre tient alors sur une seule
        rangée, ⋯ au bout. */
    void setWithoutBrand()
    {
        noBrand = true;
        resized();
    }

    /** Une autre couleur d'interface vient d'être choisie dans le menu ⋯ : l'éditeur se redessine. */
    std::function<void()> onThemeChanged;

    /** Dans une application plutôt qu'un plugin : la fenêtre a sa propre taille, le menu ⋯ ne la propose pas. */
    void setWithoutZoom()   { menu.setZoomVisible (false); layoutPanels(); }

    /** La hauteur de cette barre, en points : celle de heightFor(), une seule rangée sans marque. */
    int getBarHeight() const   { return twoRows() ? heightFor (width) : 50; }

    /** Dans resized() du plugin : la barre en haut à ce zoom, la feuille de ses panneaux sur toute la fenêtre. */
    void place (float scale, Rectangle<int> editorBounds)
    {
        setBounds (0, 0, width, getBarHeight());
        setTransform (AffineTransform::scale (scale));
        sheet.setBounds (0, 0, width, roundToInt ((float) editorBounds.getHeight() / scale));
        sheet.setTransform (AffineTransform::scale (scale));
        layoutPanels();
    }

    void openBrowser()
    {
        menu.setVisible (false);
        sheet.setVisible (true);
        browser.open();
        panelsChanged();
    }

    void openMenu()
    {
        browser.close (true);
        menu.refresh();
        menu.setVisible (true);
        sheet.setVisible (true);
        panelsChanged();
    }

    void paint (Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();

        // Le noir de la barre, et la lueur rouge seulement derrière la marque.
        g.setColour (colours::bar);
        g.fillRect (r);
        g.setGradientFill (ColourGradient (onirique::look::emberDeep.withMultipliedBrightness (1.1f).withAlpha (0.6f), 110.0f, -20.0f, onirique::look::emberDeep.withAlpha (0.0f), 430.0f, -20.0f, true));
        g.fillRect (r);

        g.setColour (colours::rose.withAlpha (0.1f));
        g.fillRect (r.withTop (r.getBottom() - 1.0f));

        paintBrand (g);

        for (auto well : { navWell, inWell, slotWell })
            drawWell (g, well.toFloat());

        if (! compact)
            drawWell (g, clipWell.toFloat());
    }

    void resized() override
    {
        constexpr int pad = 14, gap = 8, h = 30;
        const auto rowOf = [&] (int centre) { return Rectangle<int> (pad, centre - h / 2, width - 2 * pad, h); };
        auto top = rowOf (twoRows() ? 21 : 25);
        auto row = twoRows() ? rowOf (63) : top;

        // À droite de la première rangée : ⋯, et la place de la pastille.
        more.setBounds (top.removeFromRight (h));
        top.removeFromRight (gap);
        pillArea = top.removeFromRight (pillWidth);
        brandArea = top.withWidth (brandWidth());

        // Étroite et sans marque : une seule rangée, qui laisse le bout à ⋯ et à la pastille.
        if (compact && ! twoRows())
            row.removeFromRight (h + gap + pillWidth + (pillWidth > 0 ? gap : 0));

        if (! compact)
            row.removeFromLeft (brandArea.getWidth() + 18);

        auto group = [&] (Rectangle<int>& well, std::initializer_list<Component*> parts, std::initializer_list<int> widths)
        {
            int total = 4 + 2 * ((int) parts.size() - 1);

            for (auto w : widths)
                total += w;

            well = row.removeFromLeft (total);
            row.removeFromLeft (gap);
            auto inner = well.reduced (2);
            auto w = widths.begin();

            for (auto* part : parts)
            {
                part->setBounds (inner.removeFromLeft (*w++));
                inner.removeFromLeft (2);
            }
        };

        group (navWell, { &previous, &next }, { 24, 24 });

        // Le champ du preset prend ce que laissent les autres, 330 au plus.
        const auto others = in.preferredWidth() + slotA.preferredWidth() + slotB.preferredWidth() + 4 + 6 + 2 * gap
                            + (compact ? 0 : copy.preferredWidth() + paste.preferredWidth() + 6 + gap)
                            + (compact ? 0 : pillWidth + h + 2 * gap);
        const auto fieldWidth = jlimit (120, compact ? 1000 : 330, row.getWidth() - others);
        field.setBounds (row.removeFromLeft (fieldWidth));
        row.removeFromLeft (gap);

        group (inWell, { &in }, { in.preferredWidth() });
        group (slotWell, { &slotA, &slotB }, { slotA.preferredWidth(), slotB.preferredWidth() });

        if (! compact)
            group (clipWell, { &copy, &paste }, { copy.preferredWidth(), paste.preferredWidth() });

        repaint();
    }

private:
    /** Transparente, sur toute la fenêtre, quand un panneau est ouvert : elle prend les clics à côté. */
    struct Sheet final : Component
    {
        std::function<void()> onOutside;

        void mouseDown (const MouseEvent&) override   { if (onOutside) onOutside(); }

        void paint (Graphics& g) override
        {
            // L'ombre des cartes ouvertes, qu'elles ne peuvent pas dessiner hors d'elles-mêmes.
            for (auto* child : getChildren())
                if (child->isVisible())
                {
                    Path p;
                    p.addRoundedRectangle (child->getBounds().toFloat(), 14.0f);
                    DropShadow (Colours::black.withAlpha (0.7f), 36, { 0, 14 }).drawForPath (g, p);
                }
        }
    };

    void parentHierarchyChanged() override
    {
        // La feuille va dans la fenêtre du plugin, juste au-dessus de la barre : la pastille de licence,
        // ajoutée après, reste par-dessus.
        if (auto* parent = getParentComponent(); parent != nullptr && sheet.getParentComponent() != parent)
            parent->addChildComponent (sheet);
    }

    AudioProcessorEditor* editor() const   { return findParentComponentOfClass<AudioProcessorEditor>(); }

    void setZoom (float zoom)
    {
        if (auto* e = editor())
        {
            const auto ratio = e->getConstrainer() != nullptr && e->getConstrainer()->getFixedAspectRatio() > 0.0
                                   ? e->getConstrainer()->getFixedAspectRatio()
                                   : (double) e->getWidth() / (double) e->getHeight();
            const auto w = roundToInt ((float) defaultWidth * zoom);
            e->setSize (w, roundToInt (w / ratio));
        }
    }

    void layoutPanels()
    {
        const auto barHeight = getBarHeight();
        const auto below = sheet.getLocalBounds().withTrimmedTop (barHeight).reduced (8);

        // Le navigateur part du champ du preset, aussi large qu'il le faut sans dépasser.
        const auto browserWidth = jmin (below.getWidth(), compact ? below.getWidth() : 880);
        const auto x = jlimit (below.getX(), below.getRight() - browserWidth, field.getX());
        browser.setBounds (x, below.getY(), browserWidth, below.getHeight());

        const auto menuWidth = jmin (262, below.getWidth());
        menu.setBounds (below.getRight() - menuWidth, below.getY(), menuWidth, jmin (below.getHeight(), menu.idealHeight()));
    }

    void closePanels()
    {
        browser.close (true);
        menu.setVisible (false);
        panelsChanged();
    }

    void panelsChanged()
    {
        sheet.setVisible (browser.isVisible() || menu.isVisible());
        sheet.repaint();
        refresh();
    }

    bool twoRows() const   { return compact && ! noBrand; }

    int brandWidth() const
    {
        if (noBrand)
            return 0;

        if (brandPainter)
            return customBrandWidth;

        const auto size = compact ? 12.0f : 13.0f;
        return roundToInt ((compact ? 26.0f : 30.0f) + 9.0f + GlyphArrangement::getStringWidth (fonts->displayFont (size, 0.06f), "ONIRIQUE")
                           + 9.0f + GlyphArrangement::getStringWidth (fonts->bodyFont (size, 0.22f), product) + (compact ? 0.0f : 30.0f));
    }

    void paintBrand (Graphics& g)
    {
        if (noBrand)
            return;

        if (brandPainter)
        {
            brandPainter (g, brandArea.toFloat());
            return;
        }

        auto r = brandArea.toFloat();
        const auto size = compact ? 12.0f : 13.0f;
        const auto mark = compact ? 26.0f : 30.0f;

        if (sigil.isValid())
            g.drawImage (sigil, r.removeFromLeft (mark).withSizeKeepingCentre (mark, mark), RectanglePlacement::centred);
        else
            drawLed (g, r.removeFromLeft (mark).getCentre(), 8.0f, true);

        r.removeFromLeft (9.0f);
        const auto display = fonts->displayFont (size, 0.06f);
        g.setColour (Colours::white);
        g.setFont (display);
        g.drawText ("ONIRIQUE", r, Justification::centredLeft, false);
        r.removeFromLeft (GlyphArrangement::getStringWidth (display, "ONIRIQUE") + 9.0f);

        const auto body = fonts->bodyFont (size, 0.22f);
        g.setColour (colours::brandName);
        g.setFont (body);
        g.drawText (product, r, Justification::centredLeft, false);

        if (compact)
            return;

        // Les petites étoiles de la marque.
        const auto x = r.getX() + GlyphArrangement::getStringWidth (body, product) + 6.0f, y = r.getY();
        g.setColour (colours::rose.withAlpha (0.75f));
        g.fillPath (star ({ x + 6.5f, y + 4.0f }, 3.5f));
        g.setColour (colours::bone);
        g.fillPath (star ({ x + 17.0f, y + 13.0f }, 5.0f));
        g.setColour (colours::rose.withAlpha (0.55f));
        g.fillPath (star ({ x + 7.5f, y + 25.0f }, 2.5f));
    }

    void timerCallback() override   { refresh(); }

    void refresh()
    {
        const auto name = bank.getCurrentName();
        field.show (name, bank.isStock (name) ? "FACTORY" : bank.isUser (name) ? "USER" : "", browser.isVisible());
        more.setOpen (menu.isVisible());

        if (auto* bypass = state.getParameter ("bypass"))
            in.setToggleState (bypass->getValue() < 0.5f, dontSendNotification);

        slotA.setToggleState (bank.getSlot() == 0, dontSendNotification);
        slotB.setToggleState (bank.getSlot() == 1, dontSendNotification);

        // Le presse-papier, pas à chaque tour.
        if (! compact && ++ticks % 4 == 0)
            paste.setEnabled (bank.canPaste());

        if (browser.isVisible() && name != shownName)
            browser.rebuild();

        shownName = name;
    }

    Look look;
    PresetBank& bank;
    AudioProcessorValueTreeState& state;
    String product;
    Image sigil;
    Fonts fonts;
    const int width, defaultWidth;
    const bool compact;

    IconButton previous { Icon::previous, false }, next { Icon::next, false }, more { Icon::dots, true };
    PresetField field;
    Segment in { "IN", true }, slotA { "A", true }, slotB { "B", true }, copy { "COPY" }, paste { "PASTE" };

    Sheet sheet;
    Browser browser;
    Menu menu;

    Rectangle<int> brandArea, navWell, inWell, slotWell, clipWell, pillArea;
    int pillWidth = 0, ticks = 0, customBrandWidth = 0;
    bool noBrand = false;
    std::function<void (Graphics&, Rectangle<float>)> brandPainter;
    String shownName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Bar)
};

} // namespace onirique::nuit
