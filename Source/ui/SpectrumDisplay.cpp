#include "SpectrumDisplay.h"

namespace onirique
{

using namespace theme;

namespace
{
    constexpr float kMinHz = 20.0f;
    constexpr float kMaxHz = 20000.0f;
    constexpr float kSensitivitySpan = 18.0f;   // dB from the middle line to either edge
    constexpr float kSpectrumTop = 6.0f;
    constexpr float kSpectrumBottom = -84.0f;
    constexpr float kTiltPerOctave = 4.5f;      // so that music reads level, not falling
    constexpr float kSmoothingOctaves = 1.0f / 3.0f;
    constexpr float kNodeRadius = 6.5f;

    const float kScales[] = { 6.0f, 9.0f, 12.0f, 18.0f, 24.0f };

    const Colour kNodeColours[kNumNodes] = { Colour (0xffffb454), Colour (0xffff5f9e), Colour (0xffa98bff),
                                             Colour (0xff58d5e3), Colour (0xffb6e35a), Colour (0xff6aa6ff),
                                             Colour (0xffff9248), Colour (0xffe77bff),
                                             Colour (0xffff7a66), Colour (0xff7fe0a8) };

    /** A curve through every point, with no corner at any of them. */
    Path curveThrough (const std::vector<Point<float>>& points)
    {
        Path path;

        if (points.size() < 2)
            return path;

        path.startNewSubPath (points.front());

        const auto last = (int) points.size() - 1;

        for (int i = 0; i < last; ++i)
        {
            const auto before = points[(size_t) jmax (0, i - 1)];
            const auto from = points[(size_t) i];
            const auto to = points[(size_t) i + 1];
            const auto after = points[(size_t) jmin (last, i + 2)];

            path.cubicTo (from + (to - before) / 6.0f, to - (after - from) / 6.0f, to);
        }

        return path;
    }
}

Colour SpectrumDisplay::nodeColour (int node)
{
    return isPositiveAndBelow (node, kNumNodes) ? kNodeColours[node] : ember;
}

String SpectrumDisplay::nodeName (int node)
{
    if (node == kLowCutNode)
        return "LOW CUT";

    if (node == kHighCutNode)
        return "HIGH CUT";

    return "BAND " + String (node + 1);
}

//==============================================================================
/** The small drawn buttons of the island. */
class IconButton final : public Button
{
public:
    enum class Icon { power, close, previous, next };

    explicit IconButton (Icon i) : Button ({}), icon (i) {}

    Colour accent { ember };

    void paintButton (Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto lit = getToggleState() || down;

        auto colour = lit ? accent.brighter (0.2f) : (over ? Colours::white : Colour (0xffcdb6ba));
        colour = colour.withMultipliedAlpha (isEnabled() ? 1.0f : 0.3f);

        const PathStrokeType stroke (1.6f, PathStrokeType::curved, PathStrokeType::rounded);
        Path p;

        switch (icon)
        {
            case Icon::power:
            {
                // The one button with a rim, as the main switch of the band.
                g.setColour (Colour (0x59000000));
                g.fillEllipse (bounds.reduced (1.5f));
                g.setColour (colour.withMultipliedAlpha (0.55f));
                g.drawEllipse (bounds.reduced (1.5f), 1.0f);

                const auto a = r * 0.36f;
                p.addCentredArc (centre.x, centre.y + a * 0.1f, a, a, 0.0f, 0.75f, MathConstants<float>::twoPi - 0.75f, true);
                p.startNewSubPath (centre.x, centre.y - a * 1.3f);
                p.lineTo (centre.x, centre.y - a * 0.15f);
                break;
            }

            case Icon::close:
            {
                const auto a = r * 0.36f;
                p.startNewSubPath (centre.x - a, centre.y - a);
                p.lineTo (centre.x + a, centre.y + a);
                p.startNewSubPath (centre.x + a, centre.y - a);
                p.lineTo (centre.x - a, centre.y + a);
                break;
            }

            case Icon::previous:
            case Icon::next:
            {
                const auto a = r * 0.34f * (icon == Icon::next ? 1.0f : -1.0f);
                p.startNewSubPath (centre.x - a * 0.5f, centre.y - std::abs (a));
                p.lineTo (centre.x + a * 0.5f, centre.y);
                p.lineTo (centre.x - a * 0.5f, centre.y + std::abs (a));
                break;
            }

        }

        g.setColour (colour);
        g.strokePath (p, stroke);
    }

private:
    Icon icon;
};

/** The island: the controls of what is selected, floating over the display.

    For one node it carries that node's values. For several it stays, and
    moves them together: gain by the same number of dB, Q by the same ratio,
    shape, placement and bypass alike. Frequency is the one thing a group of
    nodes does not share, so its dial stands down. */
class SpectrumDisplay::NodePanel final : public Component
{
public:
    explicit NodePanel (AudioProcessorValueTreeState& s) : state (s)
    {
        for (auto* b : { &power, &close, &previous, &next })
            addAndMakeVisible (*b);

        power.onClick = [this]
        {
            if (primary < 0)
                return;

            const auto on = real (nodeIds (primary).on) > 0.5f ? 0.0f : 1.0f;

            for (const auto node : nodes)
                change (nodeIds (node).on, on);
        };

        close.onClick = [this] { if (onDelete != nullptr) onDelete(); };
        previous.onClick = [this] { if (onStep != nullptr) onStep (-1); };
        next.onClick = [this] { if (onStep != nullptr) onStep (1); };

        shapeBox.addSectionHeading ("SHAPE");
        shapeBox.addItemList ({ "BELL", "LOW SHELF", "HIGH SHELF", "PLATEAU" }, 1);
        shapeBox.onChange = [this]
        {
            for (const auto node : bands)
                change (nodeIds (node).shape, (float) (shapeBox.getSelectedId() - 1));
        };
        addAndMakeVisible (shapeBox);

        placeBox.addSectionHeading ("PLACEMENT");
        placeBox.addItemList ({ "STEREO", "LEFT", "RIGHT", "MID", "SIDE" }, 1);
        placeBox.onChange = [this]
        {
            for (const auto node : bands)
                change (nodeIds (node).place, (float) (placeBox.getSelectedId() - 1));
        };
        addAndMakeVisible (placeBox);

        caption.setJustificationType (Justification::centredLeft);
        caption.setColour (Label::textColourId, haze);
        caption.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (caption);

        number.setJustificationType (Justification::centred);
        number.setColour (Label::textColourId, mist);
        number.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (number);

        controls[0].knob = &freq;
        controls[1].knob = &gain;
        controls[2].knob = &q;

        for (auto& control : controls)
        {
            auto* c = &control;

            c->knob->setCompact (true);
            addAndMakeVisible (*c->knob);

            auto& slider = c->knob->slider;
            slider.onDragStart = [this, c] { c->dragging = true; begin (*c); };
            slider.onDragEnd = [this, c] { end (*c); c->dragging = false; };
            slider.onValueChange = [this, c]
            {
                // A wheel, a typed value or a double click is a whole gesture
                // of its own.
                if (c->dragging)
                {
                    apply (*c);
                }
                else
                {
                    begin (*c);
                    apply (*c);
                    end (*c);
                }

                c->knob->updateText();
            };
        }

        freq.setDialInset (10);
        q.setDialInset (10);

        setSize (604, 126);
    }

    /** What the island now stands for. primary is the node it shows. */
    void setNodes (const std::vector<int>& selection, int primaryNode)
    {
        nodes = selection;
        primary = primaryNode;
        bands.clear();

        for (const auto node : nodes)
            if (node < rb::kNumBands)
                bands.push_back (node);

        const auto several = nodes.size() > 1;

        // Gain, Q, shape and placement belong to bells: with a cut clicked
        // last, they read from the first bell there is.
        const auto bandLead = primary < rb::kNumBands ? primary : (bands.empty() ? -1 : bands.front());

        controls[0].nodes = several ? std::vector<int>() : nodes;
        controls[0].lead = several ? -1 : primary;
        controls[1].nodes = controls[2].nodes = bands;
        controls[1].lead = controls[2].lead = bandLead;

        const auto colour = nodeColour (primary);
        accent = colour;

        for (auto* b : { &power, &close, &previous, &next })
            b->accent = colour;

        caption.setText (several ? "(multiple)" : nodeName (primary).toLowerCase(), dontSendNotification);
        number.setText (several ? String ("#") : primary < rb::kNumBands ? String (primary + 1)
                                                                         : String (primary == kLowCutNode ? "LC" : "HC"),
                        dontSendNotification);

        for (int i = 0; i < 3; ++i)
        {
            auto& c = controls[i];
            const auto usable = c.lead >= 0;

            // The dial shows the last node it stood for, greyed, when it has
            // nothing to act on.
            const auto shown = usable ? c.lead : (i == 0 ? primary : -1);

            if (shown >= 0)
                if (auto* p = state.getParameter (idFor (shown, i)))
                    c.knob->speak (*p, i == 1);

            c.knob->setEnabled (usable);
            c.knob->setAlpha (usable ? 1.0f : 0.3f);
            c.knob->setAccent (colour);
        }

        const auto hasBand = bandLead >= 0;

        for (auto* c : std::initializer_list<Component*> { &shapeBox, &placeBox, &close })
        {
            c->setEnabled (hasBand);
            c->setAlpha (hasBand ? 1.0f : 0.3f);
        }

        refresh (true);
        repaint();
    }

    /** Follows the parameters, which automation, a preset or a drag on the
        curve may have moved. */
    void refresh (bool force = false)
    {
        if (primary < 0)
            return;

        power.setToggleState (real (nodeIds (primary).on) > 0.5f, dontSendNotification);

        if (controls[1].lead >= 0)
        {
            const auto ids = nodeIds (controls[1].lead);

            // Only when the parameter itself has moved: a choice just made in
            // the menu must not be put back before it has been acted on.
            const auto shape = roundToInt (real (ids.shape));
            const auto place = roundToInt (real (ids.place));

            if (force || shape != shownShape)
            {
                shownShape = shape;
                shapeBox.setSelectedId (shape + 1, dontSendNotification);
            }

            if (force || place != shownPlace)
            {
                shownPlace = place;
                placeBox.setSelectedId (place + 1, dontSendNotification);
            }
        }

        for (int i = 0; i < 3; ++i)
        {
            auto& c = controls[i];
            const auto shown = c.lead >= 0 ? c.lead : (i == 0 ? primary : -1);

            if (shown < 0 || c.dragging)
                continue;

            if (auto* p = state.getParameter (idFor (shown, i)))
            {
                if (force || std::abs ((double) p->getValue() - c.knob->slider.getValue()) > 1.0e-6)
                {
                    c.knob->slider.setValue (p->getValue(), dontSendNotification);
                    c.knob->updateText();
                }
            }
        }
    }

    void paint (Graphics& g) override
    {
        // Smoked glass: what is under the island still shows through it.
        g.setGradientFill (ColourGradient (Colour (0xb33a1820), 0.0f, body.getY(),
                                           Colour (0xb3170709), 0.0f, body.getBottom(), false));
        g.fillRoundedRectangle (body, 26.0f);

        // A sheen along the top edge, as on glass lit from above.
        g.setGradientFill (ColourGradient (Colour (0x1effffff), 0.0f, body.getY(),
                                           Colours::transparentWhite, 0.0f, body.getY() + 34.0f, false));
        g.fillRoundedRectangle (body, 26.0f);

        g.setColour (accent.withAlpha (0.14f));
        g.drawRoundedRectangle (body.expanded (1.0f), 27.0f, 3.0f);
        g.setColour (accent.withAlpha (0.7f));
        g.drawRoundedRectangle (body, 26.0f, 1.0f);
    }

    void resized() override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (3.0f);

        body = bounds;

        auto area = body.toNearestInt().reduced (16, 10);

        auto left = area.removeFromLeft (128);
        power.setBounds (left.removeFromTop (28).removeFromLeft (28));
        left.removeFromTop (14);
        shapeBox.setBounds (left.removeFromTop (28));
        left.removeFromTop (3);
        caption.setBounds (left.removeFromTop (16).withTrimmedLeft (12));

        auto right = area.removeFromRight (128);
        auto top = right.removeFromTop (28);
        close.setBounds (top.removeFromRight (28));
        top.removeFromRight (10);
        next.setBounds (top.removeFromRight (22));
        number.setBounds (top.removeFromRight (26));
        previous.setBounds (top.removeFromRight (22));
        right.removeFromTop (14);
        placeBox.setBounds (right.removeFromTop (28));

        area.reduce (10, 0);

        const auto side = (area.getWidth() - 128) / 2;
        freq.setBounds (area.removeFromLeft (side));
        q.setBounds (area.removeFromRight (side));
        gain.setBounds (area);
    }

    // Only the island itself takes the mouse, not the corners of its box.
    bool hitTest (int x, int y) override
    {
        const Point<float> p ((float) x, (float) y);
        return body.contains (p);
    }

    // Clicks on the island's ground are the island's, not the curve's under it.
    void mouseDown (const MouseEvent&) override {}
    void mouseDoubleClick (const MouseEvent&) override {}

    /** The description of whichever of the island's controls is at a point. */
    String descriptionAt (Point<int> p) const
    {
        const auto several = nodes.size() > 1;

        const std::pair<const Component*, String> parts[] = {
            { &power, fr (several ? "Active ou coupe les points sélectionnés. Un point coupé garde sa place et ses réglages."
                                  : "Active ou coupe ce point. Coupé, il garde sa place et ses réglages.") },
            { &shapeBox, fr ("Forme de la bande : une cloche, un plateau vers le grave (low shelf) ou vers l'aigu (high shelf), ou un plateau - une cloche au sommet plat et aux flancs raides.") },
            { &placeBox, fr ("Où la bande agit : toute l'image stéréo, la gauche, la droite, le centre (mid) ou les côtés (side). Sous une bande placée, le link se relâche.") },
            { &previous, fr ("Passe au point précédent de la courbe.") },
            { &next, fr ("Passe au point suivant de la courbe.") },
            { &number, fr (several ? "Plusieurs points sont sélectionnés." : "Le numéro du point sélectionné.") },
            { &close, fr (several ? "Supprime les bandes sélectionnées. Les deux coupes ne se suppriment pas, elles se désactivent seulement."
                                  : "Supprime cette bande. Les deux coupes ne se suppriment pas, elles se désactivent seulement.") },
            { &freq, fr (several ? "Fréquence. Grisée quand plusieurs points sont sélectionnés : ils n'ont pas de fréquence commune."
                                 : "Fréquence du point. Double-cliquez sur le nom pour taper une valeur.") },
            { &gain, fr ("Gain : de combien le suppresseur travaille plus fort (+) ou plus doucement (-) sous cette bande. Avec plusieurs points, tous bougent du même nombre de dB.") },
            { &q, fr ("Q : l'étroitesse de la bande. Avec plusieurs points, tous changent dans le même rapport.") },
            { &caption, fr (several ? "Plusieurs points sont sélectionnés : les contrôles agissent sur tous." : "Le point sélectionné.") },
        };

        for (const auto& [component, text] : parts)
            if (component->isVisible() && component->getBounds().contains (p))
                return text;

        return {};
    }

    std::function<void()> onDelete;
    std::function<void (int)> onStep;

private:
    struct Control
    {
        Knob* knob = nullptr;
        std::vector<int> nodes;     // what the dial moves
        int lead = -1;              // the one of them it shows
        std::vector<float> starts;
        float leadStart = 0.0f;
        bool dragging = false;
    };

    static String idFor (int node, int control)
    {
        const auto ids = nodeIds (node);
        return control == 0 ? ids.freq : control == 1 ? ids.gain : ids.q;
    }

    int indexOf (const Control& c) const { return (int) (&c - controls); }

    float real (const String& id) const { return state.getRawParameterValue (id)->load(); }

    void set (const String& id, float value)
    {
        if (auto* p = state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (value)));
    }

    void change (const String& id, float value)
    {
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            set (id, value);
            p->endChangeGesture();
        }
    }

    void begin (Control& c)
    {
        if (c.lead < 0)
            return;

        const auto which = indexOf (c);

        c.starts.clear();
        c.leadStart = real (idFor (c.lead, which));

        for (const auto node : c.nodes)
        {
            c.starts.push_back (real (idFor (node, which)));

            if (auto* p = state.getParameter (idFor (node, which)))
                p->beginChangeGesture();
        }
    }

    void apply (Control& c)
    {
        if (c.lead < 0 || c.starts.size() != c.nodes.size())
            return;

        const auto which = indexOf (c);
        auto* leadParameter = state.getParameter (idFor (c.lead, which));

        if (leadParameter == nullptr)
            return;

        const auto value = leadParameter->convertFrom0to1 ((float) c.knob->slider.getValue());

        for (size_t i = 0; i < c.nodes.size(); ++i)
        {
            // Gain moves by the same number of dB for every node, frequency
            // and Q by the same ratio.
            const auto wanted = which == 1 ? c.starts[i] + (value - c.leadStart)
                                           : c.starts[i] * value / jmax (1.0e-6f, c.leadStart);

            set (idFor (c.nodes[i], which), wanted);
        }
    }

    void end (Control& c)
    {
        if (c.lead < 0)
            return;

        for (const auto node : c.nodes)
            if (auto* p = state.getParameter (idFor (node, indexOf (c))))
                p->endChangeGesture();
    }

    AudioProcessorValueTreeState& state;
    Colour accent { ember };
    Rectangle<float> body;

    std::vector<int> nodes, bands;
    int primary = -1, shownShape = -1, shownPlace = -1;

    using Icon = IconButton::Icon;
    IconButton power { Icon::power }, close { Icon::close }, previous { Icon::previous }, next { Icon::next };
    ComboBox shapeBox, placeBox;
    Label caption, number;
    Knob freq { "FREQ" }, gain { "GAIN" }, q { "Q" };
    Control controls[3];
};

//==============================================================================
SpectrumDisplay::SpectrumDisplay (ResoBoxProcessor& p)
    : processor (p)
{
    panel = std::make_unique<NodePanel> (processor.apvts);
    panel->onDelete = [this] { removeSelection(); };
    panel->onStep = [this] (int direction) { stepSelection (direction); };
    addChildComponent (*panel);

    // For the Delete key.
    setWantsKeyboardFocus (true);

    startTimerHz (30);
}

SpectrumDisplay::~SpectrumDisplay() = default;

void SpectrumDisplay::resized()
{
    plot = getLocalBounds().toFloat().withTrimmedLeft (40.0f).withTrimmedRight (42.0f)
                           .withTrimmedTop (14.0f).withTrimmedBottom (26.0f);
    placePanel();
}

void SpectrumDisplay::timerCallback()
{
    if (processor.getVisual (fetchedIn, fetchedOut, fetchedRed, binHz))
    {
        const auto bins = fetchedIn.size();

        if (shownIn.size() != bins)
        {
            shownIn.assign (bins, -140.0f);
            shownOut.assign (bins, -140.0f);
            shownRed.assign (bins, 0.0f);
            cumulativeIn.assign (bins + 1, 0.0);
            cumulativeOut.assign (bins + 1, 0.0);
        }

        deepest = 0.0f;

        for (size_t k = 0; k < bins; ++k)
        {
            // Up at once, down at a pace the eye can follow.
            shownIn[k] = jmax (fetchedIn[k], shownIn[k] - 1.4f);
            shownOut[k] = jmax (fetchedOut[k], shownOut[k] - 1.4f);
            shownRed[k] += 0.6f * (fetchedRed[k] - shownRed[k]);
            deepest = jmax (deepest, shownRed[k]);

            // Running sums of power, from which any stretch of the spectrum
            // can be averaged in one step when it is drawn.
            cumulativeIn[k + 1] = cumulativeIn[k] + std::pow (10.0, 0.1 * (double) shownIn[k]);
            cumulativeOut[k + 1] = cumulativeOut[k] + std::pow (10.0, 0.1 * (double) shownOut[k]);
        }
    }

    // A node removed by a preset or by the other slot leaves the selection.
    bool gone = false;

    for (int i = 0; i < kNumNodes; ++i)
    {
        if (inSelection[(size_t) i] && ! isUsed (i))
        {
            inSelection[(size_t) i] = false;
            gone = true;
        }
    }

    if (gone)
        selectionChanged();

    if (panel->isVisible())
        panel->refresh();

    repaint();
}

//==============================================================================
float SpectrumDisplay::xForHz (float hz) const
{
    return plot.getX() + plot.getWidth() * std::log (jmax (1.0f, hz) / kMinHz) / std::log (kMaxHz / kMinHz);
}

float SpectrumDisplay::hzForX (float x) const
{
    return kMinHz * std::pow (kMaxHz / kMinHz, (x - plot.getX()) / jmax (1.0f, plot.getWidth()));
}

float SpectrumDisplay::yForSensitivity (float db) const
{
    return plot.getCentreY() - db / kSensitivitySpan * plot.getHeight() * 0.5f;
}

float SpectrumDisplay::sensitivityForY (float y) const
{
    return (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * kSensitivitySpan;
}

float SpectrumDisplay::reductionSpan() const
{
    return kScales[jlimit (0, (int) numElementsInArray (kScales) - 1, roundToInt (parameter ("scale")))];
}

bool SpectrumDisplay::isUsed (int node) const
{
    if (! isPositiveAndBelow (node, kNumNodes))
        return false;

    return node >= rb::kNumBands || parameter (nodeIds (node).used) > 0.5f;
}

float SpectrumDisplay::nodeHeight (int node) const
{
    if (! isPositiveAndBelow (node, (int) rb::kNumBands))
        return 1.0f;

    const auto shape = (rb::BandShape) roundToInt (parameter (nodeIds (node).shape));
    return shape == rb::BandShape::lowShelf || shape == rb::BandShape::highShelf ? 0.5f : 1.0f;
}

Point<float> SpectrumDisplay::nodePosition (int node, const rb::Settings& s) const
{
    if (node == kLowCutNode)
        return { xForHz (s.lowCutHz), yForSensitivity (0.0f) };

    if (node == kHighCutNode)
        return { xForHz (s.highCutHz), yForSensitivity (0.0f) };

    // On its own curve, wherever the others take the sum.
    return { xForHz (s.bands[node].freq), yForSensitivity (s.bands[node].gainDb * nodeHeight (node)) };
}

int SpectrumDisplay::nodeAt (Point<float> p) const
{
    const auto settings = processor.readSettings();
    int best = -1;
    float bestDistance = 15.0f;

    for (int i = 0; i < kNumNodes; ++i)
    {
        if (! isUsed (i))
            continue;

        // The selected node wins a tie, so that it can be pulled out from
        // under another one.
        const auto distance = nodePosition (i, settings).getDistanceFrom (p) - (isSelected (i) ? 3.0f : 0.0f);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

float SpectrumDisplay::parameter (const String& id) const
{
    return processor.apvts.getRawParameterValue (id)->load();
}

void SpectrumDisplay::setParameter (const String& id, float value)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void SpectrumDisplay::gesture (const String& id, bool begin)
{
    if (auto* p = processor.apvts.getParameter (id))
    {
        if (begin)
            p->beginChangeGesture();
        else
            p->endChangeGesture();
    }
}

void SpectrumDisplay::changeParameter (const String& id, float value)
{
    gesture (id, true);
    setParameter (id, value);
    gesture (id, false);
}

//==============================================================================
String SpectrumDisplay::descriptionAt (Point<int> position) const
{
    const auto p = position.toFloat();

    // The island lies over everything else.
    if (panel->isVisible() && panel->getBounds().contains (position))
        return panel->descriptionAt (position - panel->getPosition());

    if (scaleArea().contains (p))
        return fr ("Échelle de réduction : le nombre de dB que représente la hauteur de l'écran, sur l'axe de droite. Cliquez pour la changer.");

    if (Rectangle<float> (104.0f, 22.0f).withPosition (plot.getRight() - 114.0f, plot.getY() + 10.0f).contains (p))
        return fr ("GR : la coupe la plus profonde en ce moment, où qu'elle soit dans le spectre.");

    const auto node = nodeAt (p);

    if (node == kLowCutNode)
        return fr ("Low cut : rien n'est traité en dessous. Glissez pour le déplacer, double-cliquez pour le désactiver.");

    if (node == kHighCutNode)
        return fr ("High cut : rien n'est traité au-dessus. Glissez pour le déplacer, double-cliquez pour le désactiver.");

    if (node >= 0)
        return fr ("Bande ") + String (node + 1)
               + fr (" : glissez pour la fréquence et le gain, molette pour le Q, double-clic pour la bypasser, clic droit pour la remettre à zéro, Suppr pour la retirer. "
                     "Ctrl ou Maj + clic en sélectionne plusieurs.");

    if (plot.contains (p))
        return fr ("Corps gris : le spectre qui entre. Trait clair : celui qui sort. Rouge qui pend du haut : ce qui est retiré. "
                   "Courbe blanche : la sensibilité. Double-cliquez pour ajouter une bande ; tracez un cadre pour en sélectionner plusieurs.");

    if (p.x < plot.getX() && p.y >= plot.getY() && p.y <= plot.getBottom())
        return fr ("Sensibilité, en dB : l'échelle de la courbe blanche. Au-dessus de 0 le suppresseur travaille plus fort à cet endroit, en dessous il épargne.");

    if (p.x > plot.getRight() && p.y >= plot.getY() && p.y <= plot.getBottom())
        return fr ("Réduction, en dB : l'échelle de la zone rouge qui pend du haut.");

    return {};
}

void SpectrumDisplay::select (int node)
{
    inSelection.fill (false);

    if (isUsed (node))
    {
        inSelection[(size_t) node] = true;
        selected = node;
    }

    selectionChanged();
}

void SpectrumDisplay::setSelected (int node, bool shouldBeSelected)
{
    if (isUsed (node))
    {
        inSelection[(size_t) node] = shouldBeSelected;

        if (shouldBeSelected)
            selected = node;

        selectionChanged();
    }
}

void SpectrumDisplay::selectionChanged()
{
    std::vector<int> nodes;

    for (int i = 0; i < kNumNodes; ++i)
        if (inSelection[(size_t) i])
            nodes.push_back (i);

    // The island reads from the node clicked last, as long as it is still
    // one of those selected.
    if (nodes.empty())
        selected = -1;
    else if (! isSelected (selected))
        selected = nodes.front();

    if (nodes != shownNodes || selected != shownPrimary)
    {
        shownNodes = nodes;
        shownPrimary = selected;

        if (! nodes.empty())
            panel->setNodes (nodes, selected);
    }

    panel->setVisible (! nodes.empty());
    placePanel();
    repaint();
}

std::vector<int> SpectrumDisplay::withSelection (int node) const
{
    if (! isSelected (node))
        return { node };

    std::vector<int> nodes;

    for (int i = 0; i < kNumNodes; ++i)
        if (inSelection[(size_t) i])
            nodes.push_back (i);

    return nodes;
}

void SpectrumDisplay::placePanel()
{
    if (selected < 0 || plot.isEmpty())
        return;

    // At the foot of the display, unless that is where the node is.
    const auto node = nodePosition (selected, processor.readSettings());
    const auto atTop = node.y > plot.getCentreY() + plot.getHeight() * 0.12f;
    const auto y = atTop ? plot.getY() + 10.0f : plot.getBottom() - (float) panel->getHeight() - 10.0f;

    // In the middle, but clear of the readout in the corner on its left.
    const auto centred = plot.getCentreX() - (float) panel->getWidth() * 0.5f;
    const auto x = jmin (jmax (centred, plot.getX() + 236.0f), jmax (centred, plot.getRight() - (float) panel->getWidth() - 66.0f));

    panel->setTopLeftPosition (roundToInt (x), roundToInt (y));
}

void SpectrumDisplay::addBand (Point<float> at)
{
    for (int i = 0; i < rb::kNumBands; ++i)
    {
        if (isUsed (i))
            continue;

        const auto ids = nodeIds (i);

        changeParameter (ids.freq, jlimit (kMinHz, kMaxHz, hzForX (at.x)));
        changeParameter (ids.gain, jlimit (-12.0f, 12.0f, sensitivityForY (at.y)));
        changeParameter (ids.q, 1.0f);
        changeParameter (ids.shape, 0.0f);
        changeParameter (ids.place, 0.0f);
        changeParameter (ids.on, 1.0f);
        changeParameter (ids.used, 1.0f);

        select (i);
        return;
    }
}

void SpectrumDisplay::removeNode (int node)
{
    if (! isPositiveAndBelow (node, (int) rb::kNumBands))
        return;

    changeParameter (nodeIds (node).used, 0.0f);
    inSelection[(size_t) node] = false;
    selectionChanged();
}

void SpectrumDisplay::removeSelection()
{
    // The cuts are part of the display: they are bypassed, never removed.
    for (int i = 0; i < rb::kNumBands; ++i)
    {
        if (inSelection[(size_t) i])
        {
            changeParameter (nodeIds (i).used, 0.0f);
            inSelection[(size_t) i] = false;
        }
    }

    selectionChanged();
}

void SpectrumDisplay::stepSelection (int direction)
{
    // Every node on the curve, in the order the eye meets them.
    const auto settings = processor.readSettings();
    std::vector<std::pair<float, int>> order;

    for (int i = 0; i < kNumNodes; ++i)
        if (isUsed (i))
            order.push_back ({ nodePosition (i, settings).x, i });

    if (order.empty())
        return;

    std::sort (order.begin(), order.end());

    const auto count = (int) order.size();
    int at = 0;

    for (int i = 0; i < count; ++i)
        if (order[(size_t) i].second == selected)
            at = i;

    select (order[(size_t) ((at + direction + count) % count)].second);
}

bool SpectrumDisplay::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::deleteKey || key == KeyPress::backspaceKey)
    {
        removeSelection();
        return true;
    }

    if (key == KeyPress::escapeKey && std::find (inSelection.begin(), inSelection.end(), true) != inSelection.end())
    {
        select (-1);
        return true;
    }

    return false;
}

Rectangle<float> SpectrumDisplay::scaleArea() const
{
    return Rectangle<float> (50.0f, 20.0f).withPosition (plot.getRight() - 58.0f, plot.getBottom() - 28.0f);
}

void SpectrumDisplay::showScaleMenu()
{
    // The same card as the preset menu: without this a menu opened by hand
    // is drawn with the stock look, not the plugin's.
    PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    menu.addSectionHeader ("REDUCTION SCALE");

    const auto current = roundToInt (parameter ("scale"));

    for (int i = 0; i < (int) numElementsInArray (kScales); ++i)
        menu.addItem (i + 1, String (roundToInt (kScales[i])) + " dB", true, i == current);

    const auto area = localAreaToGlobal (scaleArea().toNearestInt());

    menu.showMenuAsync (PopupMenu::Options().withTargetScreenArea (area).withMinimumWidth (150),
                        [safe = SafePointer<SpectrumDisplay> (this)] (int chosen)
                        {
                            if (safe != nullptr && chosen > 0)
                                safe->changeParameter ("scale", (float) (chosen - 1));
                        });
}

void SpectrumDisplay::mouseMove (const MouseEvent& e)
{
    const auto now = nodeAt (e.position);

    cursor = e.position;
    cursorInside = plot.contains (cursor);

    if (now != hovered)
    {
        hovered = now;
        setMouseCursor (hovered >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }

    repaint();
}

void SpectrumDisplay::mouseExit (const MouseEvent&)
{
    hovered = -1;
    cursorInside = false;
    repaint();
}

void SpectrumDisplay::mouseDown (const MouseEvent& e)
{
    grabKeyboardFocus();

    const auto node = nodeAt (e.position);

    if (node < 0 && scaleArea().contains (e.position))
    {
        showScaleMenu();
        return;
    }

    // A right click puts a band back to doing nothing.
    if (e.mods.isPopupMenu())
    {
        for (const auto i : withSelection (node))
        {
            if (isPositiveAndBelow (i, (int) rb::kNumBands))
            {
                changeParameter (nodeIds (i).gain, 0.0f);
                changeParameter (nodeIds (i).q, 1.0f);
            }
        }

        return;
    }

    const auto adding = e.mods.isCtrlDown() || e.mods.isShiftDown() || e.mods.isCommandDown();
    dragOrigin = e.position;

    // In the empty: the start of a frame, which adds to the selection with
    // Ctrl or Shift held and replaces it without.
    if (node < 0)
    {
        if (! adding)
            inSelection.fill (false);

        frameBase = inSelection;
        frame = { e.position, e.position };
        framing = true;
        selectionChanged();
        return;
    }

    if (adding)
    {
        inSelection[(size_t) node] = ! inSelection[(size_t) node];

        if (inSelection[(size_t) node])
            selected = node;

        selectionChanged();

        if (! inSelection[(size_t) node])
            return;
    }
    else if (! inSelection[(size_t) node])
    {
        // A click on a node that is not selected selects it alone; on one
        // that is, the selection stays, to be dragged as a whole.
        select (node);
    }
    else
    {
        selected = node;
        selectionChanged();
    }

    dragging = node;

    for (const auto i : withSelection (node))
    {
        const auto ids = nodeIds (i);

        dragStart[(size_t) i] = { parameter (ids.freq), ids.gain.isNotEmpty() ? parameter (ids.gain) : 0.0f };
        gesture (ids.freq, true);

        if (ids.gain.isNotEmpty())
            gesture (ids.gain, true);
    }
}

void SpectrumDisplay::mouseDrag (const MouseEvent& e)
{
    cursor = e.position;
    cursorInside = plot.contains (cursor);

    if (framing)
    {
        frame = { dragOrigin, e.position };

        const auto settings = processor.readSettings();

        for (int i = 0; i < kNumNodes; ++i)
            inSelection[(size_t) i] = frameBase[(size_t) i]
                                      || (isUsed (i) && frame.contains (nodePosition (i, settings)));

        selectionChanged();
        return;
    }

    if (dragging < 0)
        return;

    const auto interval = hzForX (e.position.x) / hzForX (dragOrigin.x);
    const auto lift = sensitivityForY (e.position.y) - sensitivityForY (dragOrigin.y);

    for (const auto i : withSelection (dragging))
    {
        const auto ids = nodeIds (i);

        setParameter (ids.freq, jlimit (kMinHz, kMaxHz, dragStart[(size_t) i].freq * interval));

        if (ids.gain.isNotEmpty())
            setParameter (ids.gain, jlimit (-12.0f, 12.0f, dragStart[(size_t) i].gain + lift / nodeHeight (i)));
    }

    repaint();
}

void SpectrumDisplay::mouseUp (const MouseEvent&)
{
    if (framing)
    {
        framing = false;
        repaint();
        return;
    }

    if (dragging < 0)
        return;

    for (const auto i : withSelection (dragging))
    {
        const auto ids = nodeIds (i);

        gesture (ids.freq, false);

        if (ids.gain.isNotEmpty())
            gesture (ids.gain, false);
    }

    dragging = -1;
    placePanel();
}

void SpectrumDisplay::mouseDoubleClick (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const auto node = nodeAt (e.position);

    // On a node: bypass it, or bring it back, and the rest of the selection
    // with it. In the empty: a new bell there.
    if (node >= 0)
    {
        const auto on = parameter (nodeIds (node).on) > 0.5f ? 0.0f : 1.0f;

        for (const auto i : withSelection (node))
            changeParameter (nodeIds (i).on, on);
    }
    else if (plot.contains (e.position) && ! scaleArea().contains (e.position))
    {
        addBand (e.position);
    }
}

void SpectrumDisplay::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& wheel)
{
    auto node = nodeAt (e.position);

    if (node < 0)
        node = selected;

    if (node < 0)
        return;

    for (const auto i : withSelection (node))
    {
        if (isPositiveAndBelow (i, (int) rb::kNumBands))
        {
            const auto id = nodeIds (i).q;
            changeParameter (id, jlimit (0.3f, 8.0f, parameter (id) * std::pow (2.0f, wheel.deltaY * 1.5f)));
        }
    }
}

//==============================================================================
Path SpectrumDisplay::spectrumPath (const std::vector<double>& cumulative, bool closed) const
{
    if (cumulative.size() < 3 || binHz <= 0.0)
        return {};

    const auto bins = (int) cumulative.size() - 1;

    // The sum of everything below a position between bins, each bin being
    // taken to reach half way to its neighbours.
    auto upTo = [&cumulative, bins] (float position)
    {
        const auto at = jlimit (0.0f, (float) bins, position + 0.5f);
        const auto k = jmin (bins - 1, (int) at);
        return cumulative[(size_t) k] + (double) (at - (float) k) * (cumulative[(size_t) k + 1] - cumulative[(size_t) k]);
    };

    const float step = 6.0f;
    const float halfWindow = std::pow (2.0f, 0.5f * kSmoothingOctaves);
    std::vector<Point<float>> points;

    for (float x = plot.getX() - step; x <= plot.getRight() + 2.0f * step; x += step)
    {
        const auto hz = hzForX (x);
        const auto centre = hz / (float) binHz;

        // Never fewer than a bin and a half, or the lowest octaves would be
        // drawn as the staircase they are.
        const auto from = jmin (centre / halfWindow, centre - 0.75f);
        const auto to = jmax (centre * halfWindow, centre + 0.75f);

        const auto power = (upTo (to) - upTo (from)) / (double) (to - from);
        const auto level = 10.0f * (float) std::log10 (power + 1.0e-14)
                           + kTiltPerOctave * std::log2 (jmax (kMinHz, hz) / 1000.0f);

        points.push_back ({ x, jmap (jlimit (kSpectrumBottom, kSpectrumTop, level), kSpectrumBottom, kSpectrumTop,
                                     plot.getBottom(), plot.getY()) });
    }

    // Averaging rounds the spectrum; this rounds the line drawn of it.
    for (int pass = 0; pass < 2; ++pass)
    {
        auto previous = points.front().y;

        for (size_t i = 1; i + 1 < points.size(); ++i)
        {
            const auto here = points[i].y;
            points[i].y = 0.25f * previous + 0.5f * here + 0.25f * points[i + 1].y;
            previous = here;
        }
    }

    auto path = curveThrough (points);

    if (closed)
    {
        path.lineTo (points.back().x, plot.getBottom() + 2.0f);
        path.lineTo (points.front().x, plot.getBottom() + 2.0f);
        path.closeSubPath();
    }

    return path;
}

void SpectrumDisplay::paintGrid (Graphics& g)
{
    // Frequencies: the decades stand out, the steps between them barely.
    for (const auto hz : { 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f,
                           600.0f, 700.0f, 800.0f, 900.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 6000.0f,
                           7000.0f, 8000.0f, 9000.0f, 10000.0f })
    {
        const auto decade = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;

        g.setColour (Colour (0xffffd2d6).withAlpha (decade ? 0.11f : 0.04f));
        g.fillRect (xForHz (hz), plot.getY(), 1.0f, plot.getHeight());
    }

    // One set of lines serves both scales: sensitivity on the left, reduction
    // on the right.
    for (int i = 1; i < 6; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * (float) i / 6.0f;

        g.setColour (Colour (0xffffd2d6).withAlpha (i == 3 ? 0.2f : 0.06f));
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
    }
}

void SpectrumDisplay::paintSpectrum (Graphics& g)
{
    if (shownIn.empty())
        return;

    // What came in, as a body; what goes out, as the line over it.
    g.setGradientFill (ColourGradient (Colour (0x5cc8a3a8), 0.0f, plot.getY(),
                                       Colour (0x14c8a3a8), 0.0f, plot.getBottom(), false));
    g.fillPath (spectrumPath (cumulativeIn, true));

    g.setColour (bone.withAlpha (0.6f));
    g.strokePath (spectrumPath (cumulativeOut, false), PathStrokeType (1.3f, PathStrokeType::curved, PathStrokeType::rounded));
}

void SpectrumDisplay::paintReduction (Graphics& g)
{
    if (shownRed.empty() || binHz <= 0.0)
        return;

    const float step = 3.0f;
    const auto last = (int) shownRed.size() - 1;
    const auto span = reductionSpan();
    std::vector<Point<float>> points;

    for (float x = plot.getX() - step; x <= plot.getRight() + 2.0f * step; x += step)
    {
        const auto from = hzForX (x - step * 0.5f) / (float) binHz;
        const auto to = hzForX (x + step * 0.5f) / (float) binHz;
        float reduction = 0.0f;

        if (to - from < 1.0f)
        {
            // Fewer bins than pixels down here: read between them.
            const auto at = jlimit (0.0f, (float) last, 0.5f * (from + to));
            const auto k = jmin (last - 1, (int) at);
            reduction = shownRed[(size_t) k] + (at - (float) k) * (shownRed[(size_t) k + 1] - shownRed[(size_t) k]);
        }
        else
        {
            // More than one: the deepest of them, so that no cut goes unseen.
            for (int k = jlimit (0, last, (int) std::ceil (from)); k <= jmin (last, (int) to); ++k)
                reduction = jmax (reduction, shownRed[(size_t) k]);
        }

        points.push_back ({ x, plot.getY() + jmin (span, reduction) / span * plot.getHeight() });
    }

    const auto edge = curveThrough (points);

    auto area = edge;
    area.lineTo (points.back().x, plot.getY() - 2.0f);
    area.lineTo (points.front().x, plot.getY() - 2.0f);
    area.closeSubPath();

    g.setGradientFill (ColourGradient (ember.withAlpha (0.62f), 0.0f, plot.getY(),
                                       ember.withAlpha (0.2f), 0.0f, plot.getY() + plot.getHeight() * 0.6f, false));
    g.fillPath (area);

    g.setColour (ember.withAlpha (0.3f));
    g.strokePath (edge, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (emberLit);
    g.strokePath (edge, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
}

void SpectrumDisplay::paintCurve (Graphics& g, const rb::Settings& settings)
{
    const float step = 2.0f;
    const auto zero = yForSensitivity (0.0f);
    const auto floorDb = -kSensitivitySpan - 2.0f;

    auto trace = [&] (const rb::Settings& s)
    {
        Path path;
        bool started = false;

        for (float x = plot.getX(); x <= plot.getRight() + step; x += step)
        {
            const auto y = yForSensitivity (jlimit (floorDb, kSensitivitySpan + 2.0f, rb::sensitivityDb (s, hzForX (x))));

            if (! started)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);

            started = true;
        }

        return path;
    };

    // Each node first, on its own, as a tinted shape against the middle line.
    for (int i = 0; i < kNumNodes; ++i)
    {
        rb::Settings alone;
        alone.lowCutOn = alone.highCutOn = false;

        if (i == kLowCutNode)
        {
            alone.lowCutOn = settings.lowCutOn;
            alone.lowCutHz = settings.lowCutHz;
        }
        else if (i == kHighCutNode)
        {
            alone.highCutOn = settings.highCutOn;
            alone.highCutHz = settings.highCutHz;
        }
        else
        {
            alone.bands[i] = settings.bands[i];
        }

        const auto on = i == kLowCutNode ? settings.lowCutOn
                      : i == kHighCutNode ? settings.highCutOn
                      : settings.bands[i].on && std::abs (settings.bands[i].gainDb) > 0.05f;

        if (! on)
            continue;

        const auto outline = trace (alone);
        auto shape = outline;
        shape.lineTo (plot.getRight() + step, zero);
        shape.lineTo (plot.getX(), zero);
        shape.closeSubPath();

        const auto colour = nodeColour (i);
        const auto lit = isSelected (i) || i == hovered;

        g.setColour (colour.withAlpha (lit ? 0.3f : 0.17f));
        g.fillPath (shape);
        g.setColour (colour.withAlpha (lit ? 0.85f : 0.45f));
        g.strokePath (outline, PathStrokeType (1.0f, PathStrokeType::curved));
    }

    // Then the sum of them: what the suppressor actually goes by.
    const auto curve = trace (settings);

    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (4.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.95f));
    g.strokePath (curve, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

void SpectrumDisplay::paintNodes (Graphics& g, const rb::Settings& settings)
{
    for (int i = 0; i < kNumNodes; ++i)
    {
        if (! isUsed (i))
            continue;

        const auto isCut = i >= rb::kNumBands;
        const auto on = parameter (nodeIds (i).on) > 0.5f;

        const auto centre = nodePosition (i, settings);
        const auto colour = nodeColour (i);
        const auto lit = isSelected (i) || i == hovered;

        // The cuts are handles to slide, the bands are points to place.
        const auto shape = isCut ? Rectangle<float> (9.0f, 22.0f).withCentre (centre)
                                 : Rectangle<float> (2.0f * kNodeRadius, 2.0f * kNodeRadius).withCentre (centre);
        const auto corner = isCut ? 4.5f : kNodeRadius;

        if (isSelected (i))
        {
            g.setColour (colour.withAlpha (0.22f));
            g.fillRoundedRectangle (shape.expanded (6.0f), corner + 6.0f);
            g.setColour (colour.withAlpha (0.8f));
            g.drawRoundedRectangle (shape.expanded (4.0f), corner + 4.0f, 1.2f);
        }
        else if (lit)
        {
            g.setColour (colour.withAlpha (0.2f));
            g.fillRoundedRectangle (shape.expanded (5.0f), corner + 5.0f);
        }

        g.setColour (Colour (0xff120607));
        g.fillRoundedRectangle (shape.expanded (1.5f), corner + 1.5f);

        if (on)
        {
            g.setColour (colour);
            g.fillRoundedRectangle (shape, corner);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (shape, corner, 1.2f);
        }
        else
        {
            // Bypassed: the ring stays where it was, empty.
            g.setColour (colour.withAlpha (0.6f));
            g.drawRoundedRectangle (shape, corner, 1.4f);
        }

        // A band that is not for the whole picture says which part it is for.
        if (! isCut)
        {
            static const char* const letters[] = { "", "L", "R", "M", "S" };
            const auto place = jlimit (0, 4, roundToInt (parameter (nodeIds (i).place)));

            if (place > 0)
            {
                const auto tag = Rectangle<float> (16.0f, 14.0f).withCentre ({ centre.x, centre.y - kNodeRadius - 12.0f });

                g.setColour (Colour (0xcc120607));
                g.fillRoundedRectangle (tag, 4.0f);
                g.setColour (colour.withAlpha (on ? 1.0f : 0.6f));
                g.setFont (monoFont (10.0f).boldened());
                g.drawText (letters[place], tag, Justification::centred, false);
            }
        }
    }
}

void SpectrumDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto settings = processor.readSettings();

    g.setGradientFill (ColourGradient (Colour (0xff24090d), 0.0f, bounds.getY(),
                                       Colour (0xff0d0506), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 14.0f);

    {
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot.toNearestInt());

        g.setColour (Colour (0x40000000));
        g.fillRect (plot);

        paintGrid (g);
        paintSpectrum (g);
        paintReduction (g);
        paintCurve (g, settings);
        paintNodes (g, settings);

        if (framing)
        {
            g.setColour (ember.withAlpha (0.12f));
            g.fillRect (frame);
            g.setColour (emberLit.withAlpha (0.75f));
            g.drawRect (frame, 1.0f);
        }
    }

    g.setColour (Colour (0x33ffd2d6));
    g.drawRect (plot.expanded (0.5f), 1.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds, 14.0f, 1.0f);

    // Frequencies along the foot.
    g.setFont (monoFont (10.0f));
    g.setColour (haze);

    const std::pair<float, const char*> marks[] = { { 30.0f, "30" }, { 50.0f, "50" }, { 100.0f, "100" },
                                                    { 200.0f, "200" }, { 500.0f, "500" }, { 1000.0f, "1k" },
                                                    { 2000.0f, "2k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } };

    for (const auto& mark : marks)
        g.drawText (mark.second, Rectangle<float> (44.0f, 14.0f).withCentre ({ xForHz (mark.first), plot.getBottom() + 12.0f }),
                    Justification::centred, false);

    // Sensitivity on the left, in the white of its curve; reduction on the
    // right, in the ember of its own.
    const auto span = reductionSpan();

    for (int i = 1; i < 6; ++i)
    {
        const auto y = plot.getY() + plot.getHeight() * (float) i / 6.0f;
        const auto sensitivity = (3 - i) * 6;
        const auto reduction = span * (float) i / 6.0f;

        g.setColour (mist.withAlpha (0.6f));
        g.drawText ((sensitivity > 0 ? "+" : "") + String (sensitivity),
                    Rectangle<float> (0.0f, y - 7.0f, plot.getX() - 7.0f, 14.0f), Justification::centredRight, false);

        g.setColour (ember.withAlpha (0.95f));
        g.drawText ("-" + (std::abs (reduction - std::round (reduction)) < 0.01f ? String (roundToInt (reduction))
                                                                                  : String (reduction, 1)),
                    Rectangle<float> (plot.getRight() + 7.0f, y - 7.0f, 34.0f, 14.0f), Justification::centredLeft, false);
    }

    const auto panelArea = panel->isVisible() ? panel->getBounds().toFloat() : Rectangle<float>();

    auto capsule = [&g] (Rectangle<float> area)
    {
        g.setColour (Colour (0xb3000000));
        g.fillRoundedRectangle (area, area.getHeight() * 0.5f);
        g.setColour (Colour (0x33ffd2d6));
        g.drawRoundedRectangle (area, area.getHeight() * 0.5f, 1.0f);
    };

    // The deepest cut at this moment, as a number.
    const auto readout = Rectangle<float> (104.0f, 22.0f).withPosition (plot.getRight() - 114.0f, plot.getY() + 10.0f);

    if (! panelArea.intersects (readout))
    {
        capsule (readout);
        g.setColour (mist);
        g.setFont (monoFont (11.0f).boldened());
        g.drawText ("GR " + (deepest < 0.05f ? String ("0.0") : String (-deepest, 1)) + " dB", readout,
                    Justification::centred, false);
    }

    // The scale of the reduction, which a click changes.
    const auto scale = scaleArea();

    if (! panelArea.intersects (scale))
    {
        capsule (scale);
        g.setColour (ember);
        g.setFont (monoFont (11.0f).boldened());
        g.drawText ("-" + String (roundToInt (span)), scale.withTrimmedRight (16.0f), Justification::centred, false);

        Path arrow;
        const auto cx = scale.getRight() - 13.0f;
        const auto cy = scale.getCentreY();
        arrow.startNewSubPath (cx - 3.5f, cy - 1.5f);
        arrow.lineTo (cx, cy + 2.0f);
        arrow.lineTo (cx + 3.5f, cy - 1.5f);
        g.strokePath (arrow, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // The foot of the display, always: where the pointer is, in the units of
    // the two axes it moves along; and, with the pointer elsewhere, what a
    // double click and the Delete key do here.
    const auto foot = Rectangle<float> (220.0f, 16.0f).withPosition (plot.getX() + 10.0f, plot.getBottom() - 24.0f);

    if (cursorInside)
    {
        const auto hz = hzForX (cursor.x);
        const auto db = sensitivityForY (cursor.y);

        g.setColour (mist.withAlpha (0.9f));
        g.setFont (monoFont (11.0f).boldened());
        g.drawText ((hz >= 1000.0f ? String (hz / 1000.0f, 2) + " kHz" : String (roundToInt (hz)) + " Hz")
                        + "    " + (db > 0.04f ? "+" : "") + String (db, 1) + " dB",
                    foot, Justification::centredLeft, false);
    }
    else
    {
        int used = 0;

        for (int i = 0; i < rb::kNumBands; ++i)
            used += isUsed (i) ? 1 : 0;

        g.setColour (haze.withAlpha (0.55f));
        g.setFont (capsFont (9.0f));
        g.drawText (used < rb::kNumBands ? "DOUBLE-CLICK: ADD   -   DEL: REMOVE" : "DEL: REMOVE THE SELECTION",
                    foot, Justification::centredLeft, false);
    }
}

} // namespace onirique
