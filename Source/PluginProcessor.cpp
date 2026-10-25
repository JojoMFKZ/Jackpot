#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>

using namespace jackpot;

namespace
{
// stage ids used by the chain
enum Stage { sFuzz = 1, sCrush, sPitch, sFilter, sTape, sStutter, sDelay, sVerb, sHarm, sReso };
}

JackpotProcessor::JackpotProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "JACKPOT", createLayout())   // internal state name: kept from before the rename so old sessions and presets load
{
    auto& specs = paramSpecs();
    jassert (specs.size() <= effectiveNorm.size());
    for (size_t i = 0; i < specs.size(); ++i)
    {
        params.push_back (apvts.getParameter (specs[i].id));
        raw.push_back (apvts.getRawParameterValue (specs[i].id));
        indexOf[specs[i].id] = (int) i;
        effectiveNorm[i] = params.back()->getValue();
    }
    for (int m = 1; m <= kNumFx; ++m)
        onIdx[(size_t) m] = idx (onParamId ((Module) m).toRawUTF8());
    driftNoise.resize (specs.size());
    for (auto& d : driftNoise) d.seed (juce::Random::getSystemRandom().nextInt64());

    // all 24 orderings of the 4 movable stages
    std::array<int, 4> p { sFuzz, sCrush, sPitch, sFilter };
    for (auto& row : perms) { row = p; std::next_permutation (p.begin(), p.end()); }

    setMeta ("region", 12);
    setMeta ("onMap", true);
    setMeta ("gen", 1);
    syncMap();
    startTimerHz (30);
    setMeta ("seed", "----");
    setMeta ("starSign", "Bat's Wing");
    setMeta ("issued", juce::Time::getCurrentTime().formatted ("%d / %m / %Y"));
}

int JackpotProcessor::idx (const char* id) const
{
    auto it = indexOf.find (id);
    jassert (it != indexOf.end());
    return it->second;
}

bool JackpotProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void JackpotProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxB = juce::jmax (16, samplesPerBlock);
    fuzz.prepare (sr, maxB);
    crush.prepare (sr, maxB);
    pitch.prepare (sr, maxB);
    filter.prepare (sr, maxB);
    tape.prepare (sr, maxB);
    stutter.prepare (sr, maxB);
    delay.prepare (sr, maxB);
    verb.prepare (sr, maxB);
    harmony.prepare (sr, maxB);
    resonator.prepare (sr, maxB);
    dryBuf.setSize (2, maxB);
    scratch.setSize (2, maxB);
    dcL.prepare (sr); dcR.prepare (sr);
    dcL.reset(); dcR.reset();
    for (int m = 1; m <= kNumFx; ++m)
    {
        modOn[(size_t) m] = val (onIdx[(size_t) m]) > 0.5f;
        modLevel[(size_t) m] = modOn[(size_t) m] ? 1.f : 0.f;
    }
    inGainSm = juce::Decibels::decibelsToGain (val (idx ("in_gain")));
    outGainSm = juce::Decibels::decibelsToGain (val (idx ("out_gain")));
    dryPow = wetPow = 0.f;
    autoGain = 1.f;
}

void JackpotProcessor::resetAllDsp()
{
    fuzz.reset(); crush.reset(); pitch.reset(); filter.reset();
    tape.reset(); stutter.reset(); delay.reset(); verb.reset();
    harmony.reset(); resonator.reset();
    dcL.reset(); dcR.reset();
    dryPow = wetPow = 0.f; autoGain = 1.f;
}

// ---------------------------------------------------------------------------
//  Pull parameter values (with drift applied) into the DSP modules.
// ---------------------------------------------------------------------------
void JackpotProcessor::updateModuleSettings (int n)
{
    auto& specs = paramSpecs();
    const float driftAmt = val (idx ("drift"));
    const float driftInc = val (idx ("drift_rate")) * (float) n / (float) sr;

    std::array<float, 96> v {};
    for (size_t i = 0; i < specs.size(); ++i)
    {
        auto& s = specs[i];
        float d = driftNoise[i].next (driftInc * (0.7f + 0.6f * (float) ((i * 7919) % 100) / 100.f));
        if (s.kind == Kind::Float && s.rnd != Rnd::Never && driftAmt > 0.0001f && s.module != Module::Global)
        {
            float norm = juce::jlimit (0.f, 1.f, params[i]->getValue() + d * driftAmt * 0.25f);
            effectiveNorm[i] = norm;
            v[i] = params[i]->convertFrom0to1 (norm);
        }
        else
        {
            effectiveNorm[i] = params[i]->getValue();
            v[i] = raw[i]->load();
        }
    }
    auto V = [&] (const char* id) { return v[(size_t) idx (id)]; };

    fuzz.sustain = V ("fuzz_sustain"); fuzz.tone = V ("fuzz_tone"); fuzz.starve = V ("fuzz_starve");
    fuzz.octave = V ("fuzz_octave"); fuzz.blend = V ("fuzz_blend");

    crush.bits = V ("crush_bits"); crush.rate = V ("crush_rate"); crush.fold = V ("crush_fold");
    crush.ring = V ("crush_ring"); crush.ringFreq = V ("crush_ringfreq");

    pitch.semis = V ("pitch_semi"); pitch.detuneCents = V ("pitch_detune"); pitch.grain = V ("pitch_grain");
    pitch.feedback = V ("pitch_fb"); pitch.mix = V ("pitch_mix");

    filter.cutoff = V ("filt_cutoff"); filter.reso = V ("filt_reso"); filter.morph = V ("filt_morph");
    filter.envAmt = V ("filt_env"); filter.lfoRate = V ("filt_lfo_rate"); filter.lfoDepth = V ("filt_lfo_depth");
    filter.lfoShape = V ("filt_lfo_shape");

    tape.wow = V ("tape_wow"); tape.flutter = V ("tape_flutter"); tape.sat = V ("tape_sat");
    tape.age = V ("tape_age"); tape.dropout = V ("tape_dropout");

    stutter.size = V ("stut_size"); stutter.prob = V ("stut_prob"); stutter.reverse = V ("stut_reverse");
    stutter.warp = V ("stut_warp");

    delay.timeMs = V ("dly_time"); delay.feedback = V ("dly_fb"); delay.tone = V ("dly_tone");
    delay.warble = V ("dly_warble"); delay.reverse = V ("dly_reverse"); delay.mix = V ("dly_mix");

    verb.size = V ("verb_size"); verb.damp = V ("verb_damp"); verb.width = V ("verb_width");
    verb.shimmer = V ("verb_shimmer"); verb.mix = V ("verb_mix"); verb.freeze = V ("verb_freeze") > 0.5f;

    harmony.shape = V ("harm_shape"); harmony.detune = V ("harm_detune"); harmony.spread = V ("harm_spread");
    harmony.tone = V ("harm_tone"); harmony.mix = V ("harm_mix");

    resonator.key = V ("reso_key"); resonator.chord = V ("reso_chord"); resonator.reg = V ("reso_octave");
    resonator.decay = V ("reso_decay"); resonator.bright = V ("reso_bright"); resonator.mix = V ("reso_mix");

    for (int m = 1; m <= kNumFx; ++m)
        modOn[(size_t) m] = v[(size_t) onIdx[(size_t) m]] > 0.5f;
}

void JackpotProcessor::runModule (int stage, float* L, float* R, int n)
{
    switch (stage)
    {
        case sFuzz:    fuzz.process (L, R, n); break;
        case sCrush:   crush.process (L, R, n); break;
        case sPitch:   pitch.process (L, R, n); break;
        case sFilter:  filter.process (L, R, n); break;
        case sTape:    tape.process (L, R, n); break;
        case sStutter: stutter.process (L, R, n); break;
        case sDelay:   delay.process (L, R, n); break;
        case sVerb:    verb.process (L, R, n); break;
        case sHarm:    harmony.process (L, R, n); break;
        case sReso:    resonator.process (L, R, n); break;
        default: break;
    }
}

void JackpotProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2 || total == 0) return;
    if (inCh == 1) buffer.copyFrom (1, 0, buffer, 0, 0, total); // mono guitar -> both sides

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                stutter.bpm = *bpm;

    // process in chunks no bigger than what we prepared for
    const int maxChunk = dryBuf.getNumSamples();
    for (int start = 0; start < total; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, total - start);
        float* L = buffer.getWritePointer (0, start);
        float* R = buffer.getWritePointer (1, start);

        updateModuleSettings (n);

        // input gain
        const float inTarget = juce::Decibels::decibelsToGain (val (idx ("in_gain")));
        float inPeak = 0.f;
        for (int i = 0; i < n; ++i)
        {
            inGainSm += 0.002f * (inTarget - inGainSm);
            L[i] *= inGainSm; R[i] *= inGainSm;
            inPeak = juce::jmax (inPeak, std::abs (L[i]));
        }
        dryBuf.copyFrom (0, 0, L, n);
        dryBuf.copyFrom (1, 0, R, n);

        // chain: [4 movable stages in random order] -> harmony -> resonator -> tape -> stutter -> delay -> reverb
        const int order = juce::jlimit (0, 23, (int) val (idx ("order")));
        std::array<int, 10> chain { perms[(size_t) order][0], perms[(size_t) order][1], perms[(size_t) order][2], perms[(size_t) order][3],
                                    sHarm, sReso, sTape, sStutter, sDelay, sVerb };
        const float fadeStep = (float) n / (0.03f * (float) sr);

        for (int stage : chain)
        {
            auto& lvl = modLevel[(size_t) stage];
            const float target = modOn[(size_t) stage] ? 1.f : 0.f;
            if (lvl <= 0.f && target <= 0.f) continue;

            const float startLvl = lvl;
            lvl = target > lvl ? juce::jmin (target, lvl + fadeStep) : juce::jmax (target, lvl - fadeStep);

            if (startLvl >= 1.f && lvl >= 1.f)
            {
                runModule (stage, L, R, n);
            }
            else
            {
                scratch.copyFrom (0, 0, L, n);
                scratch.copyFrom (1, 0, R, n);
                runModule (stage, L, R, n);
                for (int i = 0; i < n; ++i)
                {
                    float g = startLvl + (lvl - startLvl) * (float) i / (float) n;
                    L[i] = scratch.getSample (0, i) + g * (L[i] - scratch.getSample (0, i));
                    R[i] = scratch.getSample (1, i) + g * (R[i] - scratch.getSample (1, i));
                }
            }
        }

        // auto level: keep the mangled signal roughly as loud as what came in
        const bool autoLvl = val (idx ("autolevel")) > 0.5f;
        float dp = 0.f, wp = 0.f;
        for (int i = 0; i < n; ++i)
        {
            float d = dryBuf.getSample (0, i), w = L[i];
            dp += d * d; wp += w * w;
        }
        const float a = 1.f - std::exp (-(float) n / (0.4f * (float) sr));
        dryPow += a * (dp / (float) n - dryPow);
        wetPow += a * (wp / (float) n - wetPow);
        float targetGain = autoGain;
        if (autoLvl)
        {
            if (dryPow > 1e-6f && wetPow > 1e-9f)
                targetGain = juce::jlimit (0.06f, 4.f, std::sqrt (dryPow / wetPow));
        }
        else targetGain = 1.f;
        const float g0 = autoGain;
        autoGain += (1.f - std::exp (-(float) n / (0.8f * (float) sr))) * (targetGain - autoGain);

        const float mixT = val (idx ("mix"));
        const float outT = juce::Decibels::decibelsToGain (val (idx ("out_gain")));
        float outPeak = 0.f;
        bool bad = false;

        for (int i = 0; i < n; ++i)
        {
            float ag = g0 + (autoGain - g0) * (float) i / (float) n;
            mixSm += 0.002f * (mixT - mixSm);
            outGainSm += 0.002f * (outT - outGainSm);
            float l = dryBuf.getSample (0, i) * (1.f - mixSm) + L[i] * ag * mixSm;
            float r = dryBuf.getSample (1, i) * (1.f - mixSm) + R[i] * ag * mixSm;
            l = dcL.process (l) * outGainSm;
            r = dcR.process (r) * outGainSm;

            // safety: soft knee limiter, nothing past ~0 dBFS
            auto soft = [] (float x)
            {
                float ax = std::abs (x);
                if (ax <= 0.8f) return x;
                float y = 0.8f + 0.2f * std::tanh ((ax - 0.8f) / 0.2f);
                return std::copysign (y, x);
            };
            l = soft (l); r = soft (r);
            if (! std::isfinite (l) || ! std::isfinite (r)) { bad = true; l = r = 0.f; }
            L[i] = l; R[i] = r;
            outPeak = juce::jmax (outPeak, std::abs (l));

            if (++scopeDecim >= 2)
            {
                scopeDecim = 0;
                int w = scopeWrite.load (std::memory_order_relaxed);
                scope[(size_t) w].store (0.5f * (l + r), std::memory_order_relaxed);
                scopeWrite.store ((w + 1) % kScopeSize, std::memory_order_release);
            }
        }
        if (bad) resetAllDsp();

        inLevel.store (juce::jmax (inPeak, inLevel.load() * 0.9f));
        outLevel.store (juce::jmax (outPeak, outLevel.load() * 0.9f));
    }
}

// ---------------------------------------------------------------------------
//  State / meta / locks / history
// ---------------------------------------------------------------------------
juce::String JackpotProcessor::getMeta (const juce::String& key, const juce::String& fallback) const
{
    auto v = apvts.state.getProperty (juce::Identifier (key));
    return v.isVoid() ? fallback : v.toString();
}

void JackpotProcessor::setMeta (const juce::String& key, const juce::var& value)
{
    apvts.state.setProperty (juce::Identifier (key), value, nullptr);
}

bool JackpotProcessor::isLocked (Module m) const
{
    return (bool) apvts.state.getProperty (juce::Identifier (juce::String ("lock_") + moduleKey (m)), false);
}

void JackpotProcessor::setLocked (Module m, bool locked)
{
    apvts.state.setProperty (juce::Identifier (juce::String ("lock_") + moduleKey (m)), locked, nullptr);
}

void JackpotProcessor::pushHistory() { pushTrail(); }
bool JackpotProcessor::undo() { return back(); }

// ---------------------------------------------------------------------------
//  Map + family
// ---------------------------------------------------------------------------
void JackpotProcessor::syncMap()
{
    lastMapX = apvts.getRawParameterValue ("map_x")->load();
    lastMapY = apvts.getRawParameterValue ("map_y")->load();
    lastChaos = apvts.getRawParameterValue ("chaos")->load();
}

void JackpotProcessor::timerCallback()
{
    const float x = apvts.getRawParameterValue ("map_x")->load();
    const float y = apvts.getRawParameterValue ("map_y")->load();
    const float c = apvts.getRawParameterValue ("chaos")->load();
    const bool moved = std::abs (x - lastMapX) > 1e-4f || std::abs (y - lastMapY) > 1e-4f;
    const bool rangeChanged = onMap() && std::abs (c - lastChaos) > 0.005f;
    if (! moved && ! rangeChanged) return;
    lastMapX = x; lastMapY = y; lastChaos = c;
    evo.apply (evo.at (region(), x, y));
    if (! onMap()) { setMeta ("onMap", true); setMeta ("gen", 1); }
    ++changeCounter;
}

static void setParam (juce::RangedAudioParameter* p, float v, bool gesture)
{
    if (gesture) p->beginChangeGesture();
    p->setValueNotifyingHost (juce::jlimit (0.f, 1.f, v));
    if (gesture) p->endChangeGesture();
}

void JackpotProcessor::beginMapGesture()
{
    pushTrail();
    mapGesture = true;
    apvts.getParameter ("map_x")->beginChangeGesture();
    apvts.getParameter ("map_y")->beginChangeGesture();
}

void JackpotProcessor::endMapGesture()
{
    if (! mapGesture) return;
    mapGesture = false;
    apvts.getParameter ("map_x")->endChangeGesture();
    apvts.getParameter ("map_y")->endChangeGesture();
}

void JackpotProcessor::setMap (float x, float y)
{
    setParam (apvts.getParameter ("map_x"), x, ! mapGesture);
    setParam (apvts.getParameter ("map_y"), y, ! mapGesture);
    timerCallback(); // respond immediately while dragging
}

void JackpotProcessor::teleport()
{
    pushTrail();
    auto& r = juce::Random::getSystemRandom();
    setMeta ("region", 1 + r.nextInt (99998));
    lastMapX = lastMapY = -1.f; // force a refresh even if the cursor lands on the same spot
    setParam (apvts.getParameter ("map_x"), 0.2f + r.nextFloat() * 0.6f, true);
    setParam (apvts.getParameter ("map_y"), 0.2f + r.nextFloat() * 0.6f, true);
    timerCallback();
}

void JackpotProcessor::goTo (const jackpot::Genome& g)
{
    pushTrail();
    evo.apply (g);
    setMeta ("onMap", false);
    setMeta ("gen", generation() + 1);
    ++changeCounter;
}

static juce::ValueTree listNode (juce::ValueTree& state, const char* name)
{
    return state.getOrCreateChildWithName (name, nullptr);
}

void JackpotProcessor::pushTrail()
{
    auto t = listNode (apvts.state, "TRAIL");
    juce::ValueTree e ("E");
    e.setProperty ("g", evo.current().encode(), nullptr);
    t.appendChild (e, nullptr);
    while (t.getNumChildren() > 40) t.removeChild (0, nullptr);
}

std::vector<JackpotProcessor::Entry> JackpotProcessor::trail() const
{
    std::vector<Entry> out;
    auto t = apvts.state.getChildWithName ("TRAIL");
    for (auto e : t)
    {
        auto g = upgrade (jackpot::Genome::decode (e["g"].toString()));
        if ((int) g.v.size() == numParams()) out.push_back ({ g, evo.name (g) });
    }
    return out;
}

std::vector<JackpotProcessor::Entry> JackpotProcessor::kept() const
{
    std::vector<Entry> out;
    auto t = apvts.state.getChildWithName ("KEPT");
    for (auto e : t)
    {
        auto g = upgrade (jackpot::Genome::decode (e["g"].toString()));
        if ((int) g.v.size() == numParams()) out.push_back ({ g, evo.name (g) });
    }
    return out;
}

bool JackpotProcessor::back()
{
    auto t = listNode (apvts.state, "TRAIL");
    if (t.getNumChildren() == 0) return false;
    auto e = t.getChild (t.getNumChildren() - 1);
    auto g = upgrade (jackpot::Genome::decode (e["g"].toString()));
    t.removeChild (e, nullptr);
    if ((int) g.v.size() != numParams()) return false;
    evo.apply (g);
    setMeta ("onMap", false);
    ++changeCounter;
    return true;
}

jackpot::Genome JackpotProcessor::upgrade (jackpot::Genome g) const
{
    // sounds saved before an effect existed: the new effect starts at its default (off)
    for (int i = (int) g.v.size(); i < numParams() && ! g.v.empty(); ++i)
        g.v.push_back (params[(size_t) i]->getDefaultValue());
    return g;
}

static bool sameGenome (const jackpot::Genome& a, const jackpot::Genome& b)
{
    if (a.v.size() != b.v.size()) return false;
    for (size_t i = 0; i < a.v.size(); ++i) if (std::abs (a.v[i] - b.v[i]) > 0.002f) return false;
    return true;
}

bool JackpotProcessor::isCurrentKept() const
{
    auto cur = evo.current();
    for (auto& k : kept()) if (sameGenome (k.g, cur)) return true;
    return false;
}

void JackpotProcessor::keepCurrent()
{
    if (isCurrentKept()) return;
    auto t = listNode (apvts.state, "KEPT");
    juce::ValueTree e ("E");
    e.setProperty ("g", evo.current().encode(), nullptr);
    t.appendChild (e, nullptr);
    while (t.getNumChildren() > 24) t.removeChild (0, nullptr);
    presets.save (getPatchName());   // also lands in Documents/ODDKIN/Presets
    ++changeCounter;
}

void JackpotProcessor::loadState (const juce::ValueTree& newState, bool keepLocks)
{
    if (! newState.hasType (apvts.state.getType())) return;
    auto copy = newState.createCopy();
    if (keepLocks)
        for (int m = 1; m <= kNumFx; ++m)
        {
            juce::Identifier id (juce::String ("lock_") + moduleKey ((Module) m));
            copy.setProperty (id, apvts.state.getProperty (id, false), nullptr);
        }
    // keep the session's trail and kept list when loading a single patch
    for (auto name : { "TRAIL", "KEPT" })
    {
        auto mine = apvts.state.getChildWithName (name);
        if (mine.isValid() && ! copy.getChildWithName (name).isValid())
            copy.appendChild (mine.createCopy(), nullptr);
    }
    // patches saved before an effect existed: give the new effect its default (off)
    for (auto& spec : paramSpecs())
    {
        bool found = false;
        for (auto c : copy)
            if (c.hasType ("PARAM") && c["id"].toString() == spec.id) { found = true; break; }
        if (! found)
        {
            juce::ValueTree pt ("PARAM");
            pt.setProperty ("id", spec.id, nullptr);
            pt.setProperty ("value", spec.def, nullptr);
            copy.appendChild (pt, nullptr);
        }
    }
    apvts.replaceState (copy);
    syncMap();
    ++changeCounter;
}

void JackpotProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void JackpotProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            loadState (juce::ValueTree::fromXml (*xml), false);
}

juce::AudioProcessorEditor* JackpotProcessor::createEditor()
{
    return new JackpotEditor (*this);
}

#if ! ODDKIN_TEST_BUILD
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JackpotProcessor();
}
#endif
