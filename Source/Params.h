#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  MANTIS parameter catalogue.
//  Every parameter belongs to a "module" so the randomizer can lock whole
//  modules and the UI can lay them out as cards.
// ============================================================================
namespace jackpot
{
enum class Module { Global = 0, Fuzz, Crush, Pitch, Filter, Tape, Stutter, Delay, Verb, Harmony, Resonator, Count };

constexpr int kNumFx = 10; // modules that have an on/off + lock

inline const char* moduleKey (Module m)
{
    switch (m)
    {
        case Module::Global:  return "global";
        case Module::Fuzz:    return "fuzz";
        case Module::Crush:   return "crush";
        case Module::Pitch:   return "pitch";
        case Module::Filter:  return "filt";
        case Module::Tape:    return "tape";
        case Module::Stutter: return "stut";
        case Module::Delay:   return "dly";
        case Module::Verb:    return "verb";
        case Module::Harmony: return "harm";
        case Module::Resonator: return "reso";
        default:              return "";
    }
}

enum class Kind { Float, Bool, Choice };

// How the randomizer should treat a parameter
enum class Rnd
{
    Normal,   // uniform-ish, edges more likely with more chaos
    Gentle,   // stays near the middle unless chaos is high
    Low,      // biased toward the low end (feedback, dangerous stuff)
    Never     // randomizer leaves it alone (output gain, chaos itself…)
};

struct ParamSpec
{
    const char* id;
    const char* name;      // host-facing name
    const char* label;     // short label on the knob
    Module module;
    Kind kind;
    float min, max, def, skewCentre; // skewCentre <= min means linear
    const char* unit;
    Rnd rnd;
};

// NOTE: ids are permanent (saved in presets / host sessions). Don't rename.
inline const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = {
        // ---------------------------------------------------------- GLOBAL
        { "in_gain",   "Input Gain",   "IN",     Module::Global, Kind::Float, -24.f, 24.f, 0.f, -100.f, "dB", Rnd::Never },
        { "out_gain",  "Output Gain",  "OUT",    Module::Global, Kind::Float, -24.f, 12.f, 0.f, -100.f, "dB", Rnd::Never },
        { "mix",       "Dry/Wet",      "MIX",    Module::Global, Kind::Float, 0.f, 1.f, 1.f, -1.f, "", Rnd::Never },
        { "chaos",     "Chaos",        "CHAOS",  Module::Global, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Never },
        { "drift",     "Drift",        "DRIFT",  Module::Global, Kind::Float, 0.f, 1.f, 0.15f, -1.f, "", Rnd::Low },
        { "drift_rate","Drift Rate",   "D.RATE", Module::Global, Kind::Float, 0.01f, 4.f, 0.2f, 0.3f, "Hz", Rnd::Gentle },
        { "autolevel", "Auto Level",   "AUTO",   Module::Global, Kind::Bool, 0.f, 1.f, 1.f, -1.f, "", Rnd::Never },
        { "order",     "Chain Order",  "ORDER",  Module::Global, Kind::Choice, 0.f, 23.f, 0.f, -1.f, "", Rnd::Normal },
        { "map_x",     "Map X",        "MAP X",  Module::Global, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Never },
        { "map_y",     "Map Y",        "MAP Y",  Module::Global, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Never },

        // ---------------------------------------------------------- FUZZ  (big muff-ish)
        { "fuzz_on",      "Fuzz On",       "ON",      Module::Fuzz, Kind::Bool, 0.f, 1.f, 1.f, -1.f, "", Rnd::Normal },
        { "fuzz_sustain", "Fuzz Sustain",  "SUSTAIN", Module::Fuzz, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "fuzz_tone",    "Fuzz Tone",     "TONE",    Module::Fuzz, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Normal },
        { "fuzz_starve",  "Fuzz Starve",   "STARVE",  Module::Fuzz, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Low },
        { "fuzz_octave",  "Fuzz Octave",   "OCTAVE",  Module::Fuzz, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "fuzz_blend",   "Fuzz Blend",    "BLEND",   Module::Fuzz, Kind::Float, 0.f, 1.f, 1.f, -1.f, "", Rnd::Gentle },

        // ---------------------------------------------------------- CRUSH (character)
        { "crush_on",       "Crush On",        "ON",      Module::Crush, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "crush_bits",     "Crush Bits",      "BITS",    Module::Crush, Kind::Float, 1.f, 16.f, 8.f, -1.f, "bit", Rnd::Gentle },
        { "crush_rate",     "Crush Rate",      "RATE",    Module::Crush, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Low },
        { "crush_fold",     "Crush Fold",      "FOLD",    Module::Crush, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "crush_ring",     "Ring Amount",     "RING",    Module::Crush, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "crush_ringfreq", "Ring Freq",       "R.FREQ",  Module::Crush, Kind::Float, 20.f, 3000.f, 220.f, 300.f, "Hz", Rnd::Normal },

        // ---------------------------------------------------------- PITCH
        { "pitch_on",     "Pitch On",       "ON",      Module::Pitch, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "pitch_semi",   "Pitch Semis",    "SEMI",    Module::Pitch, Kind::Float, -24.f, 24.f, 12.f, -100.f, "st", Rnd::Normal },
        { "pitch_detune", "Pitch Detune",   "DETUNE",  Module::Pitch, Kind::Float, -50.f, 50.f, 0.f, -100.f, "ct", Rnd::Gentle },
        { "pitch_grain",  "Pitch Grain",    "GRAIN",   Module::Pitch, Kind::Float, 0.f, 1.f, 0.4f, -1.f, "", Rnd::Normal },
        { "pitch_fb",     "Pitch Spiral",   "SPIRAL",  Module::Pitch, Kind::Float, 0.f, 0.9f, 0.f, -1.f, "", Rnd::Low },
        { "pitch_mix",    "Pitch Mix",      "MIX",     Module::Pitch, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Gentle },

        // ---------------------------------------------------------- FILTER
        { "filt_on",       "Filter On",        "ON",      Module::Filter, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "filt_cutoff",   "Filter Cutoff",    "CUTOFF",  Module::Filter, Kind::Float, 40.f, 16000.f, 1200.f, 900.f, "Hz", Rnd::Normal },
        { "filt_reso",     "Filter Reso",      "RESO",    Module::Filter, Kind::Float, 0.f, 1.f, 0.4f, -1.f, "", Rnd::Normal },
        { "filt_morph",    "Filter Morph",     "MORPH",   Module::Filter, Kind::Float, 0.f, 3.f, 0.f, -1.f, "type", Rnd::Normal },
        { "filt_env",      "Filter Env",       "ENV",     Module::Filter, Kind::Float, -1.f, 1.f, 0.5f, -100.f, "", Rnd::Normal },
        { "filt_lfo_rate", "Filter LFO Rate",  "L.RATE",  Module::Filter, Kind::Float, 0.02f, 20.f, 1.f, 1.5f, "Hz", Rnd::Normal },
        { "filt_lfo_depth","Filter LFO Depth", "L.DEPTH", Module::Filter, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "filt_lfo_shape","Filter LFO Shape", "SHAPE",   Module::Filter, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },

        // ---------------------------------------------------------- TAPE
        { "tape_on",      "Tape On",       "ON",      Module::Tape, Kind::Bool, 0.f, 1.f, 1.f, -1.f, "", Rnd::Normal },
        { "tape_wow",     "Tape Wow",      "WOW",     Module::Tape, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Normal },
        { "tape_flutter", "Tape Flutter",  "FLUTTER", Module::Tape, Kind::Float, 0.f, 1.f, 0.15f, -1.f, "", Rnd::Normal },
        { "tape_sat",     "Tape Saturate", "SAT",     Module::Tape, Kind::Float, 0.f, 1.f, 0.3f, -1.f, "", Rnd::Normal },
        { "tape_age",     "Tape Age",      "AGE",     Module::Tape, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Normal },
        { "tape_dropout", "Tape Dropout",  "DROPOUT", Module::Tape, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Low },

        // ---------------------------------------------------------- STUTTER
        { "stut_on",     "Stutter On",      "ON",      Module::Stutter, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "stut_size",   "Stutter Size",    "SIZE",    Module::Stutter, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Normal },
        { "stut_prob",   "Stutter Chance",  "CHANCE",  Module::Stutter, Kind::Float, 0.f, 1.f, 0.3f, -1.f, "", Rnd::Gentle },
        { "stut_reverse","Stutter Reverse", "REVERSE", Module::Stutter, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Normal },
        { "stut_warp",   "Stutter Warp",    "WARP",    Module::Stutter, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },

        // ---------------------------------------------------------- DELAY
        { "dly_on",      "Delay On",       "ON",      Module::Delay, Kind::Bool, 0.f, 1.f, 1.f, -1.f, "", Rnd::Normal },
        { "dly_time",    "Delay Time",     "TIME",    Module::Delay, Kind::Float, 10.f, 2000.f, 380.f, 300.f, "ms", Rnd::Normal },
        { "dly_fb",      "Delay Feedback", "FDBK",    Module::Delay, Kind::Float, 0.f, 1.05f, 0.35f, -1.f, "", Rnd::Low },
        { "dly_tone",    "Delay Tone",     "TONE",    Module::Delay, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "dly_warble",  "Delay Warble",   "WARBLE",  Module::Delay, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Normal },
        { "dly_reverse", "Delay Reverse",  "REVERSE", Module::Delay, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "dly_mix",     "Delay Mix",      "MIX",     Module::Delay, Kind::Float, 0.f, 1.f, 0.3f, -1.f, "", Rnd::Gentle },

        // ---------------------------------------------------------- REVERB
        { "verb_on",     "Reverb On",      "ON",      Module::Verb, Kind::Bool, 0.f, 1.f, 1.f, -1.f, "", Rnd::Normal },
        { "verb_size",   "Reverb Size",    "SIZE",    Module::Verb, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "verb_damp",   "Reverb Damping", "DAMP",    Module::Verb, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Normal },
        { "verb_width",  "Reverb Width",   "WIDTH",   Module::Verb, Kind::Float, 0.f, 1.f, 1.f, -1.f, "", Rnd::Gentle },
        { "verb_shimmer","Reverb Shimmer", "SHIMMER", Module::Verb, Kind::Float, 0.f, 1.f, 0.f, -1.f, "", Rnd::Low },
        { "verb_mix",    "Reverb Mix",     "MIX",     Module::Verb, Kind::Float, 0.f, 1.f, 0.25f, -1.f, "", Rnd::Gentle },
        { "verb_freeze", "Reverb Freeze",  "FREEZE",  Module::Verb, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Low },

        // (added in 1.1 - new params go at the end so old sessions and map spots keep their sound)
        // ---------------------------------------------------------- HARMONY
        { "harm_on",     "Harmony On",     "ON",      Module::Harmony, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "harm_shape",  "Harmony Chord",  "CHORD",   Module::Harmony, Kind::Float, 0.f, 7.f, 1.f, -1.f, "harm", Rnd::Normal },
        { "harm_detune", "Harmony Detune", "DETUNE",  Module::Harmony, Kind::Float, 0.f, 1.f, 0.2f, -1.f, "", Rnd::Gentle },
        { "harm_spread", "Harmony Spread", "SPREAD",  Module::Harmony, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "harm_tone",   "Harmony Tone",   "TONE",    Module::Harmony, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "harm_mix",    "Harmony Mix",    "MIX",     Module::Harmony, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Gentle },

        // ---------------------------------------------------------- RESONATOR
        { "reso_on",     "Resonator On",     "ON",     Module::Resonator, Kind::Bool, 0.f, 1.f, 0.f, -1.f, "", Rnd::Normal },
        { "reso_key",    "Resonator Key",    "KEY",    Module::Resonator, Kind::Float, 0.f, 11.f, 4.f, -1.f, "note", Rnd::Never },
        { "reso_chord",  "Resonator Chord",  "CHORD",  Module::Resonator, Kind::Float, 0.f, 7.f, 0.f, -1.f, "chord", Rnd::Normal },
        { "reso_octave", "Resonator Octave", "OCTAVE", Module::Resonator, Kind::Float, 0.f, 1.f, 0.4f, -1.f, "oct", Rnd::Normal },
        { "reso_decay",  "Resonator Decay",  "DECAY",  Module::Resonator, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Normal },
        { "reso_bright", "Resonator Bright", "BRIGHT", Module::Resonator, Kind::Float, 0.f, 1.f, 0.6f, -1.f, "", Rnd::Normal },
        { "reso_mix",    "Resonator Mix",    "MIX",    Module::Resonator, Kind::Float, 0.f, 1.f, 0.5f, -1.f, "", Rnd::Gentle },
    };
    return specs;
}

// names shown for the stepped Harmony / Resonator knobs (kept here so Params.h has no DSP dependency)
inline const char* dsp_noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((n % 12) + 12) % 12];
}
inline const char* dsp_harmName (int i)
{
    static const char* names[] = { "Octaves", "Fifth", "Power", "Major", "Minor", "Sus", "Major 7", "Wide" };
    return names[juce::jlimit (0, 7, i)];
}
inline const char* dsp_chordName (int i)
{
    static const char* names[] = { "Root + 5th", "Major", "Minor", "Sus2", "Major 7", "Minor 7", "Octaves", "Fifths" };
    return names[juce::jlimit (0, 7, i)];
}

inline juce::String onParamId (Module m) { return juce::String (moduleKey (m)) + "_on"; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    int version = 1;

    for (auto& s : paramSpecs())
    {
        juce::ParameterID pid { s.id, version };
        if (s.kind == Kind::Bool)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, s.def > 0.5f));
        }
        else if (s.kind == Kind::Choice)
        {
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, s.name, (int) s.min, (int) s.max, (int) s.def));
        }
        else
        {
            juce::NormalisableRange<float> r (s.min, s.max);
            if (s.skewCentre > s.min && s.skewCentre < s.max)
                r.setSkewForCentre (s.skewCentre);

            juce::String unit (s.unit);
            auto attrs = juce::AudioParameterFloatAttributes()
                .withLabel (unit)
                .withStringFromValueFunction ([unit] (float v, int)
                {
                    if (unit == "Hz")  return v >= 1000.f ? juce::String (v / 1000.f, 2) + " kHz" : juce::String (v, v < 10.f ? 2 : 0) + " Hz";
                    if (unit == "ms")  return v >= 1000.f ? juce::String (v / 1000.f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
                    if (unit == "dB")  return juce::String (v, 1) + " dB";
                    if (unit == "st")  return (v > 0 ? "+" : "") + juce::String (v, 1) + " st";
                    if (unit == "ct")  return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
                    if (unit == "type")
                    {
                        static const char* names[] = { "LP", "BP", "HP", "NOTCH" };
                        int i = juce::jlimit (0, 2, (int) v);
                        float f = v - (float) i;
                        if (f < 0.15f) return juce::String (names[i]);
                        if (f > 0.85f) return juce::String (names[i + 1]);
                        return juce::String (names[i]) + ">" + names[i + 1];
                    }
                    if (unit == "bit") return juce::String (v, 1) + " bit";
                    if (unit == "note") return juce::String (dsp_noteName (juce::roundToInt (v)));
                    if (unit == "harm") return juce::String (dsp_harmName (juce::roundToInt (v)));
                    if (unit == "chord") return juce::String (dsp_chordName (juce::roundToInt (v)));
                    if (unit == "oct") return juce::String (2 + juce::jlimit (0, 3, (int) std::floor (v * 3.999f)));
                    return juce::String (juce::roundToInt (v * 100.f)) + "%";
                });
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, s.name, r, s.def, attrs));
        }
    }
    return layout;
}
} // namespace jackpot
