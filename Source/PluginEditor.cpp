#include "PluginEditor.h"
#include "BinaryData.h"
#include <cstring>
#include <algorithm>

using namespace jackpot;
using namespace jackpot::ui;
using juce::Graphics;
using juce::Rectangle;

namespace
{
float textWidth (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }

juce::Colour moduleColour (float hue, int module)
{
    return juce::Colour::fromHSL (std::fmod (hue + module * 0.097f, 1.f), 0.7f, 0.62f, 1.f);
}

juce::String chainText (JackpotProcessor& p)
{
    static const char* names[] = { "", "fuzz", "crush", "pitch", "filter" };
    std::array<int, 4> o { 1, 2, 3, 4 };
    int order = (int) p.apvts.getRawParameterValue ("order")->load();
    for (int i = 0; i < order; ++i) std::next_permutation (o.begin(), o.end());
    const juce::String sep = U8 (" \xe2\x80\xba ");
    return juce::String (names[o[0]]) + sep + names[o[1]] + sep + names[o[2]] + sep + names[o[3]]
           + sep + "harmony" + sep + "resonator" + sep + "tape" + sep + "stutter" + sep + "delay" + sep + "reverb";
}

void sectionLabel (Graphics& g, const juce::String& t, float x, float y)
{
    g.setColour (colours::muted);
    auto f = Fonts::get().sans (10.5f, 600);
    g.setFont (f);
    // letter-spaced caps
    float cx = x;
    for (auto ch : t)
    {
        juce::String s = juce::String::charToString (ch);
        g.drawText (s, Rectangle<float> (cx, y, 20.f, 14.f), juce::Justification::centredLeft, false);
        cx += textWidth (f, s) + 1.3f;
    }
}
} // namespace

// ===========================================================================
//  Fonts + look and feel
// ===========================================================================
Fonts& Fonts::get() { static Fonts f; return f; }

Fonts::Fonts()
{
    regular   = juce::Typeface::createSystemTypefaceFor (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize);
    semibold  = juce::Typeface::createSystemTypefaceFor (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize);
    bold      = juce::Typeface::createSystemTypefaceFor (BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
    extrabold = juce::Typeface::createSystemTypefaceFor (BinaryData::InterExtraBold_ttf, BinaryData::InterExtraBold_ttfSize);
    monoFace  = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);
}

juce::Font Fonts::sans (float h, int w) const
{
    auto tf = w >= 800 ? extrabold : w >= 700 ? bold : w >= 600 ? semibold : regular;
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

juce::Font Fonts::mono (float h) const { return juce::Font (juce::FontOptions (monoFace).withHeight (h)); }

LookAndFeel::LookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::btnLine);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::AlertWindow::backgroundColourId, colours::panel);
    setColour (juce::AlertWindow::textColourId, colours::text);
    setColour (juce::AlertWindow::outlineColourId, colours::btnLine);
    setColour (juce::TextEditor::backgroundColourId, colours::btn);
    setColour (juce::TextEditor::textColourId, colours::text);
    setColour (juce::TextEditor::outlineColourId, colours::btnLine);
    setColour (juce::TextEditor::focusedOutlineColourId, colours::soft);
    setColour (juce::TextEditor::highlightColourId, colours::btnLine);
    setColour (juce::CaretComponent::caretColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::btn);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour (juce::TooltipWindow::textColourId, colours::text);
}

juce::Font LookAndFeel::getPopupMenuFont() { return Fonts::get().sans (13.5f); }
juce::Font LookAndFeel::getAlertWindowTitleFont() { return Fonts::get().sans (16.f, 700); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return Fonts::get().sans (13.f); }
juce::Font LookAndFeel::getAlertWindowFont() { return Fonts::get().sans (13.f); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int) { return Fonts::get().sans (12.5f, 600); }

void LookAndFeel::drawButtonBackground (Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (over ? juce::Colour (0xff1c1d21) : colours::btn);
    g.fillRoundedRectangle (r, 8.f);
    g.setColour (colours::btnLine);
    g.drawRoundedRectangle (r, 8.f, 1.f);
}

void LookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (colours::panel);
    g.setColour (colours::btnLine);
    g.drawRect (0, 0, w, h, 1);
}

void LookAndFeel::drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const juce::String& text)
{
    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (10.5f, 600));
    g.drawText (text.toUpperCase(), area.reduced (12, 0), juce::Justification::bottomLeft);
}

void LookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool sep, bool active, bool hi, bool ticked, bool,
                                     const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*)
{
    if (sep) { g.setColour (colours::line); g.drawHorizontalLine (area.getCentreY(), 8.f, (float) area.getRight() - 8.f); return; }
    auto r = area.reduced (4, 1).toFloat();
    if (hi && active) { g.setColour (colours::btnLine); g.fillRoundedRectangle (r, 5.f); }
    g.setColour (active ? colours::text : colours::dim);
    g.setFont (getPopupMenuFont());
    g.drawText (text, r.reduced (10, 0), juce::Justification::centredLeft);
    if (ticked) { g.setColour (colours::gold); g.fillEllipse (r.getRight() - 14.f, r.getCentreY() - 3.f, 6.f, 6.f); }
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    float size = juce::jmin (area.getWidth(), area.getHeight());
    auto c = area.getCentre();
    float r = size * 0.5f - 2.f, thick = juce::jmax (2.5f, size * 0.07f);
    juce::Colour accent = s.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["accent"]) : colours::soft;

    juce::Path track, val;
    track.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, start, end, true);
    val.addCentredArc (c.x, c.y, r - thick * 0.5f, r - thick * 0.5f, 0.f, start, start + pos * (end - start), true);
    juce::PathStrokeType st (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (colours::track); g.strokePath (track, st);
    g.setColour (accent); g.strokePath (val, st);

    float capR = r - thick - 3.f;
    g.setColour (colours::cap);
    g.fillEllipse (Rectangle<float> (capR * 2, capR * 2).withCentre (c));
    float a = start + pos * (end - start);
    juce::Point<float> p1 (c.x + std::sin (a) * capR * 0.35f, c.y - std::cos (a) * capR * 0.35f);
    juce::Point<float> p2 (c.x + std::sin (a) * capR * 0.85f, c.y - std::cos (a) * capR * 0.85f);
    g.setColour (juce::Colours::white);
    g.drawLine ({ p1, p2 }, juce::jmax (2.f, size * 0.055f));
}

// ===========================================================================
//  Small widgets
// ===========================================================================
void Pill::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    float rad = juce::jmin (r.getHeight() * 0.5f, 9.f);
    if (style == Segment) rad = 7.f;
    if (down) r = r.translated (0.f, 0.5f);
    auto f = Fonts::get().sans (fontSize, style == Solid ? 700 : 600);
    switch (style)
    {
        case Solid:
            g.setColour (over ? juce::Colours::white : colours::text);
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (colours::bg);
            break;
        case Outline:
            g.setColour (over ? juce::Colour (0xff1c1d21) : colours::btn);
            g.fillRoundedRectangle (r, rad);
            g.setColour (selected ? accent.withAlpha (0.55f) : (over ? juce::Colour (0xff4a4c53) : colours::btnLine));
            g.drawRoundedRectangle (r, rad, 1.f);
            g.setColour (selected ? accent : colours::text);
            break;
        case Ghost:
            g.setColour (bright ? (over ? juce::Colours::white : colours::text) : (over ? colours::text : colours::muted));
            break;
        case Segment:
            if (selected) { g.setColour (colours::text); g.fillRoundedRectangle (r, rad); g.setColour (colours::bg); }
            else g.setColour (over ? juce::Colours::white : colours::text);
            break;
    }
    if (! isEnabled()) g.setColour (colours::dim);
    auto textArea = r;
    if (lockIcon)
    {
        auto ic = Rectangle<float> (r.getX() + 11.f, r.getCentreY() - 6.f, 10.f, 12.f);
        auto c = selected ? accent : colours::dim;
        auto body = ic.removeFromBottom (6.5f);
        juce::Path sh;
        sh.addCentredArc (ic.getCentreX(), body.getY(), 3.f, 3.2f, 0.f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.setColour (c);
        g.strokePath (sh, juce::PathStrokeType (1.4f));
        if (selected) g.fillRoundedRectangle (body, 1.5f); else g.drawRoundedRectangle (body, 1.5f, 1.3f);
        textArea = r.withTrimmedLeft (26.f).withTrimmedRight (10.f);
        g.setColour (selected ? accent : colours::text);
    }
    g.setFont (f);
    g.drawText (getButtonText(), textArea, lockIcon ? juce::Justification::centredLeft : juce::Justification::centred, false);
}

void Switch::paintButton (Graphics& g, bool, bool)
{
    auto r = getLocalBounds().toFloat();
    float h = juce::jmin (r.getHeight(), r.getWidth() * 0.55f);
    auto pill = r.withSizeKeepingCentre (h * 1.8f, h);
    bool on = getToggleState();
    g.setColour (on ? onColour : juce::Colour (0xff33343a));
    g.fillRoundedRectangle (pill, h * 0.5f);
    float d = h - 5.f;
    g.setColour (on ? colours::bg : juce::Colours::white);
    g.fillEllipse (on ? pill.getRight() - d - 2.5f : pill.getX() + 2.5f, pill.getY() + 2.5f, d, d);
}

Knob::Knob (JackpotProcessor& p, const char* id, const juce::String& l, juce::Colour accent)
    : proc (p), param (p.apvts.getParameter (id)), label (l)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (-135.f), juce::degreesToRadians (135.f), true);
    slider.setMouseDragSensitivity (200);
    slider.setPopupMenuEnabled (false);
    slider.addListener (this);
    slider.addMouseListener (this, false);
    addAndMakeVisible (slider);
    attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, id, slider);
    for (auto& s : paramSpecs()) if (! std::strcmp (s.id, id)) slider.setDoubleClickReturnValue (true, (double) s.def);
    setAccent (accent);
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::setAccent (juce::Colour c)
{
    slider.getProperties().set ("accent", (juce::int64) c.getARGB());
    slider.repaint();
}

void Knob::resized()
{
    auto b = getLocalBounds();
    int size = juce::jmin (b.getWidth(), b.getHeight() - 14);
    slider.setBounds (b.removeFromTop (size).withSizeKeepingCentre (size, size));
}

void Knob::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    b.removeFromTop ((float) slider.getBottom());
    bool live = slider.isMouseOverOrDragging();
    g.setColour (live ? colours::text : juce::Colour (0xffa9acb3));
    g.setFont (live ? Fonts::get().mono (labelSize + 0.5f) : Fonts::get().sans (labelSize, 600));
    g.drawText (live ? param->getCurrentValueAsText() : label, b, juce::Justification::centredTop, false);
}

void SpriteButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    if (over && showText) { g.setColour (juce::Colour (0xff1a1a1e)); g.fillRoundedRectangle (r, 12.f); }
    if (! ring.isTransparent())
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (r.reduced (1.f), 10.f);
        g.setColour (ring);
        g.drawRoundedRectangle (r.reduced (1.f), 10.f, ring == colours::gold || ring == colours::text ? 2.f : 1.f);
    }
    float lift = (over ? 3.f : 0.f) + (down ? -1.f : 0.f);
    auto spriteArea = showText ? Rectangle<float> (spriteSize, spriteSize).withCentre ({ r.getCentreX(), r.getY() + 6.f + spriteSize * 0.5f - lift })
                               : r.reduced (r.getWidth() * 0.1f).translated (0.f, -lift);
    jackpot::sprite::draw (g, spriteArea, look);
    if (! showText) return;
    auto t = r.withTop (spriteArea.getBottom() + 3.f);
    g.setColour (juce::Colour (0xffd4d5da));
    g.setFont (Fonts::get().sans (10.5f, 600));
    g.drawText (name, t.removeFromTop (14.f), juce::Justification::centred, true);
    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (9.5f));
    g.drawText (note, t.removeFromTop (13.f), juce::Justification::centred, true);
}

// ===========================================================================
//  Map
// ===========================================================================
MapView::MapView (JackpotProcessor& p) : proc (p)
{
    const float values[3] = { 0.2f, 0.55f, 0.9f };
    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible (ranges[i]);
        ranges[i].fontSize = 11.f;
        ranges[i].onClick = [this, v = values[i]]
        {
            auto* c = proc.apvts.getParameter ("chaos");
            c->beginChangeGesture(); c->setValueNotifyingHost (v); c->endChangeGesture();
            refresh();
        };
    }
    ranges[0].setTooltip ("Neighbouring spots sound very alike");
    ranges[2].setTooltip ("Every step is a big jump");
    teleport.setTooltip ("Jump to a brand-new region of the map");
    teleport.onClick = [this] { proc.teleport(); refresh(); };
    addAndMakeVisible (teleport);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

void MapView::resized()
{
    int x = 15;
    for (auto& r : ranges) { int w = (int) textWidth (Fonts::get().sans (11.f, 600), r.getButtonText()) + 22; r.setBounds (x, 15, w, 26); x += w; }
    teleport.setBounds (getWidth() - 12 - 108, getHeight() - 12 - 38, 108, 38);
}

void MapView::refresh()
{
    float c = proc.apvts.getRawParameterValue ("chaos")->load();
    int sel = c < 0.37f ? 0 : c < 0.72f ? 1 : 2;
    for (int i = 0; i < 3; ++i) { ranges[i].selected = i == sel; ranges[i].repaint(); }
    repaint();
}

void MapView::rebuildTerrain()
{
    const int region = proc.region();
    const int cb = juce::roundToInt (proc.apvts.getRawParameterValue ("chaos")->load() * 40.f);
    if (terrain.isValid() && region == terrainRegion && cb == terrainChaos) return;
    terrainRegion = region; terrainChaos = cb;
    const int W = 52, H = 50;
    std::vector<juce::Colour> grid ((size_t) (W * H));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            grid[(size_t) (y * W + x)] = proc.evo.mapColour (proc.evo.at (region, (x + 0.5f) / W, (y + 0.5f) / H));
    // smooth upsample (smoothstep-weighted bilinear) so the terrain reads as soft gradients
    const int OW = 236, OH = 228;
    terrain = juce::Image (juce::Image::RGB, OW, OH, false);
    juce::Image::BitmapData bd (terrain, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < OH; ++y)
        for (int x = 0; x < OW; ++x)
        {
            float gx = juce::jlimit (0.f, (float) W - 1.001f, (x + 0.5f) / OW * W - 0.5f);
            float gy = juce::jlimit (0.f, (float) H - 1.001f, (y + 0.5f) / OH * H - 0.5f);
            int x0 = (int) gx, y0 = (int) gy;
            float fx = gx - x0, fy = gy - y0;
            auto c00 = grid[(size_t) (y0 * W + x0)], c10 = grid[(size_t) (y0 * W + x0 + 1)];
            auto c01 = grid[(size_t) ((y0 + 1) * W + x0)], c11 = grid[(size_t) ((y0 + 1) * W + x0 + 1)];
            auto top = c00.interpolatedWith (c10, fx), bot = c01.interpolatedWith (c11, fx);
            bd.setPixelColour (x, y, top.interpolatedWith (bot, fy));
        }
}

void MapView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::Path clip; clip.addRoundedRectangle (r, 16.f);
    g.saveState();
    g.reduceClipRegion (clip);
    g.fillAll (juce::Colour (0xff050506));

    rebuildTerrain();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    // draw slightly oversized so the smooth upscale has no hard edges
    g.drawImage (terrain, r, juce::RectanglePlacement::stretchToFit);

    const bool on = proc.onMap();
    const float cx = proc.apvts.getRawParameterValue ("map_x")->load() * r.getWidth();
    const float cy = proc.apvts.getRawParameterValue ("map_y")->load() * r.getHeight();

    for (float y = 16.f; y < r.getHeight(); y += 26.f)
        for (float x = 16.f; x < r.getWidth(); x += 26.f)
        {
            float d = on ? std::hypot (x - cx, y - cy) : 999.f;
            g.setColour (juce::Colours::white.withAlpha (0.08f + juce::jmax (0.f, 1.f - d / 150.f) * 0.5f));
            g.fillEllipse (x - 1.f, y - 1.f, 2.f, 2.f);
        }

    juce::ColourGradient vig (juce::Colours::transparentBlack, r.getCentreX(), r.getCentreY(), juce::Colours::black.withAlpha (0.65f), 0.f, 0.f, true);
    vig.addColour (0.55, juce::Colours::transparentBlack);
    g.setGradientFill (vig);
    g.fillRect (r);

    if (on)
    {
        juce::ColourGradient lamp (juce::Colours::white.withAlpha (0.16f), cx, cy, juce::Colours::white.withAlpha (0.f), cx + 70.f, cy, true);
        g.setGradientFill (lamp);
        g.fillEllipse (cx - 70.f, cy - 70.f, 140.f, 140.f);
        float pr = 8.f + phase * 22.f;
        g.setColour (juce::Colours::white.withAlpha (0.8f * (1.f - phase)));
        g.drawEllipse (cx - pr, cy - pr, pr * 2, pr * 2, 1.2f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (cx - 10.f, cy - 10.f, 20.f, 20.f, 2.f);
        g.fillEllipse (cx - 3.f, cy - 3.f, 6.f, 6.f);

        auto name = proc.getPatchName();
        auto f = Fonts::get().sans (11.f, 600);
        float tw = textWidth (f, name) + 18.f;
        auto tag = Rectangle<float> (tw, 22.f).withCentre ({ juce::jlimit (tw * 0.5f + 6.f, r.getWidth() - tw * 0.5f - 6.f, cx), juce::jmin (r.getHeight() - 30.f, cy + 30.f) });
        g.setColour (colours::bg.withAlpha (0.8f));
        g.fillRoundedRectangle (tag, 6.f);
        g.setColour (colours::text);
        g.setFont (f);
        g.drawText (name, tag, juce::Justification::centred);
    }
    else
    {
        auto box = Rectangle<float> (250.f, 52.f).withCentre (r.getCentre());
        g.setColour (colours::bg.withAlpha (0.82f));
        g.fillRoundedRectangle (box, 8.f);
        g.setColour (colours::soft);
        g.setFont (Fonts::get().sans (12.f, 400));
        g.drawFittedText ("Bred sounds live off the map.\nClick the map to wander again.", box.toNearestInt(), juce::Justification::centred, 2);
    }

    // segmented control backdrop
    auto seg = Rectangle<float> (12.f, 12.f, (float) (ranges[2].getRight() - 12 + 3), 32.f);
    g.setColour (colours::bg.withAlpha (0.7f));
    g.fillRoundedRectangle (seg, 10.f);
    g.restoreState();
    g.setColour (colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 16.f, 1.f);
}

void MapView::moveTo (juce::Point<float> p)
{
    proc.setMap (juce::jlimit (0.01f, 0.99f, p.x / (float) getWidth()), juce::jlimit (0.01f, 0.99f, p.y / (float) getHeight()));
    repaint();
}

void MapView::mouseDown (const juce::MouseEvent& e) { proc.beginMapGesture(); moveTo (e.position); }
void MapView::mouseDrag (const juce::MouseEvent& e) { moveTo (e.position); }
void MapView::mouseUp (const juce::MouseEvent&) { proc.endMapGesture(); }

void MapView::timerCallback()
{
    phase += 1.f / (30.f * 1.8f);
    if (phase >= 1.f) phase -= 1.f;
    if (proc.onMap()) repaint();
}

// ===========================================================================
//  Under the hood
// ===========================================================================
HoodView::HoodView (JackpotProcessor& p) : proc (p)
{
    auto& specs = paramSpecs();
    static const char* names[] = { "", "Fuzz", "Crush", "Pitch", "Filter", "Tape", "Stutter", "Delay", "Reverb", "Harmony", "Resonator" };
    for (int m = 1; m <= kNumFx; ++m)
    {
        auto box = std::make_unique<ModuleBox>();
        box->module = (Module) m;
        box->name = names[m];
        box->on = std::make_unique<Switch>();
        addAndMakeVisible (*box->on);
        box->onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, onParamId ((Module) m), *box->on);
        box->reroll = std::make_unique<Pill> (U8 ("\xe2\x86\xbb"), Pill::Ghost);
        box->reroll->fontSize = 14.f;
        box->reroll->setTooltip ("Re-roll just this effect");
        box->reroll->onClick = [this, m] { proc.randomizer.randomizeModule ((Module) m); };
        addAndMakeVisible (*box->reroll);
        for (auto& s : specs)
        {
            if ((int) s.module != m || juce::String (s.id).endsWith ("_on")) continue;
            if (s.kind == Kind::Bool)
            {
                box->freeze = std::make_unique<Switch>();
                box->freeze->setTooltip (s.name);
                addAndMakeVisible (*box->freeze);
                box->freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, s.id, *box->freeze);
                continue;
            }
            auto* k = box->knobs.add (new Knob (proc, s.id, s.label, colours::soft));
            k->labelSize = 8.5f;
            addAndMakeVisible (k);
        }
        boxes.push_back (std::move (box));
    }
    for (auto id : { "in_gain", "drift", "drift_rate" })
    {
        for (auto& s : specs)
            if (! std::strcmp (s.id, id))
            {
                auto* k = globals.add (new Knob (proc, s.id, s.label, colours::soft));
                k->labelSize = 8.5f;
                addAndMakeVisible (k);
            }
    }
    done.onClick = [this] { if (onDone) onDone(); };
    shuffle.onClick = [this]
    {
        auto* o = proc.apvts.getParameter ("order");
        int cur = (int) proc.apvts.getRawParameterValue ("order")->load();
        int next = (cur + 1 + juce::Random::getSystemRandom().nextInt (22)) % 24;
        o->beginChangeGesture(); o->setValueNotifyingHost (o->convertTo0to1 ((float) next)); o->endChangeGesture();
        repaint();
    };
    shuffle.fontSize = 11.f;
    addAndMakeVisible (done);
    addAndMakeVisible (shuffle);
}

void HoodView::resized()
{
    done.setBounds (getWidth() - 16 - 64, 12, 64, 28);
    int gx = 16;
    for (auto* k : globals) { k->setBounds (gx, 48, 44, 50); gx += 50; }
    shuffle.setBounds (gx + 6, 52, 96, 24);
    chainArea = { gx + 10, 78, getWidth() - gx - 26, 16 };

    const int rows = ((int) boxes.size() + 1) / 2;
    const int top = 102, gap = 6, colW = (getWidth() - 32 - gap) / 2, rowH = (getHeight() - top - 12 - gap * (rows - 1)) / rows;
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        auto& b = *boxes[i];
        int col = (int) i % 2, row = (int) i / 2;
        b.bounds = { 16 + col * (colW + gap), top + row * (rowH + gap), colW, rowH };
        auto inner = b.bounds.reduced (10, 4);
        auto head = inner.removeFromTop (18);
        b.on->setBounds (head.removeFromRight (34));
        b.reroll->setBounds (head.removeFromRight (24));
        if (b.freeze) b.freeze->setBounds (head.removeFromRight (40).reduced (4, 1));
        int n = b.knobs.size();
        int kw = juce::jmin (44, inner.getWidth() / juce::jmax (1, n));
        for (int k = 0; k < n; ++k) b.knobs[k]->setBounds (inner.getX() + k * kw, inner.getY() + 2, kw, inner.getHeight() - 2);
    }
}

void HoodView::refresh()
{
    auto L = proc.evo.look (proc.evo.current());
    for (size_t i = 0; i < boxes.size(); ++i)
    {
        auto c = moduleColour (L.hue, (int) i);
        boxes[i]->on->onColour = c;
        for (auto* k : boxes[i]->knobs) k->setAccent (c);
        if (boxes[i]->freeze) boxes[i]->freeze->onColour = c;
    }
    repaint();
}

void HoodView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 16.f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 16.f, 1.f);

    g.setColour (colours::text);
    g.setFont (Fonts::get().sans (14.f, 700));
    g.drawText (U8 ("Under the hood \xe2\x80\x94 ") + proc.getPatchName(), 16, 12, getWidth() - 110, 28, juce::Justification::centredLeft);
    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (11.f));
    g.drawText ("The real effects behind each trait. You never have to touch these.", 16, 34, getWidth() - 110, 14, juce::Justification::centredLeft);
    g.setFont (Fonts::get().mono (10.f));
    g.drawText (chainText (proc), chainArea, juce::Justification::centredLeft, true);

    for (auto& b : boxes)
    {
        bool on = b->on->getToggleState();
        g.setColour (juce::Colour (0xff18181c));
        g.fillRoundedRectangle (b->bounds.toFloat(), 10.f);
        g.setColour (on ? colours::text : colours::dim);
        g.setFont (Fonts::get().sans (12.f, 700));
        g.drawText (b->name, b->bounds.getX() + 10, b->bounds.getY() + 6, 120, 20, juce::Justification::centredLeft);
        if (b->freeze)
        {
            g.setColour (colours::muted);
            g.setFont (Fonts::get().sans (9.f, 600));
            g.drawText ("FREEZE", b->freeze->getX() - 40, b->freeze->getY(), 38, b->freeze->getHeight(), juce::Justification::centredRight);
        }
        for (auto* k : b->knobs) k->setAlpha (on ? 1.f : 0.4f);
    }
}

// ===========================================================================
//  Family
// ===========================================================================
FamilyView::FamilyView (JackpotProcessor& p) : proc (p), hood (p)
{
    for (int k = 0; k < 6; ++k)
    {
        addAndMakeVisible (kidButtons[k]);
        kidButtons[k].onClick = [this, k] { proc.goTo (kids[(size_t) k]); rebuild (true); };
        kidButtons[k].setTooltip ("Make this one the parent");
    }
    for (int k = 0; k < 4; ++k)
    {
        addChildComponent (keptButtons[k]);
        keptButtons[k].showText = false;
        keptButtons[k].ring = colours::gold.withAlpha (0.45f);
        keptButtons[k].onClick = [this, k]
        {
            if ((size_t) k < keptGenomes.size()) { mate = keptGenomes[(size_t) k]; hasMate = true; rebuild (true); }
        };
    }
    kidButtons[0].setComponentID ("kid0");
    keptButtons[0].setComponentID ("kept0");
    hoodButton.setComponentID ("hood");
    breed.onClick = [this] { rebuild (true); };
    breed.setTooltip ("Deal a fresh set of kids");
    hoodButton.onClick = [this] { hood.refresh(); hood.setVisible (true); hood.toFront (false); };
    hoodButton.fontSize = 11.f;
    removeMate.onClick = [this] { hasMate = false; rebuild (true); };
    removeMate.fontSize = 10.5f;
    hood.onDone = [this] { hood.setVisible (false); rebuild(); };
    addAndMakeVisible (breed);
    addAndMakeVisible (hoodButton);
    addChildComponent (removeMate);
    addChildComponent (hood);
    startTimer (550);
    rebuild (true);
}

void FamilyView::rebuild (bool newLitter)
{
    if (newLitter) ++litter;
    parent = proc.evo.current();
    auto pOn = proc.evo.activeModules (parent);
    for (int k = 0; k < 6; ++k)
    {
        kids[(size_t) k] = proc.evo.breed (parent, hasMate ? &mate : nullptr, litter, k);
        auto kOn = proc.evo.activeModules (kids[(size_t) k]);
        juce::String note;
        if (hasMate)
        {
            auto mOn = proc.evo.activeModules (mate);
            int fromMate = 0;
            for (int m = 0; m < kNumFx; ++m) if (kOn[(size_t) m] == mOn[(size_t) m] && kOn[(size_t) m] != pOn[(size_t) m]) ++fromMate;
            note = fromMate == 0 ? "mostly parent" : fromMate >= 3 ? "mostly mate" : "mix of both";
        }
        else
        {
            int diff = 0;
            for (int m = 0; m < kNumFx; ++m) if (kOn[(size_t) m] != pOn[(size_t) m]) ++diff;
            note = diff == 0 ? "close copy" : juce::String (diff) + (diff == 1 ? " trait changed" : " traits changed");
        }
        kidButtons[k].set (proc.evo.look (kids[(size_t) k]), proc.evo.name (kids[(size_t) k]), note);
    }

    auto keptList = proc.kept();
    keptGenomes.clear();
    for (int i = (int) keptList.size() - 1; i >= 0 && keptGenomes.size() < 4; --i) keptGenomes.push_back (keptList[(size_t) i].g);
    for (int k = 0; k < 4; ++k)
    {
        bool vis = ! hasMate && (size_t) k < keptGenomes.size();
        keptButtons[k].setVisible (vis);
        if (vis) keptButtons[k].set (proc.evo.look (keptGenomes[(size_t) k]), proc.evo.name (keptGenomes[(size_t) k]), {});
    }
    removeMate.setVisible (hasMate);
    breed.setButtonText (hasMate ? "Breed together" : "Breed new litter");

    // traits
    for (auto* c : traitChips) removeChildComponent (c);
    traitChips.clear();
    traitModules.clear();
    auto tr = proc.evo.traits (parent);
    for (auto& t : tr)
    {
        bool locked = proc.isLocked ((Module) (t.module + 1));
        auto* chip = traitChips.add (new Pill (t.word + "  " + t.effect));
        chip->getProperties().set ("full", t.word + "  " + t.effect);
        chip->getProperties().set ("short", t.effect);
        chip->getProperties().set ("detail", t.detail);
        chip->selected = locked;
        chip->accent = colours::text;
        chip->fontSize = 11.5f;
        chip->lockIcon = true;
        chip->setTooltip ((t.detail.isNotEmpty() ? t.word + " = " + t.effect + " (" + t.detail + "). " : juce::String())
                          + (locked ? "Locked: every kid inherits this. Click to unlock." : "Click to lock: every kid inherits this trait."));
        int m = t.module;
        chip->onClick = [this, m]
        {
            auto mod = (Module) (m + 1);
            proc.setLocked (mod, ! proc.isLocked (mod));
            rebuild();
        };
        addAndMakeVisible (chip);
        traitModules.push_back (t.module);
    }
    int dormant = kNumFx - (int) tr.size();
    dormantText = dormant > 0 ? "+ " + juce::String (dormant) + " dormant" : juce::String();
    layoutTraits();
    hood.toFront (false);
    if (hood.isVisible()) hood.refresh();
    repaint();
}

void FamilyView::layoutTraits()
{
    auto f = Fonts::get().sans (11.5f, 600);
    // two rows of chips fit; with lots of traits, fall back to just the effect names
    for (int compact = 0; compact < 2; ++compact)
    {
        int x = 16, y = 408, rows = 1;
        for (auto* c : traitChips)
        {
            auto label = c->getProperties()[compact ? "short" : "full"].toString();
            if (compact && c->getProperties()["detail"].toString().isNotEmpty())
                label << " " << c->getProperties()["detail"].toString();
            c->setButtonText (label);
            int w = (int) textWidth (f, label) + 40;
            if (x + w > getWidth() - 16) { x = 16; y += 36; ++rows; }
            c->setBounds (x, y, w, 30);
            x += w + 6;
        }
        if (rows <= 2) break;
    }
}

void FamilyView::resized()
{
    hoodButton.setBounds (getWidth() - 16 - 120, 10, 120, 28);
    breed.setBounds (getWidth() / 2 - 76, 231, 152, 30);
    const float colW = (getWidth() - 32) / 6.f;
    for (int k = 0; k < 6; ++k)
    {
        kidButtons[k].spriteSize = 62.f;
        kidButtons[k].setBounds (juce::roundToInt (16 + k * colW + 2), 268, juce::roundToInt (colW - 4), 108);
    }
    const int mateCx = getWidth() / 2 + 115;
    for (int k = 0; k < 4; ++k) keptButtons[k].setBounds (mateCx - 2 * 38 + k * 38, 184, 34, 34);
    removeMate.setBounds (mateCx - 60, 192, 120, 22);
    hood.setBounds (getLocalBounds());
    layoutTraits();
}

void FamilyView::timerCallback()
{
    bob = bob ? 0 : 1;
    repaint (Rectangle<int> (getWidth() / 2 - 190, 40, 160, 160));
}

void FamilyView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 16.f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 16.f, 1.f);

    sectionLabel (g, "FAMILY", 16.f, 17.f);
    g.setColour (colours::muted);
    g.setFont (Fonts::get().mono (10.5f));
    g.drawText ("gen " + juce::String (proc.generation()), 80, 17, 80, 14, juce::Justification::centredLeft);

    const float cx = getWidth() * 0.5f, parentCx = cx - 115.f, mateCx = cx + 115.f;
    // parent
    jackpot::sprite::draw (g, Rectangle<float> (136.f, 136.f).withCentre ({ parentCx, 112.f }), proc.evo.look (parent), bob);
    g.setColour (colours::text);
    g.setFont (Fonts::get().sans (13.f, 700));
    g.drawText (proc.evo.name (parent), Rectangle<float> (220.f, 18.f).withCentre ({ parentCx, 190.f }), juce::Justification::centred);
    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (10.5f));
    g.drawText ("parent", Rectangle<float> (220.f, 14.f).withCentre ({ parentCx, 206.f }), juce::Justification::centred);

    g.setColour (juce::Colour (0xff4a4c53));
    g.setFont (Fonts::get().sans (22.f));
    g.drawText (U8 ("\xc3\x97"), Rectangle<float> (30.f, 30.f).withCentre ({ cx, 112.f }), juce::Justification::centred);

    // mate
    if (hasMate)
    {
        jackpot::sprite::draw (g, Rectangle<float> (108.f, 108.f).withCentre ({ mateCx, 112.f }), proc.evo.look (mate));
        g.setColour (colours::text);
        g.setFont (Fonts::get().sans (13.f, 700));
        g.drawText (proc.evo.name (mate), Rectangle<float> (220.f, 18.f).withCentre ({ mateCx, 178.f }), juce::Justification::centred);
    }
    else
    {
        auto box = Rectangle<float> (108.f, 108.f).withCentre ({ mateCx, 108.f });
        juce::Path p; p.addRoundedRectangle (box, 14.f);
        juce::Path dashed;
        float dashes[] = { 4.f, 4.f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (juce::Colour (0xff33343a));
        g.fillPath (dashed);
        g.setColour (colours::muted);
        g.setFont (Fonts::get().sans (11.f));
        g.drawFittedText ("no mate:\nkids are mutations", box.toNearestInt(), juce::Justification::centred, 2);
        g.drawText (keptGenomes.empty() ? "keep sounds to use them as mates" : "or pick one you kept:",
                    Rectangle<float> (240.f, 14.f).withCentre ({ mateCx, 172.f }), juce::Justification::centred);
    }

    // family tree
    g.setColour (juce::Colour (0xff2a2b30));
    const float colW = (getWidth() - 32) / 6.f;
    g.drawLine (cx, 220.f, cx, 231.f, 1.f);
    g.drawLine (16 + colW * 0.5f, 252.f, 16 + colW * 5.5f, 252.f, 1.f);
    for (int k = 0; k < 6; ++k) g.drawLine (16 + colW * (k + 0.5f), 252.f, 16 + colW * (k + 0.5f), 266.f, 1.f);

    // traits
    g.setColour (colours::line);
    g.drawHorizontalLine (386, 16.f, getWidth() - 16.f);
    sectionLabel (g, "TRAITS", 16.f, 391.f);
    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (11.f));
    g.drawText ("click a trait to lock it: every kid inherits it", getWidth() - 16 - 300, 391, 300, 14, juce::Justification::centredRight);
    if (dormantText.isNotEmpty())
    {
        int x = 16, y = 408;
        if (traitChips.size() > 0) { auto* last = traitChips.getLast(); x = last->getRight() + 10; y = last->getY(); }
        g.setColour (colours::dim);
        g.drawText (dormantText, x, y, 120, 30, juce::Justification::centredLeft);
    }
}

// ===========================================================================
//  Trail
// ===========================================================================
void TrailView::refresh()
{
    items = proc.trail();
    if (items.size() > 11) items.erase (items.begin(), items.end() - 11);
    auto kept = proc.kept();
    keptFlags.clear();
    for (auto& it : items)
    {
        bool k = false;
        for (auto& kk : kept)
        {
            bool same = kk.g.v.size() == it.g.v.size();
            for (size_t i = 0; same && i < it.g.v.size(); ++i) same = std::abs (kk.g.v[i] - it.g.v[i]) < 0.002f;
            if (same) { k = true; break; }
        }
        keptFlags.push_back (k);
    }
    repaint();
}

void TrailView::paint (Graphics& g)
{
    hit.clear();
    float x = 0.f;
    const float cy = getHeight() * 0.5f;
    for (size_t i = 0; i < items.size(); ++i)
    {
        auto box = Rectangle<float> (x, cy - 29.f, 58.f, 58.f);
        g.setColour (colours::panel);
        g.fillRoundedRectangle (box, 10.f);
        g.setColour (keptFlags[i] ? colours::gold : colours::line);
        g.drawRoundedRectangle (box.reduced (keptFlags[i] ? 1.f : 0.5f), 10.f, keptFlags[i] ? 2.f : 1.f);
        jackpot::sprite::draw (g, box.reduced (6.f), proc.evo.look (items[i].g));
        hit.push_back (box);
        x += 64.f;
    }
    auto cur = Rectangle<float> (x, cy - 33.f, 66.f, 66.f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (cur, 12.f);
    g.setColour (colours::text);
    g.drawRoundedRectangle (cur.reduced (1.f), 12.f, 2.f);
    jackpot::sprite::draw (g, cur.reduced (7.f), proc.evo.look (proc.evo.current()));
    if (items.empty())
    {
        g.setColour (colours::dim);
        g.setFont (Fonts::get().sans (11.f));
        g.drawText ("every sound you visit is kept here", (int) cur.getRight() + 14, 0, 300, getHeight(), juce::Justification::centredLeft);
    }
}

void TrailView::mouseUp (const juce::MouseEvent& e)
{
    for (size_t i = 0; i < hit.size(); ++i)
        if (hit[i].contains (e.position)) { proc.goTo (items[i].g); return; }
}

// ===========================================================================
//  Editor
// ===========================================================================
void JackpotEditor::Canvas::paint (Graphics& g)
{
    auto& p = ed.proc;
    g.fillAll (colours::bg);
    // header
    g.setColour (colours::text);
    auto wf = Fonts::get().sans (12.f, 800);
    float x = 20.f;
    for (auto ch : juce::String ("ODDKIN"))
    {
        auto s = juce::String::charToString (ch);
        g.setFont (wf);
        g.drawText (s, Rectangle<float> (x, 20.f, 20.f, 16.f), juce::Justification::centredLeft);
        x += textWidth (wf, s) + 4.f;
    }
    g.setColour (juce::Colour (0xff2a2b30));
    g.fillRect (x + 10.f, 20.f, 1.f, 16.f);

    g.setColour (colours::muted);
    g.setFont (Fonts::get().mono (11.f));
    juce::String origin = p.onMap() ? "R" + juce::String (p.region()) + U8 (" \xc2\xb7 ")
                                          + juce::String (juce::roundToInt (p.apvts.getRawParameterValue ("map_x")->load() * 1000)) + " / "
                                          + juce::String (juce::roundToInt (p.apvts.getRawParameterValue ("map_y")->load() * 1000))
                                    : U8 ("bred \xc2\xb7 gen ") + juce::String (p.generation());
    g.drawText (origin, ed.nameButton.getRight() + 6, 20, 200, 16, juce::Justification::centredLeft);

    g.setColour (colours::muted);
    g.setFont (Fonts::get().sans (11.5f));
    g.drawText (U8 ("Drag or click to explore. Close spots sound alike; the map never ends."), 20, 520, 470, 16, juce::Justification::centredLeft);

    // footer
    g.setColour (juce::Colour (0xff1b1c20));
    g.fillRect (20, 552, baseW - 40, 1);
    float lx = 20.f;
    auto lf = Fonts::get().sans (10.5f, 600);
    g.setColour (colours::muted);
    for (auto ch : juce::String ("TRAIL"))
    {
        auto s = juce::String::charToString (ch);
        g.setFont (lf);
        g.drawText (s, Rectangle<float> (lx, 620.f, 20.f, 14.f), juce::Justification::centredLeft);
        lx += textWidth (lf, s) + 1.3f;
    }
    g.setColour (colours::dim);
    g.setFont (Fonts::get().mono (10.f));
    g.drawText (juce::String (p.trail().size()) + " visited", 20, 636, 70, 14, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff1b1c20));
    g.fillRect (918, 600, 1, 64);
    g.setColour (juce::Colour (0xffa9acb3));
    g.setFont (Fonts::get().sans (9.5f, 600));
    g.drawText ("AUTO LVL", 1018, 646, 62, 14, juce::Justification::centred);
}

JackpotEditor::JackpotEditor (JackpotProcessor& p)
    : AudioProcessorEditor (p), proc (p), map (p), family (p), trail (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (canvas);
    canvas.setSize (baseW, baseH);
    for (juce::Component* c : { (juce::Component*) &map, (juce::Component*) &family, (juce::Component*) &trail,
                                (juce::Component*) &back, (juce::Component*) &keep, (juce::Component*) &saveAs, (juce::Component*) &nameButton })
        canvas.addAndMakeVisible (c);

    mixKnob = std::make_unique<Knob> (proc, "mix", "MIX", colours::soft);
    outKnob = std::make_unique<Knob> (proc, "out_gain", "OUTPUT", colours::soft);
    canvas.addAndMakeVisible (*mixKnob);
    canvas.addAndMakeVisible (*outKnob);
    canvas.addAndMakeVisible (autoLevel);
    autoAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "autolevel", autoLevel);
    autoLevel.setTooltip ("Keeps wild sounds near your guitar's volume");

    back.onClick = [this] { proc.back(); refreshAll(); };
    back.setTooltip ("Go back to the previous sound");
    keep.onClick = [this] { proc.keepCurrent(); refreshAll(); };
    keep.setTooltip ("Keep this sound: it's saved to your presets and can be used as a mate");
    saveAs.onClick = [this] { askToSave(); };
    nameButton.onClick = [this] { showPresetMenu(); };
    nameButton.fontSize = 15.f;
    nameButton.bright = true;
    nameButton.setTooltip ("Your saved sounds");

    map.setBounds (20, 56, 470, 456);
    family.setBounds (506, 56, 574, 482);
    trail.setBounds (100, 566, 800, 140);
    mixKnob->setBounds (934, 598, 48, 62);
    outKnob->setBounds (980, 598, 48, 62);
    autoLevel.setBounds (1030, 612, 38, 30);

    setResizable (true, true);
    setResizeLimits (baseW * 6 / 10, baseH * 6 / 10, baseW * 16 / 10, baseH * 16 / 10);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    setSize (baseW, baseH);

    refreshAll();
    startTimerHz (15);
}

JackpotEditor::~JackpotEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void JackpotEditor::resized()
{
    canvas.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
}

void JackpotEditor::refreshAll()
{
    auto name = proc.getPatchName();
    nameButton.setButtonText (name);
    int nw = (int) textWidth (Fonts::get().sans (15.f, 600), name) + 8;
    nameButton.setBounds (150, 14, nw, 28);

    bool kept = proc.isCurrentKept();
    keep.setButtonText (kept ? U8 ("\xe2\x98\x85  Kept") : U8 ("\xe2\x98\x86  Keep"));
    keep.selected = kept;
    keep.accent = colours::gold;
    back.setEnabled (proc.canUndo());

    saveAs.setBounds (baseW - 20 - 86, 12, 86, 32);
    keep.setBounds (saveAs.getX() - 8 - 84, 12, 84, 32);
    back.setBounds (keep.getX() - 8 - 80, 12, 80, 32);

    map.refresh();
    family.rebuild();
    trail.refresh();
    canvas.repaint();
    lastChange = proc.changeCounter.load();
}

void JackpotEditor::timerCallback()
{
    if (proc.changeCounter.load() != lastChange) refreshAll();
}

void JackpotEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Starting points");
    auto fac = proc.presets.factoryNames();
    for (int i = 0; i < fac.size(); ++i) m.addItem (1 + i, fac[i]);
    m.addSectionHeader ("Saved");
    auto users = proc.presets.userPresets();
    if (users.isEmpty()) m.addItem (999, "Nothing saved yet: press Keep", false);
    for (int i = 0; i < users.size(); ++i) m.addItem (100 + i, users[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addItem (2000, U8 ("Save as\xe2\x80\xa6"));
    m.addItem (2001, "Show presets folder");

    juce::Component::SafePointer<JackpotEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameButton).withMinimumWidth (240),
                     [safe, users] (int result)
                     {
                         if (safe == nullptr || result == 0) return;
                         auto& self = *safe;
                         if (result >= 1 && result < 100) self.proc.presets.loadFactory (result - 1);
                         else if (result >= 100 && result < 100 + users.size()) self.proc.presets.loadFile (users[result - 100]);
                         else if (result == 2000) self.askToSave();
                         else if (result == 2001) jackpot::PresetManager::folder().revealToUser();
                         self.proc.apvts.state.setProperty ("onMap", false, nullptr);
                         self.refreshAll();
                     });
}

void JackpotEditor::askToSave()
{
    auto* w = new juce::AlertWindow ("Save sound", "Name it:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&lnf);
    w->addTextEditor ("name", proc.getPatchName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<JackpotEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty()) return;
        if (! safe->proc.presets.save (name))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", "Check that Documents/ODDKIN/Presets is writable.");
        safe->refreshAll();
    }), true);
}
