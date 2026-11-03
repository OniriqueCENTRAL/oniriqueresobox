// Onirique ResoBox — the display: spectrum, reduction, and the sensitivity curve
#pragma once

#include <array>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../PluginProcessor.h"
#include "Controls.h"

namespace onirique
{

/** Everything the suppressor sees and does, on one frequency axis.

    From the bottom up: the spectrum going in and coming out; the sensitivity
    curve with its nodes, drawn about the middle line; and, hanging from the
    top edge, what is being taken away at this moment.

    The curve starts with two cuts and three bells. A double click in the
    empty adds a bell there, up to eight; a double click on a node bypasses
    it; its panel has the button that removes it.

    Several nodes can be selected at once, with Ctrl or Shift and a click or
    with a frame drawn in the empty. They are then dragged together, and the
    Delete key removes those of them that are bells.

    In multiband the curve is four zones side by side instead: each is dragged
    up or down as a whole, the lines between them left or right, and a click
    on one opens its own island.
*/
class SpectrumDisplay final : public Component,
                              private Timer
{
public:
    explicit SpectrumDisplay (ResoBoxProcessor&);
    ~SpectrumDisplay() override;

    void paint (Graphics&) override;
    void resized() override;

    void mouseMove (const MouseEvent&) override;
    void mouseExit (const MouseEvent&) override;
    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;
    void mouseDoubleClick (const MouseEvent&) override;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails&) override;
    bool keyPressed (const KeyPress&) override;

    /** Selects one node and nothing else, or nothing at all with -1. */
    void select (int node);

    /** Adds a node to the selection, or takes it out. */
    void setSelected (int node, bool shouldBeSelected);

    /** The node the panel reads its values from: the one clicked last among
        those selected, or -1 when nothing is. */
    int getSelected() const noexcept { return selected; }
    bool isSelected (int node) const noexcept { return isPositiveAndBelow (node, kNumNodes) && inSelection[(size_t) node]; }

    /** What the pointer is resting on, in words, for the description the
        editor shows after two seconds: a control of the island, a node, one
        of the readouts, or the display itself. Empty outside all of them. */
    String descriptionAt (Point<int> position) const;

    static Colour nodeColour (int node);
    static String nodeName (int node);

    /** Multiband: selects one zone, or none with -1, and opens its island. */
    void selectZone (int zone);

    static Colour zoneColour (int zone);
    static String zoneName (int zone);

private:
    class NodePanel;
    class ZonePanel;

    void timerCallback() override;

    float xForHz (float hz) const;
    float hzForX (float x) const;
    float yForSensitivity (float db) const;
    float sensitivityForY (float y) const;
    float reductionSpan() const;

    /** Whether a node is on the curve at all: the cuts always, a bell once added. */
    bool isUsed (int node) const;
    Point<float> nodePosition (int node, const rb::Settings&) const;

    /** How far up its own curve a band's node sits: at the top for a bell,
        half way for a shelf, which is at half its height at its frequency. */
    float nodeHeight (int node) const;
    int nodeAt (Point<float>) const;
    void addBand (Point<float>);
    void removeNode (int node);
    void removeSelection();

    /** Moves the selection to the next node up or down the spectrum. */
    void stepSelection (int direction);

    /** Brings the panel in line with the selection: there whenever anything
        is selected, for one node or for several. */
    void selectionChanged();

    /** The node acted on and, when it is one of several selected, the others. */
    std::vector<int> withSelection (int node) const;
    void placePanel();
    Rectangle<float> scaleArea() const;
    void showScaleMenu();

    float parameter (const String& id) const;
    void setParameter (const String& id, float value);
    void changeParameter (const String& id, float value);
    void gesture (const String& id, bool begin);

    void paintGrid (Graphics&);
    void paintSpectrum (Graphics&);
    void paintReduction (Graphics&);
    void paintCurve (Graphics&, const rb::Settings&);
    void paintNodes (Graphics&, const rb::Settings&);

    // Multiband.
    bool isMultiband() const;
    float crossoverHz (int crossover) const;

    /** The zone under a point by its frequency, or -1 outside the plot. */
    int zoneAt (Point<float>) const;

    /** The crossover line within a few pixels of a point, or -1. */
    int crossoverAt (Point<float>) const;

    /** Where a zone starts and ends across the plot. */
    Range<float> zoneSpan (int zone) const;
    void placeZonePanel();
    void paintZones (Graphics&, const rb::Settings&);

    void zoneMouseDown (const MouseEvent&);
    void zoneMouseDrag (const MouseEvent&);
    void zoneMouseUp();

    /** The spectrum as a line with no corners in it: averaged over a third of
        an octave, then drawn through as a curve. */
    Path spectrumPath (const std::vector<double>& cumulative, bool closed) const;

    ResoBoxProcessor& processor;
    std::unique_ptr<NodePanel> panel;
    std::unique_ptr<ZonePanel> zonePanel;

    // Multiband: the zone whose island is open, the one or the line under the
    // pointer, and what a drag started from.
    int selectedZone = -1, hoveredZone = -1, hoveredCrossover = -1;
    int draggingZone = -1, draggingCrossover = -1;
    float dragStartValue = 0.0f;
    bool shownMultiband = false;

    Rectangle<float> plot;
    std::array<bool, kNumNodes> inSelection {};
    int selected = -1, hovered = -1, dragging = -1;
    std::vector<int> shownNodes;
    int shownPrimary = -1;

    // Where the pointer is, for the readout at the foot of the display.
    Point<float> cursor;
    bool cursorInside = false;

    // A drag moves every selected node by the same interval and the same
    // number of dB, from where each of them started.
    struct Start { float freq = 0.0f, gain = 0.0f; };
    std::array<Start, kNumNodes> dragStart;
    Point<float> dragOrigin;

    // A frame being drawn in the empty, and what was selected before it.
    bool framing = false;
    Rectangle<float> frame;
    std::array<bool, kNumNodes> frameBase {};

    // What is drawn: the last frame, with the spectrum falling back slowly.
    std::vector<float> fetchedIn, fetchedOut, fetchedRed;
    std::vector<float> shownIn, shownOut, shownRed;
    std::vector<double> cumulativeIn, cumulativeOut;
    double binHz = 0.0;
    float deepest = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumDisplay)
};

} // namespace onirique
