#pragma once
#include <juce_core/juce_core.h>
#include "Params.h"

class JackpotProcessor;

namespace jackpot
{
// The dice. Every roll gets a fresh 64-bit seed (time + entropy + counter),
// so two rolls never produce the same patch. A seed can be typed back in to
// regenerate the same patch with the same chaos & locks.
class Randomizer
{
public:
    explicit Randomizer (JackpotProcessor& p) : proc (p) {}

    juce::uint64 randomize();                       // brand-new patch
    juce::uint64 mutate();                          // nudge the current one
    void fromSeed (juce::uint64 seed);              // deterministic full patch
    void randomizeModule (Module m);                // re-roll a single card

    static juce::uint64 freshSeed();
    static juce::String seedToString (juce::uint64 seed);
    static bool stringToSeed (const juce::String& text, juce::uint64& out);

private:
    void generate (juce::uint64 seed, bool full, int onlyModule);
    void setNorm (int index, float norm);

    JackpotProcessor& proc;
};
} // namespace jackpot
