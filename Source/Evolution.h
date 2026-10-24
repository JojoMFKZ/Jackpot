#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include "Params.h"

class JackpotProcessor;

namespace jackpot
{
// A genome is a full patch: one normalised (0..1) value per parameter.
struct Genome
{
    std::vector<float> v;
    bool valid() const { return ! v.empty(); }
    juce::String encode() const;
    static Genome decode (const juce::String& s);
};

// Everything the pixel creature needs, derived from the real effect settings.
struct Look
{
    std::array<bool, kNumFx> on {};
    float fuzz = 0, crush = 0, pitch = 0, tape = 0, delay = 0, verb = 0, harmony = 0, reso = 0;
    int filterType = -1;   // -1 = filter off, 0 LP, 1 BP, 2 HP, 3 notch
    bool stutter = false;
    float hue = 0, body = .5f, form = .5f, form2 = .5f;
};

struct Trait
{
    int module;            // 0..9 (fuzz..reverb, harmony, resonator)
    juce::String word;     // "spiky hair"
    juce::String effect;   // "fuzz"
    juce::String detail {}; // "Major", "E Minor"
};

// The map, breeding and the creature descriptions.
class Evolution
{
public:
    explicit Evolution (JackpotProcessor& p) : proc (p) {}

    Genome current() const;                                   // what the knobs are now
    Genome at (int region, float x, float y) const;           // a point on the endless map
    Genome breed (const Genome& parent, const Genome* mate, juce::uint64 litter, int kid) const;
    Genome withLocks (const Genome& g) const;                 // locked effects keep their current values
    void apply (const Genome& g);                             // write to the parameters (respects locks)

    Look look (const Genome& g) const;
    juce::String name (const Genome& g) const;
    std::vector<Trait> traits (const Genome& g) const;
    std::array<bool, kNumFx> activeModules (const Genome& g) const;

    // map preview colour for a genome
    juce::Colour mapColour (const Genome& g) const;

    float chaos() const;

private:
    void sanitize (Genome& g, float chaos, float rollA, float rollB) const;
    int index (const char* id) const;
    JackpotProcessor& proc;
};

// Draws a creature into the given square. 'bob' shifts it by whole pixels.
namespace sprite
{
void draw (juce::Graphics& g, juce::Rectangle<float> area, const Look& look, int bob = 0, float alpha = 1.f);
}
} // namespace jackpot
