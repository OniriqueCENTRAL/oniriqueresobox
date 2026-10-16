#include "PluginEditor.h"
#include "ui/OniriqueWindowSize.h"

#include <BinaryData.h>

namespace onirique
{

using namespace theme;

namespace
{
    OniriqueLookAndFeel& sharedLookAndFeel()
    {
        static OniriqueLookAndFeel laf;
        return laf;
    }

    constexpr int kHeaderHeight = 68;
    constexpr int kTitleLeft = 72;
    constexpr int kColumnWidth = 208;
    constexpr int kFootHeight = 54;

    Font titleFont (bool heavy)
    {
        return displayFont (23.0f, heavy).withExtraKerningFactor (0.09f);
    }

    /** A four-pointed star, as on the Onirique sigil. */
    Path star (Point<float> centre, float radius)
    {
        Path p;
        const auto waist = radius * 0.16f;

        p.startNewSubPath (centre.x, centre.y - radius);
        p.lineTo (centre.x + waist, centre.y - waist);
        p.lineTo (centre.x + radius, centre.y);
        p.lineTo (centre.x + waist, centre.y + waist);
        p.lineTo (centre.x, centre.y + radius);
        p.lineTo (centre.x - waist, centre.y + waist);
        p.lineTo (centre.x - radius, centre.y);
        p.lineTo (centre.x - waist, centre.y - waist);
        p.closeSubPath();
        return p;
    }

    constexpr int kLogoWidth = 44;
    constexpr int kLogoMargin = 8;   // room for the glow around it

    Image makeLogo()
    {
        const auto sigil = ImageCache::getFromMemory (BinaryData::sigil_png, BinaryData::sigil_pngSize);

        if (! sigil.isValid())
            return {};

        const auto width = 2 * kLogoWidth;
        const auto height = width * sigil.getHeight() / sigil.getWidth();
        const auto margin = 2 * kLogoMargin;

        Image padded (Image::ARGB, width + 2 * margin, height + 2 * margin, true);

        {
            Graphics g (padded);
            g.setImageResamplingQuality (Graphics::highResamplingQuality);
            g.drawImage (sigil, Rectangle<int> (margin, margin, width, height).toFloat());
        }

        Image result (Image::ARGB, padded.getWidth(), padded.getHeight(), true);
        Graphics g (result);

        GlowEffect glow;
        glow.setGlowProperties (7.0f, ember.withAlpha (0.8f));
        glow.applyEffect (padded, g, 1.0f, 1.0f);

        return result;
    }
}

ResoBoxEditor::ResoBoxEditor (ResoBoxProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      display (p)
{
    setLookAndFeel (&sharedLookAndFeel());
    logo = makeLogo();

    auto& state = processor.apvts;

    addAndMakeVisible (display);

    // Presets.
    for (auto* b : { &previousButton, &nextButton, &saveButton, &deleteButton,
                     &slotAButton, &slotBButton, &copyButton })
    {
        b->getProperties().set ("plain", true);
        addAndMakeVisible (*b);
    }

    previousButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    saveButton.onClick = [this] { beginSaving(); };
    deleteButton.onClick = [this] { deleteClicked(); };

    presetBox.setTextWhenNothingSelected ("Default");
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedId() - 1;

        if (isPositiveAndBelow (index, presetNames.size()))
            processor.loadPreset (presetNames[index]);
    };
    addAndMakeVisible (presetBox);

    // The name of a preset being saved is typed where its name is shown.
    // It lies over the menu and looks like it, with the text where the menu
    // had it, so that the name seems to be typed into the menu itself.
    presetNameEditor.getProperties().set ("box", true);
    presetNameEditor.setJustificationType (Justification::centredLeft);
    presetNameEditor.setBorderSize ({ 0, 13, 0, 30 });
    presetNameEditor.setEditable (true);
    presetNameEditor.onTextChange = [this]
    {
        const auto saved = processor.saveUserPreset (presetNameEditor.getText());

        if (saved.isNotEmpty())
            refreshPresets();
    };
    presetNameEditor.onEditorHide = [this] { presetNameEditor.setVisible (false); };
    addChildComponent (presetNameEditor);

    refreshPresets();

    // A and B.
    slotAButton.onClick = [this] { processor.switchToSlot (0); };
    slotBButton.onClick = [this] { processor.switchToSlot (1); };
    copyButton.onClick = [this] { processor.copyToOtherSlot(); };

    // Soft and hard are the two halves of one switch.
    for (auto* b : { &softButton, &hardButton })
        addAndMakeVisible (*b);

    modeAttachment = std::make_unique<ParameterAttachment> (*state.getParameter ("hard"), [this] (float value)
    {
        softButton.setToggleState (value < 0.5f, dontSendNotification);
        hardButton.setToggleState (value >= 0.5f, dontSendNotification);
    });

    softButton.onClick = [this] { modeAttachment->setValueAsCompleteGesture (0.0f); };
    hardButton.onClick = [this] { modeAttachment->setValueAsCompleteGesture (1.0f); };
    modeAttachment->sendInitialUpdate();

    const std::pair<Knob*, const char*> knobs[] = { { &depth, "depth" }, { &detail, "detail" }, { &tilt, "tilt" },
                                                    { &attack, "attack" }, { &release, "release" } };

    for (const auto& [knob, id] : knobs)
    {
        knob->attach (state, id);
        addAndMakeVisible (*knob);
    }

    depth.setEnds ("0", "10");

    auto choice = [this, &state] (ComboBox& box, const String& id, const String& heading)
    {
        box.addSectionHeading (heading);

        if (auto* parameter = dynamic_cast<AudioParameterChoice*> (state.getParameter (id)))
            for (int i = 0; i < parameter->choices.size(); ++i)
                box.addItem (parameter->choices[i].toUpperCase(), i + 1);

        comboAttachments.push_back (std::make_unique<ComboBoxAttachment> (state, id, box));
        addAndMakeVisible (box);
    };

    choice (qualityBox, "quality", "QUALITY");
    choice (stereoBox, "mode", "STEREO MODE");

    const std::pair<TextButton*, const char*> buttons[] = { { &deltaButton, "delta" }, { &bypassButton, "bypass" },
                                                            { &sidechainButton, "sidechain" },
                                                            { &lowLatencyButton, "lowlat" },
                                                            { &autoGainButton, "autogain" } };

    for (const auto& [button, id] : buttons)
    {
        button->setClickingTogglesState (true);
        buttonAttachments.push_back (std::make_unique<ButtonAttachment> (state, id, *button));
        addAndMakeVisible (*button);
    }

    const std::pair<ValueField*, const char*> fields[] = { { &mix, "mix" }, { &output, "out" },
                                                           { &link, "link" }, { &balance, "balance" } };

    for (const auto& [field, id] : fields)
    {
        sliderAttachments.push_back (std::make_unique<SliderAttachment> (state, id, *field));
        addAndMakeVisible (*field);
    }

    describe ({ &previousButton }, fr ("Preset précédent."));
    describe ({ &nextButton }, fr ("Preset suivant."));
    describe ({ &presetBox }, fr ("Presets : ceux d'usine d'abord, puis les vôtres."));
    describe ({ &saveButton }, fr ("Enregistre les réglages actuels comme preset personnel : tapez son nom à la place du preset affiché, puis Entrée. Échap annule."));
    describe ({ &deleteButton }, fr ("Supprime ce preset. Seuls vos presets personnels se suppriment, et un second clic est demandé."));
    describe ({ &slotAButton }, fr ("Emplacement A : l'un des deux réglages gardés côte à côte, pour les comparer."));
    describe ({ &slotBButton }, fr ("Emplacement B : l'autre réglage. Il part d'une copie de A."));
    describe ({ &copyButton }, fr ("Copie le réglage en cours d'écoute dans l'autre emplacement."));
    describe ({ &deltaButton }, fr ("Delta : n'écouter que ce qui est retiré."));
    describe ({ &bypassButton }, fr ("Bypass. La latence est gardée : la piste ne se décale pas."));

    describe ({ &softButton }, fr ("Soft : les coupes s'installent en douceur et, au plus, aplanissent un pic."));
    describe ({ &hardButton }, fr ("Hard : les coupes s'installent net et vont deux fois plus loin."));
    describe ({ &depth }, fr ("Depth : la part de ce qui dépasse du spectre qui est retirée. À 0, rien n'est touché. Cliquez sur la valeur pour la taper."));
    describe ({ &detail }, fr ("Detail : l'échelle à laquelle les résonances sont cherchées. Bas, il trouve les bosses larges et coupe large ; haut, il trouve les sifflements isolés et fait des encoches."));
    describe ({ &tilt }, fr ("Tilt : penche toute la sensibilité vers l'aigu (+) ou vers le grave (-), autour de 1 kHz."));
    describe ({ &attack }, fr ("Attack : la vitesse à laquelle une coupe s'installe quand une résonance apparaît."));
    describe ({ &release }, fr ("Release : la vitesse à laquelle une coupe se relâche une fois la résonance passée."));

    describe ({ &mix }, fr ("Mix : la part du signal traité par rapport au signal d'origine. Glissez vers le haut ou le bas."));
    describe ({ &output }, fr ("Niveau de sortie, après tout le reste. Glissez vers le haut ou le bas."));
    describe ({ &autoGainButton }, fr ("Auto gain : rend le niveau que les coupes ont enlevé, jusqu'à 12 dB."));
    describe ({ &stereoBox }, fr ("Mode stéréo : gauche et droite, mid et side, ou mono - un seul détecteur sur la somme, et la même coupe des deux côtés."));
    describe ({ &link }, fr ("Link : à 100 %, les deux canaux reçoivent la plus profonde des deux coupes, ce qui garde l'image stable ; à 0 %, chacun est traité seul."));
    describe ({ &balance }, fr ("Balance : épargne un canal - gauche ou droite, ou mid ou side dans ce mode."));
    describe ({ &sidechainButton }, fr ("Sidechain : les résonances sont cherchées dans l'entrée latérale, et coupées dans cette piste."));
    describe ({ &qualityBox }, fr ("Quality : la longueur de la trame de traitement. Plus longue, elle applique les coupes étroites plus exactement, au prix de la latence. Ce qui est détecté ne change pas."));
    describe ({ &lowLatencyButton }, fr ("Low latency : la trame la plus courte, environ 21 ms, quelle que soit la qualité. Les coupes étroites sortent un peu plus larges."));

    hoverTip.setInterceptsMouseClicks (false, false);
    hoverTip.setAlwaysOnTop (true);
    addChildComponent (hoverTip);

    setResizable (true, true);
    setResizeLimits (1270, 620, 2000, 1250);
    // taille d'ouverture : la règle commune des plugins ONIRIQUE (ui/OniriqueWindowSize.h)
    const auto opening = onirique::window::openingSize ({}, "ONIRIQUE RESOBOX", { 1290, 740 }, { 1270, 620 }, { 2000, 1250 });
    setSize (opening.x, opening.y);

    timerCallback();
    // The licence pill and the activation page, over everything else.
    licenseOverlay = std::make_unique<license::Overlay> ("ONIRIQUE RESOBOX", processor.license);
    addAndMakeVisible (*licenseOverlay);
    resized();

    startTimerHz (30);
}

ResoBoxEditor::~ResoBoxEditor()
{
    onirique::window::rememberOnThisComputer ("ONIRIQUE RESOBOX", getWidth(), getHeight());
    setLookAndFeel (nullptr);
}

//==============================================================================
void ResoBoxEditor::refreshPresets()
{
    const auto stock = processor.getStockPresets();
    const auto user = processor.getUserPresets();

    presetNames = stock;
    stockCount = stock.size();
    presetNames.addArray (user);

    presetBox.clear (dontSendNotification);
    presetBox.addSectionHeading ("STOCK");

    for (int i = 0; i < stock.size(); ++i)
        presetBox.addItem (stock[i], i + 1);

    if (! user.isEmpty())
    {
        presetBox.addSectionHeading ("USER");

        for (int i = 0; i < user.size(); ++i)
            presetBox.addItem (user[i], stock.size() + i + 1);
    }

    presetBox.setSelectedId (presetNames.indexOf (processor.getPresetName()) + 1, dontSendNotification);
}

void ResoBoxEditor::stepPreset (int direction)
{
    if (presetNames.isEmpty())
        return;

    const auto count = presetNames.size();
    const auto current = presetNames.indexOf (processor.getPresetName());
    const auto next = current < 0 ? 0 : (current + direction + count) % count;

    processor.loadPreset (presetNames[next]);
}

void ResoBoxEditor::beginSaving()
{
    const auto current = processor.getPresetName();

    presetNameEditor.setText ({}, dontSendNotification);
    presetNameEditor.setVisible (true);
    presetNameEditor.showEditor();

    // The name on show is there, selected: Enter keeps it, typing replaces
    // it, Escape gives up. A stock preset's name comes back with "(user)".
    if (auto* editor = presetNameEditor.getCurrentTextEditor())
    {
        // Where the menu wrote it: the same inset, on the middle line.
        editor->setBorder ({ 0, 13, 0, 30 });
        editor->setIndents (0, 0);
        editor->setJustification (Justification::centredLeft);
        editor->setText (current, false);
        editor->selectAll();
    }
}

void ResoBoxEditor::deleteClicked()
{
    // Deleting is for good, so it takes two clicks: the first asks.
    if (deleteArmed == 0)
    {
        deleteArmed = 90;
        return;
    }

    deleteArmed = 0;
    processor.deleteUserPreset (processor.getPresetName());
    refreshPresets();
}

void ResoBoxEditor::timerCallback()
{
    // What a preset, the other slot or the host may have changed under us.
    const auto name = processor.getPresetName();
    const auto index = presetNames.indexOf (name);

    if (presetBox.getSelectedId() != index + 1)
        presetBox.setSelectedId (index + 1, dontSendNotification);

    if (deleteArmed > 0)
        --deleteArmed;

    deleteButton.setEnabled (index >= stockCount);
    deleteButton.setButtonText (deleteArmed > 0 ? "SURE ?" : "DELETE");
    deleteButton.setToggleState (deleteArmed > 0, dontSendNotification);

    const auto slot = processor.getActiveSlot();

    slotAButton.setToggleState (slot == 0, dontSendNotification);
    slotBButton.setToggleState (slot == 1, dontSendNotification);
    copyButton.setButtonText (slot == 0 ? "A > B" : "B > A");

    // The quality is the frame; with low latency on, the frame is not its to set.
    qualityBox.setEnabled (! lowLatencyButton.getToggleState());
    qualityBox.setAlpha (qualityBox.isEnabled() ? 1.0f : 0.4f);

    // The header is the only part of the frame that moves by itself.
    repaint (0, 0, getWidth(), kHeaderHeight);

    updateHoverTip();
}

//==============================================================================
void ResoBoxEditor::describe (std::initializer_list<Component*> components, const String& text)
{
    for (auto* c : components)
        described.push_back ({ c, text });
}

String ResoBoxEditor::descriptionAt (Point<int> at) const
{
    // The display knows its own insides: the island, the nodes, the readouts.
    if (display.getBounds().contains (at))
        return display.descriptionAt (at - display.getPosition());

    for (const auto& d : described)
        if (d.component->isVisible() && d.component->getBounds().contains (at))
            return d.text;

    return {};
}

void ResoBoxEditor::pinDescription (Point<int> at)
{
    pinned = true;
    hoverTip.text = descriptionAt (at);
    hoverTip.setBounds (getLookAndFeel().getTooltipBounds (hoverTip.text, at, getLocalBounds().reduced (6)));
    hoverTip.setVisible (hoverTip.text.isNotEmpty());
    hoverTip.toFront (false);
}

void ResoBoxEditor::updateHoverTip()
{
    if (pinned)
        return;


    const auto now = Time::getMillisecondCounter();
    const auto at = getMouseXYRelative();

    // Only while the pointer is over this window with no button held.
    auto* under = Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
    const auto here = under != nullptr && (under == this || isParentOf (under))
                      && ! ModifierKeys::currentModifiers.isAnyMouseButtonDown();

    const auto over = here ? descriptionAt (at) : String();

    // Leaving the control takes the description away and starts over.
    if (over != restingOn)
    {
        restingOn = over;
        restingAt = at;
        restingSince = now;
        hoverTip.setVisible (false);
        return;
    }

    // Once it is up, it stays for as long as the pointer is on that control,
    // moving or not.
    if (restingOn.isEmpty() || hoverTip.isVisible())
        return;

    // Until then, any movement starts the two seconds again.
    if (at.getDistanceFrom (restingAt) > 3)
    {
        restingAt = at;
        restingSince = now;
        return;
    }

    if (now - restingSince < 2000)
        return;

    hoverTip.text = restingOn;
    hoverTip.setBounds (getLookAndFeel().getTooltipBounds (restingOn, at, getLocalBounds().reduced (6)));
    hoverTip.setVisible (true);
    hoverTip.toFront (false);
    hoverTip.repaint();
}

int ResoBoxEditor::titleRight() const
{
    return kTitleLeft
           + roundToInt (GlyphArrangement::getStringWidth (titleFont (true), "ONIRIQUE")
                         + GlyphArrangement::getStringWidth (titleFont (false), "RESOBOX"))
           + 12;
}

void ResoBoxEditor::paintHeader (Graphics& g, Rectangle<int> header)
{
    const auto area = header.toFloat();
    const auto seconds = (Time::getMillisecondCounterHiRes() - openedAt) * 0.001;

    // The crimson of the range: darkest at the sigil, brightest behind the
    // name, and falling away again to the right and towards the foot.
    ColourGradient ground (Colour (0xff2c060b), 0.0f, 0.0f, Colour (0xff3b080e), area.getRight(), 0.0f, false);
    ground.addColour (0.32, Colour (0xff7a0e1c));
    ground.addColour (0.7, Colour (0xff5a0a14));
    g.setGradientFill (ground);
    g.fillRect (area);

    g.setGradientFill (ColourGradient (Colours::transparentBlack, 0.0f, area.getY(),
                                       Colour (0x73000000), 0.0f, area.getBottom(), false));
    g.fillRect (area);

    if (logo.isValid())
    {
        const auto width = (float) logo.getWidth() * 0.5f;
        const auto height = (float) logo.getHeight() * 0.5f;

        g.drawImage (logo, { 16.0f - (float) kLogoMargin, area.getCentreY() - height * 0.5f, width, height });
    }

    // The name: the house in white, the product in a paler weight beside it.
    const auto nameWidth = GlyphArrangement::getStringWidth (titleFont (true), "ONIRIQUE");

    g.setColour (Colours::white);
    g.setFont (titleFont (true));
    g.drawText ("ONIRIQUE", area.withTrimmedLeft ((float) kTitleLeft).withWidth (nameWidth + 4.0f),
                Justification::centredLeft, false);

    g.setColour (Colour (0xffe3b5ba));
    g.setFont (titleFont (false));
    g.drawText ("RESOBOX", area.withTrimmedLeft ((float) kTitleLeft + nameWidth + 10.0f).withWidth (260.0f),
                Justification::centredLeft, false);

    // Stars beside it, each breathing at its own pace.
    struct Spark { float x, y, radius, rate, offset; };
    static const Spark sparks[] = { { 10.0f, 17.0f, 5.5f, 0.9f, 0.0f },  { 27.0f, 11.0f, 3.0f, 1.3f, 1.7f },
                                    { 38.0f, 24.0f, 4.0f, 0.7f, 3.1f },  { 18.0f, 42.0f, 4.5f, 1.1f, 4.4f },
                                    { 34.0f, 48.0f, 2.5f, 1.6f, 2.3f },  { 46.0f, 38.0f, 2.0f, 1.0f, 5.2f } };

    const auto sparkLeft = (float) titleRight();

    for (const auto& s : sparks)
    {
        const auto glow = 0.5f + 0.5f * (float) std::sin (seconds * s.rate * 2.0 + s.offset);

        g.setColour (Colours::white.withAlpha (0.25f + 0.65f * glow));
        g.fillPath (star ({ sparkLeft + s.x, area.getY() + s.y }, s.radius * (0.8f + 0.2f * glow)));
    }

    // The foot of the header: a faint rule, and a streak of light that runs
    // along it from left to right and comes round again.
    const auto footY = area.getBottom() - 2.0f;

    g.setColour (Colour (0x30ffd2d6));
    g.fillRect (0.0f, footY, area.getWidth(), 1.0f);

    const auto length = jmax (160.0f, area.getWidth() * 0.22f);
    const auto cycle = 3.6;
    const auto travelled = (float) (std::fmod (seconds, cycle) / cycle);
    const auto head = -40.0f + travelled * (area.getWidth() + length + 80.0f);
    const auto tail = head - length;

    // Bright at the head, trailing off behind it, with a halo around the lot.
    ColourGradient halo (Colours::transparentBlack, tail, 0.0f, Colours::transparentBlack, head + 14.0f, 0.0f, false);
    halo.addColour (0.82, ember.withAlpha (0.55f));
    g.setGradientFill (halo);
    g.fillRect (tail, footY - 3.0f, length + 14.0f, 7.0f);

    ColourGradient streak (Colours::transparentWhite, tail, 0.0f, Colours::transparentWhite, head + 6.0f, 0.0f, false);
    streak.addColour (0.55, emberLit.withAlpha (0.9f));
    streak.addColour (0.93, Colours::white);
    g.setGradientFill (streak);
    g.fillRoundedRectangle (tail, footY - 0.5f, length + 6.0f, 2.0f, 1.0f);
}

void ResoBoxEditor::paint (Graphics& g)
{
    g.fillAll (night);

    paintHeader (g, getLocalBounds().removeFromTop (kHeaderHeight));

    // The column's ground, a shade redder than the rest.
    auto body = getLocalBounds().withTop (kHeaderHeight);
    const auto column = body.removeFromLeft (kColumnWidth);

    g.setColour (deep);
    g.fillRect (column);
    g.setColour (line);
    g.drawVerticalLine (column.getRight(), (float) column.getY(), (float) column.getBottom());

    // A rule between the two families of knobs: what is found, and how fast.
    const auto rule = (float) (attack.getY() - 9);
    g.fillRect ((float) column.getX() + 22.0f, rule, (float) column.getWidth() - 44.0f, 1.0f);

    // And between the three families of the foot.
    for (const auto x : { stereoBox.getX() - 11, sidechainButton.getX() - 11, qualityBox.getX() - 11 })
        g.fillRect ((float) x, (float) stereoBox.getY() + 3.0f, 1.0f, (float) stereoBox.getHeight() - 6.0f);
}

void ResoBoxEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (kHeaderHeight);

    // Controls sit in the upper part of the header, clear of the light that
    // runs along its foot.
    auto row = header.withTrimmedBottom (6).withSizeKeepingCentre (header.getWidth(), 30).withTrimmedRight (14);

    bypassButton.setBounds (row.removeFromRight (100));
    row.removeFromRight (8);
    deltaButton.setBounds (row.removeFromRight (90));
    row.removeFromRight (8);

    // The licence badge is one of the header's buttons, next to Delta and Bypass: the same height, the same
    // capsule. Its room follows its wording, and the header is laid out again when the licence changes.
    if (licenseOverlay != nullptr)
        licenseOverlay->fit (getLocalBounds(), 1.0f, row.removeFromRight (licenseOverlay->pillWidth()), Justification::centredRight);
    row.removeFromRight (18);

    copyButton.setBounds (row.removeFromRight (64));
    row.removeFromRight (4);
    slotBButton.setBounds (row.removeFromRight (34));
    row.removeFromRight (4);
    slotAButton.setBounds (row.removeFromRight (34));
    row.removeFromRight (18);

    deleteButton.setBounds (row.removeFromRight (74));
    row.removeFromRight (4);
    saveButton.setBounds (row.removeFromRight (60));
    row.removeFromRight (8);

    // The name takes what is left between the title and the buttons.
    row.removeFromLeft (titleRight() + 62);
    previousButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (4);
    nextButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (6);
    presetBox.setBounds (row);
    presetNameEditor.setBounds (row);

    auto column = bounds.removeFromLeft (kColumnWidth).reduced (14, 16);

    auto mode = column.removeFromTop (30);
    softButton.setBounds (mode.removeFromLeft (mode.getWidth() / 2).withTrimmedRight (3));
    hardButton.setBounds (mode.withTrimmedLeft (3));
    column.removeFromTop (12);

    // The two rows of small knobs keep their size; depth takes what is left.
    const auto small = jlimit (104, 128, (column.getHeight() - 170) / 2);

    auto lower = column.removeFromBottom (small);
    attack.setBounds (lower.removeFromLeft (lower.getWidth() / 2));
    release.setBounds (lower);
    column.removeFromBottom (18);

    auto upper = column.removeFromBottom (small);
    detail.setBounds (upper.removeFromLeft (upper.getWidth() / 2));
    tilt.setBounds (upper);
    column.removeFromBottom (10);

    depth.setBounds (column.withSizeKeepingCentre (jmin (column.getWidth(), 164), jmin (column.getHeight(), 186)));

    auto foot = bounds.removeFromBottom (kFootHeight).reduced (14, 0).withTrimmedBottom (12);

    auto button = [&foot] (Component& c, int width, bool fromRight = false)
    {
        const auto area = fromRight ? foot.removeFromRight (width) : foot.removeFromLeft (width);
        c.setBounds (area.withSizeKeepingCentre (width, 28));
    };

    // From the right: how it is computed.
    button (lowLatencyButton, 124, true);
    foot.removeFromRight (8);
    button (qualityBox, 118, true);

    // From the left: what comes out, then how the two channels are treated,
    // then where it listens.
    mix.setBounds (foot.removeFromLeft (92));
    foot.removeFromLeft (12);
    output.setBounds (foot.removeFromLeft (100));
    foot.removeFromLeft (8);
    button (autoGainButton, 100);
    foot.removeFromLeft (22);
    button (stereoBox, 120);
    foot.removeFromLeft (12);
    link.setBounds (foot.removeFromLeft (94));
    foot.removeFromLeft (12);
    balance.setBounds (foot.removeFromLeft (90));
    foot.removeFromLeft (22);
    button (sidechainButton, 110);

    display.setBounds (bounds.reduced (14, 12));
}

} // namespace onirique
