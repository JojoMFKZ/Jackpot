#include "PresetManager.h"
#include "PluginProcessor.h"
#include <algorithm>

namespace jackpot
{
namespace
{
struct Factory
{
    const char* name;
    const char* sign;
    std::vector<std::pair<const char*, float>> values; // real units; anything missing = default
};

const std::vector<Factory>& factory()
{
    static const std::vector<Factory> f = {
        { "Rust Engine", "Bat's Wing", {
            { "order", 0 }, { "drift", 0.08f },
            { "fuzz_on", 1 }, { "fuzz_sustain", 0.85f }, { "fuzz_tone", 0.45f }, { "fuzz_octave", 0.2f },
            { "crush_on", 0 }, { "pitch_on", 0 }, { "filt_on", 0 }, { "stut_on", 0 },
            { "tape_on", 1 }, { "tape_wow", 0.1f }, { "tape_flutter", 0.1f }, { "tape_sat", 0.3f }, { "tape_age", 0.15f },
            { "dly_on", 1 }, { "dly_time", 120.f }, { "dly_fb", 0.2f }, { "dly_mix", 0.25f }, { "dly_warble", 0.1f },
            { "verb_on", 1 }, { "verb_size", 0.35f }, { "verb_mix", 0.15f } } },

        { "Velvet Tide", "Sleeping Wasp", {
            { "order", 5 }, { "drift", 0.3f }, { "drift_rate", 0.1f },
            { "fuzz_on", 0 }, { "crush_on", 0 }, { "pitch_on", 0 }, { "stut_on", 0 },
            { "filt_on", 1 }, { "filt_cutoff", 2500.f }, { "filt_reso", 0.2f }, { "filt_morph", 0.f }, { "filt_env", 0.2f },
            { "tape_on", 1 }, { "tape_wow", 0.7f }, { "tape_flutter", 0.4f }, { "tape_sat", 0.5f }, { "tape_age", 0.6f }, { "tape_dropout", 0.3f },
            { "dly_on", 1 }, { "dly_time", 520.f }, { "dly_fb", 0.45f }, { "dly_warble", 0.6f }, { "dly_tone", 0.35f }, { "dly_mix", 0.35f },
            { "verb_on", 1 }, { "verb_size", 0.7f }, { "verb_damp", 0.6f }, { "verb_mix", 0.35f } } },

        { "Hollow Hornet", "Cat's Cradle", {
            { "order", 3 }, { "drift", 0.12f },
            { "fuzz_on", 1 }, { "fuzz_sustain", 0.7f }, { "fuzz_tone", 0.6f }, { "fuzz_starve", 0.6f },
            { "pitch_on", 1 }, { "pitch_semi", -12.f }, { "pitch_mix", 0.5f }, { "pitch_fb", 0.3f }, { "pitch_grain", 0.5f },
            { "filt_on", 1 }, { "filt_cutoff", 450.f }, { "filt_reso", 0.75f }, { "filt_morph", 1.f }, { "filt_env", 0.85f },
            { "crush_on", 0 }, { "stut_on", 0 }, { "tape_on", 0 },
            { "dly_on", 1 }, { "dly_time", 260.f }, { "dly_fb", 0.3f }, { "dly_mix", 0.2f },
            { "verb_on", 0 } } },

        { "Glass Cathedral", "Crooked Halo", {
            { "order", 12 }, { "drift", 0.25f }, { "drift_rate", 0.05f },
            { "fuzz_on", 0 }, { "crush_on", 0 }, { "filt_on", 0 }, { "stut_on", 0 }, { "tape_on", 0 },
            { "pitch_on", 1 }, { "pitch_semi", 12.f }, { "pitch_detune", 8.f }, { "pitch_mix", 0.35f }, { "pitch_grain", 0.6f },
            { "dly_on", 1 }, { "dly_time", 700.f }, { "dly_fb", 0.5f }, { "dly_reverse", 0.8f }, { "dly_mix", 0.45f }, { "dly_tone", 0.5f },
            { "verb_on", 1 }, { "verb_size", 0.92f }, { "verb_damp", 0.3f }, { "verb_shimmer", 0.7f }, { "verb_mix", 0.6f } } },

        { "Static Swarm", "Paper Crown", {
            { "order", 8 }, { "drift", 0.2f },
            { "fuzz_on", 0 }, { "pitch_on", 0 }, { "tape_on", 0 },
            { "crush_on", 1 }, { "crush_bits", 5.f }, { "crush_rate", 0.5f }, { "crush_ring", 0.4f }, { "crush_ringfreq", 440.f },
            { "filt_on", 1 }, { "filt_cutoff", 900.f }, { "filt_reso", 0.6f }, { "filt_morph", 1.f }, { "filt_env", 0.f },
            { "filt_lfo_rate", 6.f }, { "filt_lfo_depth", 0.7f }, { "filt_lfo_shape", 1.f },
            { "stut_on", 1 }, { "stut_size", 0.4f }, { "stut_prob", 0.5f }, { "stut_reverse", 0.4f }, { "stut_warp", 0.4f },
            { "dly_on", 1 }, { "dly_time", 180.f }, { "dly_fb", 0.35f }, { "dly_mix", 0.25f },
            { "verb_on", 1 }, { "verb_size", 0.4f }, { "verb_mix", 0.2f } } },

        { "Molten Bell", "Hollow Bell", {
            { "order", 17 }, { "drift", 0.15f },
            { "fuzz_on", 1 }, { "fuzz_sustain", 0.7f }, { "fuzz_octave", 0.9f }, { "fuzz_tone", 0.55f },
            { "filt_on", 1 }, { "filt_cutoff", 1400.f }, { "filt_reso", 0.5f }, { "filt_morph", 2.5f }, { "filt_env", 0.f },
            { "filt_lfo_rate", 0.3f }, { "filt_lfo_depth", 0.6f }, { "filt_lfo_shape", 0.f },
            { "crush_on", 0 }, { "pitch_on", 0 }, { "stut_on", 0 },
            { "tape_on", 1 }, { "tape_wow", 0.3f }, { "tape_age", 0.35f },
            { "dly_on", 1 }, { "dly_time", 300.f }, { "dly_fb", 0.6f }, { "dly_mix", 0.3f }, { "dly_warble", 0.3f },
            { "verb_on", 1 }, { "verb_size", 0.6f }, { "verb_mix", 0.25f } } },
    };
    return f;
}
} // namespace

juce::File PresetManager::folder()
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("JACKPOT").getChildFile ("Presets");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> PresetManager::userPresets() const
{
    auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
    return files;
}

juce::StringArray PresetManager::factoryNames() const
{
    juce::StringArray s;
    for (auto& f : factory()) s.add (f.name);
    return s;
}

bool PresetManager::save (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    proc.setMeta ("patchName", name.trim());
    auto state = proc.apvts.copyState();
    for (auto n : { "TRAIL", "KEPT" })
        if (auto c = state.getChildWithName (n); c.isValid()) state.removeChild (c, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    auto file = folder().getChildFile (clean + extension);
    return xml->writeTo (file);
}

bool PresetManager::loadFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (proc.apvts.state.getType())) return false;
    proc.pushHistory();
    proc.loadState (juce::ValueTree::fromXml (*xml), true);
    proc.setMeta ("patchName", f.getFileNameWithoutExtension());
    auto all = userPresets();
    position = factoryNames().size() + all.indexOf (f);
    return true;
}

void PresetManager::loadFactory (int index)
{
    auto& list = factory();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    proc.pushHistory();

    auto& specs = paramSpecs();
    auto& f = list[(size_t) index];
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        if (s.rnd == Rnd::Never && juce::String (s.id) != "chaos") continue; // leave the player's gain/mix alone
        if (juce::String (s.id) == "chaos") continue;
        float v = s.def;
        for (auto& kv : f.values) if (juce::String (kv.first) == s.id) v = kv.second;
        auto* p = proc.param ((int) i);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
    proc.setMeta ("patchName", juce::String (f.name));
    proc.setMeta ("seed", "FACT-ORY0-000" + juce::String (index + 1));
    proc.setMeta ("starSign", juce::String (f.sign));
    proc.setMeta ("issued", "01 / 01 / 1989");
    position = index;
    ++proc.changeCounter;
}

bool PresetManager::deleteFile (const juce::File& f)
{
    return f.existsAsFile() && f.isAChildOf (folder()) && f.deleteFile();
}

void PresetManager::step (int direction)
{
    auto users = userPresets();
    const int nf = factoryNames().size();
    const int total = nf + users.size();
    if (total == 0) return;
    position = ((position < 0 ? (direction > 0 ? -1 : 0) : position) + direction + total) % total;
    if (position < nf) loadFactory (position);
    else loadFile (users[position - nf]);
}
} // namespace jackpot
