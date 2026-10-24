#include "Randomizer.h"
#include "PluginProcessor.h"
#include <random>
#include <chrono>
#include <atomic>
#include <cstring>

namespace jackpot
{
namespace
{
// small, portable, deterministic RNG (same results on mac + windows)
struct Rng
{
    juce::uint64 s;
    explicit Rng (juce::uint64 seed) : s (seed ^ 0x9E3779B97F4A7C15ull) { next(); next(); }
    juce::uint64 next()
    {
        juce::uint64 z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    float uni() { return (float) ((next() >> 40) * (1.0 / 16777216.0)); }   // [0,1)
    int range (int n) { return (int) (uni() * (float) n) % juce::jmax (1, n); }
    bool chance (float p) { return uni() < p; }
    float gauss()
    {
        float u1 = juce::jmax (1e-7f, uni()), u2 = uni();
        return std::sqrt (-2.f * std::log (u1)) * std::cos (6.2831853f * u2);
    }
    template <typename T, size_t N> const T& pick (const T (&arr)[N]) { return arr[(size_t) range ((int) N)]; }
};

const char* adjectives[] = {
    "Glass", "Velvet", "Rust", "Sugar", "Static", "Hollow", "Molten", "Paper", "Feral", "Silver", "Broken", "Liquid", "Dust", "Neon", "Frozen", "Sour"
};
const char* nouns[] = {
    "Choir", "Engine", "Halo", "Swarm", "Organ", "Tide", "Wire", "Moth", "Bell", "Signal", "Ghost", "Cathedral", "Hornet", "Cassette", "Prism", "Lung"
};
const char* starSigns[] = {
    "Bat's Wing", "Moth Eye", "Broken Fork", "Third Antenna", "Velvet Fang", "Dog's Ear", "Wet Match",
    "Crooked Halo", "Paper Crown", "Static Hare", "Sleeping Wasp", "Lantern Jaw", "Cat's Cradle",
    "Rusty Key", "Tin Moon", "Sour Plum", "Hollow Bell", "Spilled Ink"
};

juce::String makeName (Rng& r)
{
    switch (r.range (5))
    {
        case 0:  return juce::String (r.pick (nouns)) + " of the " + r.pick (nouns);
        case 1:  return juce::String ("The ") + r.pick (adjectives) + " " + r.pick (nouns);
        case 2:  return juce::String (r.pick (adjectives)) + " " + r.pick (nouns) + " '" + juce::String (70 + r.range (30));
        default: return juce::String (r.pick (adjectives)) + " " + r.pick (nouns);
    }
}

// base probability of each module being switched on by a full roll
float baseOnProb (Module m)
{
    switch (m)
    {
        case Module::Fuzz:    return 0.6f;
        case Module::Crush:   return 0.4f;
        case Module::Pitch:   return 0.4f;
        case Module::Filter:  return 0.5f;
        case Module::Tape:    return 0.6f;
        case Module::Stutter: return 0.3f;
        case Module::Delay:   return 0.6f;
        case Module::Verb:    return 0.6f;
        case Module::Harmony: return 0.3f;
        case Module::Resonator: return 0.28f;
        default:              return 0.5f;
    }
}

float sampleNorm (Rng& r, Rnd type, float chaos)
{
    float u = r.uni();
    switch (type)
    {
        case Rnd::Gentle:
            return 0.5f + (u - 0.5f) * (0.45f + 0.55f * chaos);
        case Rnd::Low:
            return std::pow (u, 2.4f - 1.4f * chaos);
        case Rnd::Normal:
        default:
            if (r.chance (0.25f + 0.5f * chaos))
            {
                // push toward the extremes; more chaos = more extreme
                float d = (u - 0.5f) * 2.f;
                float e = std::pow (std::abs (d), 1.f - 0.65f * chaos);
                return 0.5f + std::copysign (e, d) * 0.5f;
            }
            return u;
    }
}
} // namespace

juce::uint64 Randomizer::freshSeed()
{
    static std::atomic<juce::uint64> counter { 0 };
    std::random_device rd;
    juce::uint64 a = ((juce::uint64) rd() << 32) ^ rd();
    juce::uint64 t = (juce::uint64) std::chrono::high_resolution_clock::now().time_since_epoch().count();
    juce::uint64 c = ++counter * 0xD1B54A32D192ED03ull;
    Rng mix (a ^ t ^ c);
    return mix.next() & 0xFFFFFFFFFFFFull; // 48 bits -> 12 hex digits on the ID card
}

juce::String Randomizer::seedToString (juce::uint64 seed)
{
    auto h = juce::String::toHexString ((juce::int64) seed).toUpperCase().paddedLeft ('0', 12);
    return h.substring (0, 4) + "-" + h.substring (4, 8) + "-" + h.substring (8, 12);
}

bool Randomizer::stringToSeed (const juce::String& text, juce::uint64& out)
{
    auto clean = text.toUpperCase().retainCharacters ("0123456789ABCDEF");
    if (clean.isEmpty() || clean.length() > 12) return false;
    out = (juce::uint64) clean.getHexValue64();
    return true;
}

void Randomizer::setNorm (int index, float norm)
{
    auto* p = proc.param (index);
    p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, norm));
    p->endChangeGesture();
}

juce::uint64 Randomizer::randomize()
{
    auto seed = freshSeed();
    proc.pushHistory();
    generate (seed, true, -1);
    return seed;
}

void Randomizer::fromSeed (juce::uint64 seed)
{
    proc.pushHistory();
    generate (seed, true, -1);
}

juce::uint64 Randomizer::mutate()
{
    auto seed = freshSeed();
    proc.pushHistory();
    generate (seed, false, -1);
    return seed;
}

void Randomizer::randomizeModule (Module m)
{
    proc.pushHistory();
    generate (freshSeed(), true, (int) m);
}

void Randomizer::generate (juce::uint64 seed, bool full, int onlyModule)
{
    auto& specs = paramSpecs();
    Rng r (seed);
    const float chaos = proc.apvts.getRawParameterValue ("chaos")->load();

    auto allowed = [&] (Module m)
    {
        if (onlyModule >= 0) return (int) m == onlyModule;
        if (m == Module::Global) return true;
        return ! proc.isLocked (m);
    };

    std::vector<float> norm ((size_t) proc.numParams());
    for (int i = 0; i < proc.numParams(); ++i) norm[(size_t) i] = proc.param (i)->getValue();

    // ---------------------------------------------------------------- on / off
    std::vector<int> unlockedFx;
    for (int m = 1; m <= kNumFx; ++m)
        if (allowed ((Module) m)) unlockedFx.push_back (m);

    auto onIndex = [&] (int m)
    {
        auto id = onParamId ((Module) m);
        for (size_t i = 0; i < specs.size(); ++i) if (id == specs[i].id) return (int) i;
        return -1;
    };

    if (onlyModule < 0)
    {
        for (int m : unlockedFx)
        {
            int oi = onIndex (m);
            if (full)
                norm[(size_t) oi] = r.chance (juce::jmin (0.95f, baseOnProb ((Module) m) * (0.65f + 0.7f * chaos))) ? 1.f : 0.f;
            else if (r.chance (0.12f * chaos))
                norm[(size_t) oi] = norm[(size_t) oi] > 0.5f ? 0.f : 1.f;
        }
        // make sure at least two effects are doing something
        int onCount = 0;
        for (int m = 1; m <= kNumFx; ++m) if (norm[(size_t) onIndex (m)] > 0.5f) ++onCount;
        for (int guard = 0; onCount < 2 && ! unlockedFx.empty() && guard < 32; ++guard)
        {
            int m = unlockedFx[(size_t) r.range ((int) unlockedFx.size())];
            if (norm[(size_t) onIndex (m)] < 0.5f) { norm[(size_t) onIndex (m)] = 1.f; ++onCount; }
        }
    }
    else
    {
        norm[(size_t) onIndex (onlyModule)] = 1.f;
    }

    // ---------------------------------------------------------------- values
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        if (! allowed (s.module) || s.rnd == Rnd::Never) continue;
        if (juce::String (s.id).endsWith ("_on")) continue;
        if (s.module == Module::Global && onlyModule >= 0) continue;

        if (s.kind == Kind::Bool)
        {
            // freeze: only by hand (a surprise frozen pad sounds like stuck feedback)
            if (full) norm[i] = 0.f;
            continue;
        }

        if (full)
        {
            norm[i] = sampleNorm (r, s.rnd, chaos);
        }
        else
        {
            if (s.kind == Kind::Choice) { if (r.chance (0.25f * chaos)) norm[i] = r.uni(); continue; }
            float amt = (0.04f + 0.2f * chaos) * (s.rnd == Rnd::Low ? 0.5f : 1.f);
            norm[i] = juce::jlimit (0.f, 1.f, norm[i] + r.gauss() * amt);
        }
    }

    // ---------------------------------------------------------------- musical-ish guard rails
    auto at = [&] (const char* id) -> float& { for (size_t i = 0; i < specs.size(); ++i) if (! std::strcmp (specs[i].id, id)) return norm[i]; return norm[0]; };
    auto toNorm = [&] (const char* id, float v) { for (size_t i = 0; i < specs.size(); ++i) if (! std::strcmp (specs[i].id, id)) return proc.param ((int) i)->convertTo0to1 (v); return 0.f; };
    auto fromNorm = [&] (const char* id, float n) { for (size_t i = 0; i < specs.size(); ++i) if (! std::strcmp (specs[i].id, id)) return proc.param ((int) i)->convertFrom0to1 (n); return 0.f; };

    if (full && allowed (Module::Pitch) && r.chance (1.f - 0.65f * chaos))
    {
        static const float intervals[] = { -24.f, -12.f, -7.f, -5.f, 5.f, 7.f, 12.f, 12.f, 19.f, 24.f };
        at ("pitch_semi") = toNorm ("pitch_semi", r.pick (intervals));
    }
    if (allowed (Module::Filter))
    {
        // a closed low-pass with no movement is just silence
        if (fromNorm ("filt_morph", at ("filt_morph")) < 0.6f && fromNorm ("filt_cutoff", at ("filt_cutoff")) < 250.f
            && fromNorm ("filt_env", at ("filt_env")) < 0.3f)
            at ("filt_env") = toNorm ("filt_env", 0.4f + 0.5f * r.uni());
    }
    if (allowed (Module::Delay) && fromNorm ("dly_fb", at ("dly_fb")) > 0.92f)
        at ("dly_fb") = toNorm ("dly_fb", 0.92f);
    if (allowed (Module::Delay) && fromNorm ("dly_fb", at ("dly_fb")) > 0.85f)
        at ("dly_mix") = juce::jmin (at ("dly_mix"), 0.55f);
    if (allowed (Module::Verb) && at ("verb_freeze") > 0.5f)
        at ("verb_mix") = juce::jmin (at ("verb_mix"), 0.5f);
    if (allowed (Module::Crush) && fromNorm ("crush_bits", at ("crush_bits")) < 2.5f && r.chance (0.6f))
        at ("crush_bits") = toNorm ("crush_bits", 3.f + r.uni() * 6.f);

    // ---------------------------------------------------------------- apply
    for (int i = 0; i < proc.numParams(); ++i)
        if (std::abs (norm[(size_t) i] - proc.param (i)->getValue()) > 1e-6f)
            setNorm (i, norm[(size_t) i]);

    if (onlyModule < 0)
    {
        if (full)
        {
            proc.setMeta ("patchName", makeName (r));
            proc.setMeta ("seed", seedToString (seed));
            proc.setMeta ("starSign", juce::String (r.pick (starSigns)));
        }
        else
        {
            // mutations keep a family resemblance in the name
            auto old = proc.getPatchName();
            auto words = juce::StringArray::fromTokens (old, " ", "");
            if (words.size() > 0) words.set (r.range (words.size()), r.chance (0.5f) ? r.pick (adjectives) : r.pick (nouns));
            proc.setMeta ("patchName", words.joinIntoString (" "));
            proc.setMeta ("seed", seedToString (seed) + "*");
        }
        proc.setMeta ("issued", juce::Time::getCurrentTime().formatted ("%d / %m / %Y"));
    }
    ++proc.changeCounter;
}
} // namespace jackpot
