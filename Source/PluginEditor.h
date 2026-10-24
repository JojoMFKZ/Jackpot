#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

#define U8(s) juce::String (juce::CharPointer_UTF8 (s))

namespace jackpot::ui
{
// ---------------------------------------------------------------------------
namespace colours
{
const juce::Colour bg { 0xff0c0c0e }, panel { 0xff131316 }, line { 0xff1e1f23 }, btn { 0xff141417 },
                   btnLine { 0xff26272c }, text { 0xffececee }, muted { 0xff74777f }, dim { 0xff55585f },
                   soft { 0xffb4b6bd }, gold { 0xffffd24a }, track { 0xff26272c }, cap { 0xff1e1f23 };
}

struct Fonts
{
    static Fonts& get();
    juce::Font sans (float h, int weight = 400) const;   // 400, 600, 700, 800
    juce::Font mono (float h) const;
private:
    Fonts();
    juce::Typeface::Ptr regular, semibold, bold, extrabold, monoFace;
};

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool, bool, bool, bool, bool,
                            const juce::String&, const juce::String&, const juce::Drawable*, const juce::Colour*) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

// rounded text button in three flavours
class Pill : public juce::Button
{
public:
    enum Style { Outline, Solid, Ghost, Segment };
    Pill (const juce::String& text, Style s = Outline) : juce::Button (text), style (s) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    Style style;
    bool selected = false;
    juce::Colour accent = colours::text;
    float fontSize = 12.f;
    bool lockIcon = false;
    bool bright = false;
};

// pill-shaped on/off switch
class Switch : public juce::ToggleButton
{
public:
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour onColour = colours::text;
};

// knob with label underneath (value shown while hovering/dragging)
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (JackpotProcessor&, const char* paramId, const juce::String& label, juce::Colour accent);
    ~Knob() override;
    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void setAccent (juce::Colour c);
    float labelSize = 9.5f;
private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    JackpotProcessor& proc;
    juce::RangedAudioParameter* param;
    juce::String label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
};

// a creature you can click
class SpriteButton : public juce::Button
{
public:
    SpriteButton() : juce::Button ("sprite") {}
    void set (const jackpot::Look& l, const juce::String& n, const juce::String& sub) { look = l; name = n; note = sub; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    jackpot::Look look;
    juce::String name, note;
    juce::Colour ring = juce::Colours::transparentBlack;
    bool showText = true;
    float spriteSize = 64.f;
};

// ---------------------------------------------------------------------------
class MapView : public juce::Component, private juce::Timer
{
public:
    explicit MapView (JackpotProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    void timerCallback() override;
    void rebuildTerrain();
    void moveTo (juce::Point<float>);
    JackpotProcessor& proc;
    juce::Image terrain;
    int terrainRegion = -1, terrainChaos = -1;
    float phase = 0.f;
    Pill ranges[3] { { "Close", Pill::Segment }, { "Wider", Pill::Segment }, { "Wild", Pill::Segment } };
    Pill teleport { "Teleport", Pill::Solid };
};

class HoodView : public juce::Component
{
public:
    explicit HoodView (JackpotProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    std::function<void()> onDone;
private:
    struct ModuleBox
    {
        jackpot::Module module;
        juce::String name;
        std::unique_ptr<Switch> on;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;
        std::unique_ptr<Pill> reroll;
        juce::OwnedArray<Knob> knobs;
        std::unique_ptr<Switch> freeze;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach;
        juce::Rectangle<int> bounds;
    };
    JackpotProcessor& proc;
    std::vector<std::unique_ptr<ModuleBox>> boxes;
    juce::OwnedArray<Knob> globals;
    Pill done { "Done" }, shuffle { "Shuffle order", Pill::Ghost };
    juce::Rectangle<int> chainArea;
};

class FamilyView : public juce::Component, private juce::Timer
{
public:
    explicit FamilyView (JackpotProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebuild (bool newLitter = false);
private:
    void timerCallback() override;
    void layoutTraits();
    JackpotProcessor& proc;
    jackpot::Genome parent, mate;
    bool hasMate = false;
    std::array<jackpot::Genome, 6> kids;
    juce::uint64 litter = 1;
    int bob = 0;
    SpriteButton kidButtons[6];
    SpriteButton keptButtons[4];
    std::vector<jackpot::Genome> keptGenomes;
    Pill breed { "Breed new litter", Pill::Solid };
    Pill hoodButton { "Under the hood" };
    Pill removeMate { U8 ("remove mate  \xc3\x97"), Pill::Ghost };
    juce::OwnedArray<Pill> traitChips;
    std::vector<int> traitModules;
    juce::String dormantText;
    HoodView hood;
};

class TrailView : public juce::Component
{
public:
    explicit TrailView (JackpotProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    JackpotProcessor& proc;
    std::vector<JackpotProcessor::Entry> items;
    std::vector<bool> keptFlags;
    std::vector<juce::Rectangle<float>> hit;
};
} // namespace jackpot::ui

// ===========================================================================
class JackpotEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseW = 1100, baseH = 720;
    explicit JackpotEditor (JackpotProcessor&);
    ~JackpotEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (jackpot::ui::colours::bg); }

private:
    class Canvas : public juce::Component
    {
    public:
        explicit Canvas (JackpotEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        JackpotEditor& ed;
    };
    void timerCallback() override;
    void refreshAll();
    void showPresetMenu();
    void askToSave();

    JackpotProcessor& proc;
    jackpot::ui::LookAndFeel lnf;
    Canvas canvas { *this };
    jackpot::ui::MapView map;
    jackpot::ui::FamilyView family;
    jackpot::ui::TrailView trail;
    jackpot::ui::Pill back { U8 ("\xe2\x86\x90  Back") }, keep { U8 ("\xe2\x98\x86  Keep") }, saveAs { U8 ("Save as\xe2\x80\xa6") }, nameButton { "", jackpot::ui::Pill::Ghost };
    std::unique_ptr<jackpot::ui::Knob> mixKnob, outKnob;
    jackpot::ui::Switch autoLevel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttach;
    int lastChange = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JackpotEditor)
};
