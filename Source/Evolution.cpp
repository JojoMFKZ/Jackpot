#include "Evolution.h"
#include "PluginProcessor.h"
#include <cmath>
#include <unordered_map>

namespace jackpot
{
namespace
{
double hash3 (double a, double b, double c)
{
    double x = std::sin (a * 127.1 + b * 311.7 + c * 74.7) * 43758.5453;
    return x - std::floor (x);
}

double smooth (double t) { return t * t * (3.0 - 2.0 * t); }

double noise2 (double x, double y, double ch)
{
    double xi = std::floor (x), yi = std::floor (y);
    double xf = smooth (x - xi), yf = smooth (y - yi);
    double a = hash3 (xi, yi, ch), b = hash3 (xi + 1, yi, ch), c = hash3 (xi, yi + 1, ch), d = hash3 (xi + 1, yi + 1, ch);
    return a + (b - a) * xf + (c - a) * yf + (a - b - c + d) * xf * yf;
}

float clamp01 (double v) { return (float) juce::jlimit (0.0, 1.0, v); }
float fract (double v) { return (float) (v - std::floor (v)); }

float baseOnProb (Module m)
{
    switch (m)
    {
        case Module::Fuzz:    return 0.55f;
        case Module::Crush:   return 0.38f;
        case Module::Pitch:   return 0.38f;
        case Module::Filter:  return 0.5f;
        case Module::Tape:    return 0.55f;
        case Module::Stutter: return 0.3f;
        case Module::Delay:   return 0.6f;
        case Module::Verb:    return 0.6f;
        default:              return 0.5f;
    }
}

bool isOnParam (const ParamSpec& s) { return juce::String (s.id).endsWith ("_on"); }

const char* kAdj[] = { "Glass", "Velvet", "Rust", "Sugar", "Static", "Hollow", "Molten", "Paper",
                       "Feral", "Silver", "Broken", "Liquid", "Dust", "Neon", "Frozen", "Sour" };
const char* kNoun[] = { "Choir", "Engine", "Halo", "Swarm", "Organ", "Tide", "Wire", "Moth",
                        "Bell", "Signal", "Ghost", "Cathedral", "Hornet", "Cassette", "Prism", "Lung" };
} // namespace

// ---------------------------------------------------------------------------
juce::String Genome::encode() const
{
    juce::StringArray parts;
    for (auto x : v) parts.add (juce::String (x, 6));
    return parts.joinIntoString (",");
}

Genome Genome::decode (const juce::String& s)
{
    Genome g;
    for (auto& t : juce::StringArray::fromTokens (s, ",", ""))
        if (t.isNotEmpty()) g.v.push_back (t.getFloatValue());
    return g;
}

// ---------------------------------------------------------------------------
int Evolution::index (const char* id) const
{
    static std::unordered_map<std::string, int> cache;
    auto it = cache.find (id);
    if (it != cache.end()) return it->second;
    auto& specs = paramSpecs();
    for (size_t i = 0; i < specs.size(); ++i)
        if (! std::strcmp (specs[i].id, id)) { cache[id] = (int) i; return (int) i; }
    jassertfalse;
    return 0;
}

float Evolution::chaos() const { return proc.apvts.getRawParameterValue ("chaos")->load(); }

Genome Evolution::current() const
{
    Genome g;
    for (int i = 0; i < proc.numParams(); ++i) g.v.push_back (proc.param (i)->getValue());
    return g;
}

Genome Evolution::at (int region, float x, float y) const
{
    auto& specs = paramSpecs();
    Genome g = current();
    const float c = chaos();
    const double scale = 1.6 + c * 6.0;
    const double X = x * scale + region * 31.7, Y = y * scale + region * 17.3;

    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        if (s.rnd == Rnd::Never) continue;
        double raw = noise2 (X, Y, (double) i + region * 0.1) * 0.72 + noise2 (X * 2.3, Y * 2.3, (double) i + 40.0) * 0.28;
        double e = juce::jlimit (0.0, 1.0, 0.5 + (raw - 0.5) * 2.6);
        float v;
        if (s.kind == Kind::Bool)
        {
            if (isOnParam (s)) v = e > 1.0 - baseOnProb (s.module) * (0.65 + 0.7 * c) ? 1.f : 0.f;
            else               v = e > 0.965 - 0.06 * c ? 1.f : 0.f;   // freeze: rare
        }
        else if (s.kind == Kind::Choice)
        {
            v = (float) (std::round (e * 23.0) / 23.0);
        }
        else if (s.rnd == Rnd::Gentle) v = clamp01 (0.5 + (e - 0.5) * (0.45 + 0.55 * c));
        else if (s.rnd == Rnd::Low)    v = clamp01 (std::pow (e, 2.4 - 1.4 * c));
        else
        {
            double d = (e - 0.5) * 2.0;
            v = clamp01 (0.5 + std::copysign (std::pow (std::abs (d), 1.0 - 0.5 * c), d) * 0.5);
        }
        g.v[i] = v;
    }
    sanitize (g, c, noise2 (X, Y, 90.0), noise2 (X, Y, 91.0));
    return g;
}

void Evolution::sanitize (Genome& g, float c, float rollA, float rollB) const
{
    auto P = [&] (const char* id) { return proc.param (index (id)); };
    auto real = [&] (const char* id) { return P (id)->convertFrom0to1 (g.v[(size_t) index (id)]); };
    auto setReal = [&] (const char* id, float val) { g.v[(size_t) index (id)] = P (id)->convertTo0to1 (val); };
    auto n = [&] (const char* id) -> float& { return g.v[(size_t) index (id)]; };

    if (rollA < 1.f - 0.65f * c)
    {
        static const float iv[] = { -24.f, -12.f, -7.f, -5.f, 5.f, 7.f, 12.f, 19.f, 24.f };
        float s = real ("pitch_semi"), best = iv[0];
        for (auto x : iv) if (std::abs (x - s) < std::abs (best - s)) best = x;
        setReal ("pitch_semi", best);
    }
    if (real ("filt_morph") < 0.6f && real ("filt_cutoff") < 250.f && real ("filt_env") < 0.3f)
        setReal ("filt_env", 0.4f + 0.5f * rollB);
    if (real ("dly_fb") > 0.92f) n ("dly_mix") = juce::jmin (n ("dly_mix"), 0.55f);
    if (n ("verb_freeze") > 0.5f) n ("verb_mix") = juce::jmin (n ("verb_mix"), 0.5f);
    if (real ("crush_bits") < 2.5f) setReal ("crush_bits", 3.f + rollB * 6.f);

    int onCount = 0;
    for (int m = 1; m <= kNumFx; ++m) if (g.v[(size_t) index (onParamId ((Module) m).toRawUTF8())] > 0.5f) ++onCount;
    static const Module pref[] = { Module::Tape, Module::Delay, Module::Verb, Module::Fuzz };
    for (int k = 0; onCount < 2 && k < 4; ++k)
    {
        auto& v = g.v[(size_t) index (onParamId (pref[(k + (int) (rollB * 4)) % 4]).toRawUTF8())];
        if (v < 0.5f) { v = 1.f; ++onCount; }
    }
}

Genome Evolution::breed (const Genome& parent, const Genome* mate, juce::uint64 litter, int kid) const
{
    auto& specs = paramSpecs();
    Genome q = parent;
    const float c = chaos();
    const double L = (double) (litter % 100000);

    if (mate != nullptr && mate->v.size() == q.v.size())
    {
        for (int m = 1; m <= kNumFx; ++m)
            if (hash3 (L + 3.0, kid, m) < 0.5)
                for (size_t i = 0; i < specs.size(); ++i)
                    if ((int) specs[i].module == m) q.v[i] = mate->v[i];
        for (size_t i = 0; i < specs.size(); ++i)
            if (specs[i].module == Module::Global && specs[i].rnd != Rnd::Never && hash3 (L, kid, (double) i + 7) < 0.5)
                q.v[i] = mate->v[i];
    }

    const double amt = 0.05 + 0.16 * c;
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        if (s.rnd == Rnd::Never) continue;
        double h = hash3 (L * 7.0 + kid, (double) i, 11.0);
        if (s.kind == Kind::Bool)
        {
            double flip = isOnParam (s) ? 0.06 + 0.12 * c : (q.v[i] > 0.5f ? 0.3 : 0.02 * c);
            if (h < flip) q.v[i] = q.v[i] > 0.5f ? 0.f : 1.f;
        }
        else if (s.kind == Kind::Choice)
        {
            if (h < 0.12 * c) q.v[i] = (float) (std::round (hash3 (L, kid, 99) * 23.0) / 23.0);
        }
        else
        {
            q.v[i] = clamp01 (q.v[i] + (h - 0.5) * 2.0 * amt * (s.rnd == Rnd::Low ? 0.5 : 1.0));
        }
    }
    sanitize (q, c, (float) hash3 (L, kid, 1), (float) hash3 (L, kid, 2));
    return withLocks (q);
}

Genome Evolution::withLocks (const Genome& g) const
{
    auto& specs = paramSpecs();
    Genome q = g;
    for (size_t i = 0; i < specs.size() && i < q.v.size(); ++i)
        if (specs[i].module != Module::Global && proc.isLocked (specs[i].module))
            q.v[i] = proc.param ((int) i)->getValue();
    return q;
}

void Evolution::apply (const Genome& g)
{
    auto& specs = paramSpecs();
    if (g.v.size() != specs.size()) return;
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        if (s.rnd == Rnd::Never) continue;
        if (s.module != Module::Global && proc.isLocked (s.module)) continue;
        auto* p = proc.param ((int) i);
        if (std::abs (p->getValue() - g.v[i]) > 1e-5f)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (g.v[i]);
            p->endChangeGesture();
        }
    }
    ++proc.changeCounter;
}

std::array<bool, kNumFx> Evolution::activeModules (const Genome& g) const
{
    std::array<bool, kNumFx> on {};
    for (int m = 1; m <= kNumFx; ++m)
        on[(size_t) m - 1] = g.v[(size_t) index (onParamId ((Module) m).toRawUTF8())] > 0.5f;
    return on;
}

Look Evolution::look (const Genome& g) const
{
    Look L;
    if (! g.valid()) return L;
    auto val = [&] (const char* id) { return g.v[(size_t) index (id)]; };
    auto real = [&] (const char* id) { return proc.param (index (id))->convertFrom0to1 (val (id)); };
    L.on = activeModules (g);
    L.fuzz = L.on[0] ? val ("fuzz_sustain") * 0.8f + 0.2f * val ("fuzz_octave") : 0.f;
    L.crush = L.on[1] ? clamp01 ((1.0 - val ("crush_bits")) * 0.6 + val ("crush_rate") * 0.4 + 0.15) : 0.f;
    L.pitch = L.on[2] ? juce::jlimit (-1.f, 1.f, real ("pitch_semi") / 24.f * (0.4f + 0.6f * val ("pitch_mix")) * 1.6f) : 0.f;
    L.filterType = L.on[3] ? juce::jlimit (0, 3, (int) real ("filt_morph")) : -1;
    L.tape = L.on[4] ? juce::jmax (val ("tape_age"), val ("tape_wow")) : 0.f;
    L.stutter = L.on[5];
    L.delay = L.on[6] ? clamp01 (val ("dly_fb") * 0.7 + val ("dly_mix") * 0.5) : 0.f;
    L.verb = L.on[7] ? clamp01 (val ("verb_size") * 0.6 + val ("verb_mix") * 0.6) : 0.f;
    L.hue = fract (0.47 * val ("filt_cutoff") + 0.31 * val ("dly_time") + 0.22 * val ("crush_ringfreq") + 0.17 * val ("fuzz_tone") + 0.11 * val ("tape_flutter"));
    L.body = val ("fuzz_tone") * 0.5f + val ("verb_width") * 0.5f;
    L.form = fract (val ("stut_size") * 0.7 + val ("filt_reso") * 0.6 + val ("crush_fold") * 0.3);
    L.form2 = fract (val ("pitch_grain") * 0.8 + val ("tape_sat") * 0.5 + val ("dly_warble") * 0.4);
    return L;
}

juce::String Evolution::name (const Genome& g) const
{
    auto L = look (g);
    int a = juce::jlimit (0, 15, (int) (fract (L.form * 1.7 + L.hue * 0.3) * 16.f));
    int b = juce::jlimit (0, 15, (int) (fract (L.form2 * 1.9 + L.body * 0.4) * 16.f));
    return juce::String (kAdj[a]) + " " + kNoun[b];
}

std::vector<Trait> Evolution::traits (const Genome& g) const
{
    std::vector<Trait> t;
    if (! g.valid()) return t;
    auto L = look (g);
    auto val = [&] (const char* id) { return g.v[(size_t) index (id)]; };
    if (L.on[0]) t.push_back ({ 0, L.fuzz > 0.6f ? "wild hair" : "spiky hair", "fuzz" });
    if (L.on[1]) t.push_back ({ 1, "glitchy skin", "crush" });
    if (L.on[2]) t.push_back ({ 2, L.pitch >= 0 ? "tall & wide-eyed" : "short & sleepy", "pitch" });
    if (L.on[3]) { static const char* m[] = { "flat mouth", "round mouth", "wide mouth", "zigzag mouth" }; t.push_back ({ 3, m[juce::jmax (0, L.filterType)], "filter" }); }
    if (L.on[4]) t.push_back ({ 4, "faded & wobbly", "tape" });
    if (L.on[5]) t.push_back ({ 5, "third eye", "stutter" });
    if (L.on[6]) t.push_back ({ 6, val ("dly_fb") > 0.5f ? "long ghost trail" : "ghost trail", "delay" });
    if (L.on[7]) t.push_back ({ 7, L.verb > 0.6f ? "big halo" : "halo", "reverb" });
    return t;
}

juce::Colour Evolution::mapColour (const Genome& g) const
{
    // only smooth, continuous parameters here so the terrain reads as gradients
    auto val = [&] (const char* id) { return g.v[(size_t) index (id)]; };
    float hue = fract (0.55 * val ("filt_cutoff") + 0.35 * val ("dly_time") + 0.25 * val ("crush_ringfreq") + 0.15 * val ("fuzz_tone"));
    float sat = 0.32f + 0.36f * (val ("verb_size") * 0.5f + val ("tape_wow") * 0.5f);
    float lum = 0.08f + 0.2f * (val ("filt_reso") + val ("dly_fb") + val ("fuzz_sustain")) / 3.f;
    return juce::Colour::fromHSL (hue, sat, lum, 1.f);
}

// ===========================================================================
//  Pixel creature
// ===========================================================================
namespace sprite
{
void draw (juce::Graphics& gr, juce::Rectangle<float> area, const Look& l, int bob, float alpha)
{
    constexpr int G = 20;
    std::array<int, G * G> g {};
    auto set = [&] (int x, int y, int v, bool onlyEmpty = false)
    {
        if (x < 0 || x >= G || y < 0 || y >= G) return;
        if (onlyEmpty && g[(size_t) (y * G + x)]) return;
        g[(size_t) (y * G + x)] = v;
    };
    auto setM = [&] (int x, int y, int v, bool oe = false) { set (x, y, v, oe); set (G - 1 - x, y, v, oe); };
    auto H = [] (double a, double b, double c) { return hash3 (a, b, c); };

    const int hw = 3 + (int) std::round (l.body * 3.f);
    const int top = juce::jlimit (5, 11, 9 - (int) std::round (l.pitch * 3.f));
    const int bottom = 16;
    for (int y = top; y <= bottom; ++y)
    {
        double t = (double) (y - top) / (bottom - top);
        int w = (int) std::round (hw * (0.5 + 0.5 * std::sin (juce::MathConstants<double>::pi * juce::jmin (1.0, t * 0.9 + 0.12))))
                + (H (l.form * 9.0, y, 3) < 0.3 ? 1 : 0);
        w = juce::jlimit (1, 8, w);
        for (int x = 10 - w; x <= 9; ++x) setM (x, y, y > top + (bottom - top) * 0.62 ? 2 : 1);
    }
    const int lx = 10 - juce::jmax (2, hw - 1);
    setM (lx, 17, 2); setM (lx, 18, 2);

    const int ey = top + 2, ex = 10 - juce::jmax (2, (int) std::round (hw * 0.55));
    if (l.pitch > 0.3f)       { setM (ex, ey, 4); setM (ex - 1, ey, 4); setM (ex, ey + 1, 5); setM (ex - 1, ey + 1, 4); }
    else if (l.pitch < -0.3f) { setM (ex, ey + 1, 5); setM (ex - 1, ey + 1, 4); }
    else                      { setM (ex, ey, 4); setM (ex, ey + 1, 5); }
    if (l.stutter) { set (9, top + 1, 4); set (10, top + 1, 5); }

    const int my = juce::jmin (bottom - 2, ey + 3);
    switch (l.filterType)
    {
        case -1: setM (9, my, 6); break;
        case 0:  setM (8, my, 6); setM (9, my, 6); break;
        case 1:  setM (9, my, 6); setM (9, my + 1, 6); break;
        case 2:  setM (8, my, 6); setM (9, my, 6); setM (8, my + 1, 6); setM (9, my + 1, 9); break;
        default: setM (7, my, 6); setM (8, my + 1, 6); setM (9, my, 6); break;
    }

    int hairH = 0;
    if (l.fuzz > 0.05f)
    {
        int n = 2 + (int) std::round (l.fuzz * 3.f);
        for (int i = 0; i < n; ++i)
        {
            int x = juce::jmax (10 - hw + 1, 9 - i * 2);
            int len = 1 + (int) std::round (l.fuzz * 3.f * (0.4 + 0.6 * H (l.form2 * 7.0, i, 5)));
            hairH = juce::jmax (hairH, len);
            for (int k = 0; k < len; ++k) setM (x, top - 1 - k, 3);
        }
    }
    for (int y = 0; y < G; ++y)
        for (int x = 0; x < G; ++x)
        {
            auto& v = g[(size_t) (y * G + x)];
            if ((v == 1 || v == 2) && H (x * 1.3, y * 1.7, l.form * 50.0 + 1.0) < l.crush * 0.12) v = 7;
        }
    if (l.verb > 0.05f)
    {
        int hy = juce::jmax (0, top - 3 - hairH), hwid = 2 + (int) std::round (l.verb * 4.f);
        for (int x = 10 - hwid; x <= 9; ++x) setM (x, x == 10 - hwid ? hy + 1 : hy, 8, true);
    }
    auto solid = [] (int v) { return v != 0 && v != 8 && v != 10; };
    std::array<int, G * G> outline = g;
    for (int y = 0; y < G; ++y)
        for (int x = 0; x < G; ++x)
        {
            if (g[(size_t) (y * G + x)]) continue;
            static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (auto& dd : d)
            {
                int X = x + dd[0], Y = y + dd[1];
                if (X >= 0 && X < G && Y >= 0 && Y < G && solid (g[(size_t) (Y * G + X)])) { outline[(size_t) (y * G + x)] = 10; break; }
            }
        }
    g = outline;

    const float hue = l.hue, sat = 0.62f - l.tape * 0.34f, L = 0.60f - l.tape * 0.06f;
    auto hsl = [&] (float h, float s, float li) { return juce::Colour::fromHSL (fract (h), juce::jlimit (0.f, 1.f, s), juce::jlimit (0.f, 1.f, li), alpha); };
    juce::Colour col[11] = {
        {}, hsl (hue, sat, L), hsl (hue, sat, L - 0.15f), hsl (hue + 160.f / 360.f, 0.72f - l.tape * 0.3f, 0.64f),
        juce::Colour (0xfff4f1e8).withMultipliedAlpha (alpha), juce::Colour (0xff0c0c0e).withMultipliedAlpha (alpha),
        hsl (hue, 0.4f, 0.14f), hsl (hue + 190.f / 360.f, 0.85f, 0.64f), juce::Colour (0xffffd86b).withMultipliedAlpha (alpha * 0.9f),
        juce::Colour (0xfff4f1e8).withMultipliedAlpha (alpha), hsl (hue, 0.3f, 0.08f)
    };

    auto rowShift = [&] (int y)
    {
        int s = (l.tape > 0.35f && y % 3 == 0) ? 1 : 0;
        if (l.crush > 0.15f && H (y, l.form * 30.0, 77) < l.crush * 0.35) s += H (y, 2, l.form) < 0.5 ? -2 : 2;
        return s;
    };

    const float px = std::floor (juce::jmin (area.getWidth(), area.getHeight()) / G * 4.f) / 4.f;
    const float ox = std::round (area.getCentreX() - px * G * 0.5f), oy = std::round (area.getCentreY() - px * G * 0.5f) - bob * px;
    auto cell = [&] (int x, int y, float dx) { return juce::Rectangle<float> (ox + (x + dx) * px, oy + y * px, px + 0.35f, px + 0.35f); };

    // delay: ghost echoes behind
    if (l.delay > 0.05f)
    {
        int dx = 1 + (int) std::round (l.delay * 2.f);
        auto ghost = hsl (hue, sat, L + 0.1f);
        for (int pass = (l.delay > 0.5f ? 2 : 1); pass >= 1; --pass)
        {
            gr.setColour (ghost.withMultipliedAlpha (pass == 2 ? 0.16f : 0.32f));
            for (int y = 0; y < G; ++y)
                for (int x = 0; x < G; ++x)
                {
                    int v = g[(size_t) (y * G + x)];
                    if (v == 1 || v == 2 || v == 3 || v == 7) gr.fillRect (cell (x, y, (float) (rowShift (y) + dx * pass)));
                }
        }
    }
    static const int order[] = { 10, 1, 2, 7, 3, 8, 4, 5, 6, 9 };
    for (int v : order)
    {
        gr.setColour (col[v]);
        for (int y = 0; y < G; ++y)
            for (int x = 0; x < G; ++x)
                if (g[(size_t) (y * G + x)] == v) gr.fillRect (cell (x, y, (float) rowShift (y)));
    }
}
} // namespace sprite
} // namespace jackpot
