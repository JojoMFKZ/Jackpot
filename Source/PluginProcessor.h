#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "DSP.h"
#include "Randomizer.h"
#include "PresetManager.h"
#include "Evolution.h"

class JackpotProcessor : public juce::AudioProcessor, private juce::Timer, private juce::AudioProcessorParameter::Listener
{
public:
    JackpotProcessor();
    ~JackpotProcessor() override = default;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ODDKIN"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return getPatchName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- MANTIS specifics (message thread)
    juce::AudioProcessorValueTreeState apvts;
    jackpot::Randomizer randomizer { *this };
    jackpot::PresetManager presets { *this };
    jackpot::Evolution evo { *this };

    // ---- map + family (message thread)
    int region() const { return (int) apvts.state.getProperty ("region", 12); }
    bool onMap() const { return (bool) apvts.state.getProperty ("onMap", true); }
    void setMap (float x, float y);                 // moves the cursor; the sound follows
    void beginMapGesture();
    void endMapGesture();
    void teleport();
    void goTo (const jackpot::Genome& g);           // jump to a bred / remembered sound
    struct Entry { jackpot::Genome g; juce::String name; };
    std::vector<Entry> trail() const;
    std::vector<Entry> kept() const;
    void pushTrail();
    bool back();
    void keepCurrent();
    bool isCurrentKept() const;
    int generation() const { return (int) apvts.state.getProperty ("gen", 1); }

    juce::String getMeta (const juce::String& key, const juce::String& fallback = {}) const;
    void setMeta (const juce::String& key, const juce::var& value);
    juce::String getPatchName() const { return evo.name (evo.current()); }

    bool isLocked (jackpot::Module m) const;
    void setLocked (jackpot::Module m, bool locked);

    void pushHistory();

    // ---- undo / redo: every knob change, map move, breed and preset load (message thread)
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const { return undoPos + 1 < (int) undoStack.size(); }
    void markEdited() { pendingCommit = true; }

    // ---- A/B: two versions of the sound to flip between (saved with the session)
    bool isB() const { return apvts.state.getChildWithName ("AB")["slot"].toString() == "B"; }
    void switchAB();             // store the current sound in this slot, load the other one
    void copyToOtherSlot();      // make the other slot the same as what you hear now
    void loadState (const juce::ValueTree& newState, bool keepLocks = true);

    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    int numParams() const { return (int) params.size(); }

    // UI feedback (audio -> UI), lock free
    std::array<std::atomic<float>, 96> effectiveNorm {};  // knob position after drift
    static constexpr int kScopeSize = 1024;
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> inLevel { 0.f }, outLevel { 0.f };
    std::atomic<int> changeCounter { 0 }; // bumped whenever the patch changes wholesale

private:
    void timerCallback() override;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override { if (! starting && ! restoring) pendingCommit = true; }

    struct Snap { std::vector<float> v; int region = 12; bool onMap = true; int gen = 1; };
    Snap snapshot() const;
    void restore (const Snap&);
    static bool sameSnap (const Snap&, const Snap&);
    static juce::String encodeSnap (const Snap&);
    Snap decodeSnap (const juce::String&) const;
    void commitUndo();
    void resetUndo();
    std::vector<Snap> undoStack;
    int undoPos = -1;
    std::atomic<bool> pendingCommit { false };
    bool restoring = false;
    void syncMap();
    float lastMapX = -1.f, lastMapY = -1.f, lastChaos = -1.f;
    bool mapGesture = false;

    void updateModuleSettings (int numSamples);
    void runModule (int stage, float* L, float* R, int n);
    void resetAllDsp();
    jackpot::Genome upgrade (jackpot::Genome g) const; // pads genomes saved before new effects existed

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<jackpot::dsp::SmoothNoise> driftNoise;
    std::vector<juce::ValueTree> history;

    jackpot::dsp::Fuzz fuzz;
    jackpot::dsp::Crush crush;
    jackpot::dsp::Pitch pitch;
    jackpot::dsp::Filter filter;
    jackpot::dsp::Tape tape;
    jackpot::dsp::Stutter stutter;
    jackpot::dsp::Delay delay;
    jackpot::dsp::Verb verb;
    jackpot::dsp::Harmony harmony;
    jackpot::dsp::Resonator resonator;
    jackpot::dsp::Gate gate;

    std::array<float, jackpot::kNumFx + 1> modLevel {};   // fade in/out per module
    std::array<bool, jackpot::kNumFx + 1> modOn {};
    std::array<int, jackpot::kNumFx + 1> onIdx {};
    juce::AudioBuffer<float> dryBuf, scratch;
    std::array<std::array<int, 4>, 24> perms {};

    double sr = 44100.0;
    float inGainSm = 1.f, outGainSm = 1.f, mixSm = 1.f;
    float dryPow = 0.f, wetPow = 0.f, autoGain = 1.f;
    jackpot::dsp::DCBlock dcL, dcR;
    int scopeDecim = 0;

    int idx (const char* id) const;
    float val (int i) const { return raw[(size_t) i]->load(); }
    std::unordered_map<std::string, int> indexOf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JackpotProcessor)
};
