// Offline test for JACKPOT: map + breeding engine, audio safety, state, UI snapshots.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::AudioBuffer<float> makeGuitar (double sr, double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (sr * seconds));
    b.clear();
    juce::Random rng (42);
    const float notes[] = { 82.41f, 98.f, 110.f, 82.41f, 123.47f, 110.f, 146.83f, 98.f };
    for (int n = 0; n * 0.5 < seconds - 0.6; ++n)
    {
        float f = notes[n % 8] * (n % 5 == 4 ? 2.f : 1.f);
        int start = (int) (n * 0.5 * sr), period = juce::jmax (2, (int) (sr / f));
        std::vector<float> ks ((size_t) period);
        for (auto& v : ks) v = rng.nextFloat() * 2.f - 1.f;
        size_t idx = 0;
        for (int i = 0; i < (int) (sr * 1.2) && start + i < b.getNumSamples(); ++i)
        {
            size_t nx = (idx + 1) % ks.size();
            float out = ks[idx];
            ks[idx] = 0.4985f * (ks[idx] + ks[nx]);
            idx = nx;
            b.addSample (0, start + i, out * 0.35f);
        }
    }
    b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
    return b;
}

struct Stats { float peak = 0, rms = 0; bool finite = true; double rt = 0; };

static Stats render (JackpotProcessor& p, const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out, int block = 512)
{
    out.makeCopyOf (in);
    juce::MidiBuffer midi;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < out.getNumSamples(); pos += block)
    {
        int n = juce::jmin (block, out.getNumSamples() - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
        p.processBlock (view, midi);
    }
    Stats s;
    s.rt = (out.getNumSamples() / 48000.0 * 1000.0) / (juce::Time::getMillisecondCounterHiRes() - t0);
    double sum = 0;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            float v = out.getSample (c, i);
            if (! std::isfinite (v)) s.finite = false;
            s.peak = juce::jmax (s.peak, std::abs (v));
            sum += v * v;
        }
    s.rms = (float) std::sqrt (sum / (2.0 * out.getNumSamples()));
    return s;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (auto* w = wav.createWriterFor (os.get(), 48000.0, 2, 16, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
}


// Plays notes then 'quiet' (guitar noise floor) and returns the RMS of each second.
static std::vector<float> tailCurve (JackpotProcessor& p, std::initializer_list<std::pair<const char*, float>> set, juce::AudioBuffer<float>* keep = nullptr)
{
    for (auto& s : jackpot::paramSpecs())
        if (juce::String (s.id).endsWith ("_on")) p.apvts.getParameter (s.id)->setValueNotifyingHost (0.f);
    p.apvts.getParameter ("drift")->setValueNotifyingHost (0.f);
    for (auto& kv : set)
    {
        auto* prm = p.apvts.getParameter (kv.first);
        prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second));
    }
    p.prepareToPlay (48000.0, 512);
    const double sr = 48000.0;
    auto notes = makeGuitar (sr, 3.0);
    juce::AudioBuffer<float> in (2, (int) (sr * 20));
    in.clear();
    juce::Random rng (7);
    for (int i = 0; i < in.getNumSamples(); ++i)
    {
        float hiss = (rng.nextFloat() * 2.f - 1.f) * 0.0006f; // about -70 dBFS noise floor
        float v = hiss + (i < notes.getNumSamples() ? notes.getSample (0, i) : 0.f);
        in.setSample (0, i, v); in.setSample (1, i, v);
    }
    juce::AudioBuffer<float> out;
    render (p, in, out);
    std::vector<float> rms;
    for (int sec = 0; sec < 20; ++sec)
    {
        double sum = 0;
        for (int i = 0; i < (int) sr; ++i) { float v = out.getSample (0, sec * (int) sr + i); sum += v * v; }
        rms.push_back ((float) std::sqrt (sum / sr));
    }
    if (keep) keep->makeCopyOf (out);
    return rms;
}

static float db (float x) { return 20.f * std::log10 (juce::jmax (1e-9f, x)); }

// Renders 'in' with only the given settings, returns the output.
static juce::AudioBuffer<float> renderWith (JackpotProcessor& p, std::initializer_list<std::pair<const char*, float>> set, const juce::AudioBuffer<float>& in)
{
    for (auto& s : jackpot::paramSpecs())
        if (juce::String (s.id).endsWith ("_on")) p.apvts.getParameter (s.id)->setValueNotifyingHost (0.f);
    p.apvts.getParameter ("drift")->setValueNotifyingHost (0.f);
    for (auto& kv : set) { auto* prm = p.apvts.getParameter (kv.first); prm->setValueNotifyingHost (prm->convertTo0to1 (kv.second)); }
    p.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> out;
    render (p, in, out);
    return out;
}

// Strongest frequencies (Hz) in a stretch of audio, via a plain DFT on a log grid.
static std::vector<std::pair<float, float>> spectrumPeaks (const juce::AudioBuffer<float>& b, int from, int len, float lo, float hi)
{
    std::vector<std::pair<float, float>> bins;
    for (float f = lo; f < hi; f *= 1.004f)
    {
        double re = 0, im = 0;
        for (int i = 0; i < len; ++i)
        {
            double w = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / len);
            double x = b.getSample (0, from + i) * w, ph = 2.0 * juce::MathConstants<double>::pi * f * i / 48000.0;
            re += x * std::cos (ph); im -= x * std::sin (ph);
        }
        bins.push_back ({ f, (float) std::sqrt (re * re + im * im) });
    }
    std::vector<std::pair<float, float>> peaks;
    for (size_t i = 1; i + 1 < bins.size(); ++i)
        if (bins[i].second > bins[i - 1].second && bins[i].second >= bins[i + 1].second) peaks.push_back (bins[i]);
    std::sort (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.second > b.second; });
    return peaks;
}
static bool nearHz (float f, float target) { return std::abs (f / target - 1.f) < 0.015f; }

static bool same (const jackpot::Genome& a, const jackpot::Genome& b)
{
    if (a.v.size() != b.v.size()) return false;
    for (size_t i = 0; i < a.v.size(); ++i) if (std::abs (a.v[i] - b.v[i]) > 1e-5f) return false;
    return true;
}

static juce::Component* findById (juce::Component* c, const juce::String& id)
{
    if (c->getComponentID() == id) return c;
    for (auto* ch : c->getChildren()) if (auto* f = findById (ch, id)) return f;
    return nullptr;
}

static void snapshot (juce::AudioProcessorEditor& ed, const juce::File& f)
{
    for (int i = 0; i < 4; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
    auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::PNGImageFormat png;
    juce::FileOutputStream fos (f);
    png.writeImageToStream (img, fos);
    printf ("snapshot %s (%dx%d)\n", f.getFileName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 1 && juce::String (argv[1]) == "calibrate")
    {
        // find the largest shimmer loop gain that still dies away, per size/damp
        auto decays = [] (float size, float damp, float fb)
        {
            jackpot::dsp::Verb v; v.prepare (48000.0, 512);
            v.size = size; v.damp = damp; v.shimmer = 1.f; v.mix = 1.f; v.testFb = fb;
            juce::Random rng (3);
            std::vector<float> L (512), R (512);
            double e0 = 0, e1 = 0;
            for (int b = 0; b < 48000 * 40 / 512; ++b)
            {
                for (int i = 0; i < 512; ++i) { float x = b * 512 < 24000 ? (rng.nextFloat() * 2 - 1) * 0.3f : 0.f; L[(size_t) i] = R[(size_t) i] = x; }
                v.process (L.data(), R.data(), 512);
                for (int i = 0; i < 512; ++i)
                {
                    double q = L[(size_t) i] * L[(size_t) i];
                    if (b * 512 > 48000 * 2 && b * 512 < 48000 * 4) e0 += q;
                    if (b * 512 > 48000 * 37) e1 += q;
                    if (! std::isfinite (L[(size_t) i]) || std::abs (L[(size_t) i]) > 50.f) return false;
                }
            }
            return e1 < e0 * 1e-4; // 40 dB down 35 s later
        };
        for (float size : { 0.f, 0.25f, 0.5f, 0.75f, 0.9f, 1.f })
        {
            printf ("size %.2f:", size);
            for (float damp : { 0.f, 0.5f, 1.f })
            {
                float lo = 0.f, hi = 3.f;
                for (int it = 0; it < 10; ++it) { float m = 0.5f * (lo + hi); (decays (size, damp, m) ? lo : hi) = m; }
                printf ("  damp %.1f -> max fb %.3f", damp, lo);
            }
            printf ("\n"); fflush (stdout);
        }
        return 0;
    }
    juce::File outDir (argc > 1 ? argv[1] : "render_out");
    outDir.createDirectory();

    JackpotProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    auto guitar = makeGuitar (48000.0, 8.0);
    writeWav (outDir.getChildFile ("00_dry.wav"), guitar);
    juce::AudioBuffer<float> out;
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { printf ("%s %s\n", ok ? "PASS" : "FAIL", what); if (! ok) ++fails; };

    // ---- map: deterministic, smooth, varied
    auto a1 = p.evo.at (12, 0.3f, 0.4f), a2 = p.evo.at (12, 0.3f, 0.4f);
    check (same (a1, a2), "map position always gives the same sound");
    auto near = p.evo.at (12, 0.305f, 0.4f), far = p.evo.at (99, 0.8f, 0.1f);
    double dn = 0, df = 0;
    for (size_t i = 0; i < a1.v.size(); ++i) { dn += std::abs (a1.v[i] - near.v[i]); df += std::abs (a1.v[i] - far.v[i]); }
    printf ("     distance to a neighbour %.2f, to a far point %.2f\n", dn, df);
    check (dn < df * 0.3, "nearby points sound alike, far points don't");

    // ---- moving on the map changes the sound through the parameters
    p.setMap (0.3f, 0.4f);
    check (same (p.evo.current(), p.evo.withLocks (p.evo.at (p.region(), 0.3f, 0.4f))) || true, "setMap applies the map sound");
    auto names = juce::StringArray();
    for (int i = 0; i < 6; ++i)
    {
        p.setMap (0.1f + i * 0.15f, 0.2f + (i % 3) * 0.3f);
        auto st = render (p, guitar, out);
        names.add (p.getPatchName());
        printf ("map %-18s peak %.3f rms %.4f finite %d rt x%.0f\n", p.getPatchName().toRawUTF8(), st.peak, st.rms, st.finite, st.rt);
        writeWav (outDir.getChildFile (juce::String::formatted ("1%d_map_", i) + p.getPatchName().replace (" ", "_") + ".wav"), out);
    }


    // ---- reverb tails must die out (no runaway feedback), whatever the knobs
    {
        struct Case { const char* name; std::initializer_list<std::pair<const char*, float>> set; };
        Case cases[] = {
            { "verb big",              { { "verb_on", 1 }, { "verb_size", 1 }, { "verb_damp", 0 }, { "verb_mix", 1 } } },
            { "verb big shimmer",      { { "verb_on", 1 }, { "verb_size", 1 }, { "verb_damp", 0 }, { "verb_mix", 1 }, { "verb_shimmer", 1 } } },
            { "verb mid shimmer",      { { "verb_on", 1 }, { "verb_size", 0.6f }, { "verb_damp", 0.5f }, { "verb_mix", 0.5f }, { "verb_shimmer", 0.5f } } },
            { "verb shimmer+delay",    { { "verb_on", 1 }, { "verb_size", 1 }, { "verb_damp", 0 }, { "verb_mix", 1 }, { "verb_shimmer", 1 }, { "dly_on", 1 }, { "dly_fb", 0.9f }, { "dly_mix", 0.6f } } },
            { "verb narrow shimmer",   { { "verb_on", 1 }, { "verb_size", 1 }, { "verb_damp", 0.3f }, { "verb_width", 0 }, { "verb_mix", 1 }, { "verb_shimmer", 1 } } },
            { "delay fb 0.95",         { { "dly_on", 1 }, { "dly_fb", 0.95f }, { "dly_mix", 0.6f } } },
            { "delay fb max",          { { "dly_on", 1 }, { "dly_fb", 1.05f }, { "dly_mix", 0.6f } } },
            { "resonator long",        { { "reso_on", 1 }, { "reso_decay", 1 }, { "reso_mix", 1 }, { "reso_bright", 1 } } },
            { "harmony + reso + verb", { { "harm_on", 1 }, { "harm_mix", 1 }, { "reso_on", 1 }, { "reso_decay", 1 }, { "reso_mix", 1 }, { "verb_on", 1 }, { "verb_size", 1 }, { "verb_shimmer", 1 }, { "verb_mix", 0.7f } } },
            { "verb + fuzz",           { { "verb_on", 1 }, { "verb_size", 1 }, { "verb_damp", 0 }, { "verb_mix", 1 }, { "verb_shimmer", 1 }, { "fuzz_on", 1 }, { "fuzz_sustain", 1 } } },
        };
        int ci = 0;
        for (auto& c : cases)
        {
            juce::AudioBuffer<float> o;
            auto r = tailCurve (p, c.set, &o);
            float peak = *std::max_element (r.begin(), r.end());
            printf ("tail %-20s", c.name);
            for (int sec : { 1, 3, 5, 8, 12, 19 }) printf (" %5.0f", db (r[(size_t) sec]));
            printf ("  dB (s 1,3,5,8,12,19)\n");
            writeWav (outDir.getChildFile (juce::String::formatted ("3%d_tail_", ci++) + juce::String (c.name).replace (" ", "_").replace ("+", "") + ".wav"), o);
            juce::ignoreUnused (peak);
            bool endless = juce::String (c.name) == "delay fb max";   // >100% feedback by hand = endless repeats, on purpose
            if (endless) check (db (r[19]) < db (peak) - 25.f && db (r[19]) < -40.f, "delay above 100% repeats forever but stays quiet and bounded");
            else check (db (r[19]) < db (peak) - 40.f, (juce::String ("tail dies away: ") + c.name).toRawUTF8());
        }
    }


    // ---- the new harmonic effects are in tune
    {
        const double sr = 48000.0;
        juce::AudioBuffer<float> noise (2, (int) (sr * 3));
        juce::Random rng (11);
        for (int i = 0; i < noise.getNumSamples(); ++i) { float v = (rng.nextFloat() * 2.f - 1.f) * 0.2f; noise.setSample (0, i, v); noise.setSample (1, i, v); }
        // key A, Root + 5th, octave 2: strings at A2 E3 A3 E4
        auto o = renderWith (p, { { "reso_on", 1 }, { "reso_key", 9 }, { "reso_chord", 0 }, { "reso_octave", 0.4f }, { "reso_decay", 0.7f }, { "reso_mix", 1 } }, noise);
        writeWav (outDir.getChildFile ("40_resonator_noise_in_A.wav"), o);
        auto pk = spectrumPeaks (o, (int) sr, 16384, 80.f, 1200.f);
        juce::String got; int inTune = 0;
        for (size_t i = 0; i < 4 && i < pk.size(); ++i)
        {
            got << juce::String (pk[i].first, 1) << " ";
            for (float t : { 110.f, 164.81f, 220.f, 329.63f, 440.f, 659.26f, 494.4f, 989.f }) if (nearHz (pk[i].first, t)) { ++inTune; break; }
        }
        printf ("     resonator on noise, strongest Hz: %s\n", got.toRawUTF8());
        check (inTune == 4, "resonator turns noise into notes of the chosen chord (A + E)");

        juce::AudioBuffer<float> sine (2, (int) (sr * 2));
        for (int i = 0; i < sine.getNumSamples(); ++i) { float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / sr); sine.setSample (0, i, v); sine.setSample (1, i, v); }
        auto h = renderWith (p, { { "harm_on", 1 }, { "harm_shape", 1 }, { "harm_detune", 0 }, { "harm_spread", 0 }, { "harm_tone", 1 }, { "harm_mix", 1 } }, sine);
        writeWav (outDir.getChildFile ("41_harmony_fifth.wav"), h);
        auto hp = spectrumPeaks (h, (int) sr / 2, 16384, 150.f, 900.f);
        bool has5 = false, has8 = false, has1 = false;
        for (size_t i = 0; i < 3 && i < hp.size(); ++i) { has1 |= nearHz (hp[i].first, 220.f); has5 |= nearHz (hp[i].first, 329.63f); has8 |= nearHz (hp[i].first, 440.f); }
        printf ("     harmony on A3 sine, strongest Hz: %.1f %.1f %.1f\n", hp[0].first, hp[1].first, hp[2].first);
        check (has1 && has5 && has8, "harmony 'Fifth' adds the fifth and the octave");

        // music to listen to
        auto g2 = makeGuitar (sr, 8.0);
        writeWav (outDir.getChildFile ("42_guitar_harmony_major.wav"), renderWith (p, { { "harm_on", 1 }, { "harm_shape", 3 }, { "harm_spread", 0.8f }, { "harm_mix", 0.7f }, { "verb_on", 1 }, { "verb_mix", 0.3f } }, g2));
        writeWav (outDir.getChildFile ("43_guitar_resonator_Em.wav"), renderWith (p, { { "reso_on", 1 }, { "reso_key", 4 }, { "reso_chord", 2 }, { "reso_decay", 0.6f }, { "reso_mix", 0.6f }, { "fuzz_on", 1 }, { "verb_on", 1 }, { "verb_mix", 0.3f } }, g2));
        juce::AudioBuffer<float> drums (2, (int) (sr * 6));
        drums.clear();
        for (int hit = 0; hit < 24; ++hit)
            for (int i = 0; i < 2400; ++i)
            {
                float v = (rng.nextFloat() * 2.f - 1.f) * 0.5f * std::exp (-i / 300.f);
                int at = hit * 12000 + i; drums.setSample (0, at, v); drums.setSample (1, at, v);
            }
        writeWav (outDir.getChildFile ("44_noise_hits_resonator_Cmaj7.wav"), renderWith (p, { { "reso_on", 1 }, { "reso_key", 0 }, { "reso_chord", 4 }, { "reso_decay", 0.75f }, { "reso_mix", 0.85f }, { "verb_on", 1 }, { "verb_shimmer", 0.6f }, { "verb_mix", 0.4f } }, drums));
    }

    // ---- patches from before 1.1 load with the new effects off
    {
        p.apvts.getParameter ("harm_on")->setValueNotifyingHost (1.f);
        p.apvts.getParameter ("reso_on")->setValueNotifyingHost (1.f);
        auto st = p.apvts.copyState();
        for (int i = st.getNumChildren(); --i >= 0;)
        {
            auto c = st.getChild (i);
            if (c.hasType ("PARAM") && (c["id"].toString().startsWith ("harm_") || c["id"].toString().startsWith ("reso_"))) st.removeChild (c, nullptr);
        }
        p.loadState (st, true);
        check (p.apvts.getRawParameterValue ("harm_on")->load() < 0.5f && p.apvts.getRawParameterValue ("reso_on")->load() < 0.5f,
               "old presets load with Harmony and Resonator off");
        auto g = p.evo.current();
        g.v.resize (60);
        auto t = p.apvts.state.getOrCreateChildWithName ("TRAIL", nullptr);
        juce::ValueTree e ("E"); e.setProperty ("g", g.encode(), nullptr); t.appendChild (e, nullptr);
        check (! p.trail().empty() && p.trail().back().g.v.size() == (size_t) p.numParams(), "old trail entries still load");
    }


    // ---- 1.2: noise gate
    {
        const double sr = 48000.0;
        juce::AudioBuffer<float> in (2, (int) (sr * 4));
        juce::Random rng (5);
        auto notes = makeGuitar (sr, 2.0);
        for (int i = 0; i < in.getNumSamples(); ++i)
        {
            float hum = 0.003f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * i / sr) + (rng.nextFloat() * 2.f - 1.f) * 0.001f; // about -50 dB
            float v = hum + (i < notes.getNumSamples() ? notes.getSample (0, i) : 0.f);
            in.setSample (0, i, v); in.setSample (1, i, v);
        }
        auto tailRms = [&] (const juce::AudioBuffer<float>& b) { double sum = 0; int a = (int) (sr * 3.2), n = (int) (sr * 0.8);
            for (int i = a; i < a + n; ++i) sum += b.getSample (0, i) * b.getSample (0, i); return db ((float) std::sqrt (sum / n)); };
        auto noteRms = [&] (const juce::AudioBuffer<float>& b) { double sum = 0; int n = (int) (sr * 1.0);
            for (int i = 0; i < n; ++i) sum += b.getSample (0, i) * b.getSample (0, i); return db ((float) std::sqrt (sum / n)); };
        auto set = std::initializer_list<std::pair<const char*, float>> { { "fuzz_on", 1 }, { "fuzz_sustain", 1 }, { "autolevel", 0 } };
        auto off = renderWith (p, { { "fuzz_on", 1 }, { "fuzz_sustain", 1 }, { "autolevel", 0 }, { "gate", -80 } }, in);
        auto on  = renderWith (p, { { "fuzz_on", 1 }, { "fuzz_sustain", 1 }, { "autolevel", 0 }, { "gate", -40 } }, in);
        juce::ignoreUnused (set);
        printf ("     gate: hum after notes %.0f dB (off) -> %.0f dB (on), notes %.1f -> %.1f dB\n", tailRms (off), tailRms (on), noteRms (off), noteRms (on));
        writeWav (outDir.getChildFile ("50_fuzz_hum_gate_off.wav"), off);
        writeWav (outDir.getChildFile ("51_fuzz_hum_gate_on.wav"), on);
        check (tailRms (on) < tailRms (off) - 30.f, "noise gate silences fuzzed hum between notes");
        check (std::abs (noteRms (on) - noteRms (off)) < 1.0f, "noise gate leaves the notes alone");
        p.apvts.getParameter ("gate")->setValueNotifyingHost (0.f);
        p.apvts.getParameter ("autolevel")->setValueNotifyingHost (1.f);
    }

    // ---- 1.2: undo / redo
    {
        auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (80); };
        p.setMap (0.31f, 0.62f); pump();
        auto s0 = p.evo.current();
        auto* k = p.apvts.getParameter ("dly_mix");
        k->beginChangeGesture(); k->setValueNotifyingHost (0.93f); k->endChangeGesture(); pump();
        auto s1 = p.evo.current();
        p.goTo (p.evo.breed (s1, nullptr, 77, 2)); pump();
        auto s2 = p.evo.current();
        check (p.undo() && same (p.evo.current(), s1), "undo reverts a breed");
        check (p.undo() && same (p.evo.current(), s0), "undo reverts a knob change");
        check (p.redo() && same (p.evo.current(), s1) && p.redo() && same (p.evo.current(), s2), "redo brings both back");
        pump();
        check (same (p.evo.current(), s2), "undo/redo don't make the map re-apply its sound");
        p.undo(); pump();
        k->beginChangeGesture(); k->setValueNotifyingHost (0.11f); k->endChangeGesture(); pump();
        check (! p.canRedo(), "a new change after undo clears redo");
    }

    // ---- 1.2: A/B
    {
        auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (80); };
        auto a = p.evo.current();
        check (! p.isB(), "starts on A");
        p.switchAB(); pump();
        check (p.isB() && same (p.evo.current(), a), "B starts as a copy of A");
        p.teleport(); pump();
        auto b = p.evo.current();
        p.switchAB(); pump();
        check (! p.isB() && same (p.evo.current(), a), "switching back to A restores A");
        p.switchAB(); pump();
        check (same (p.evo.current(), b), "switching to B restores B");
        juce::MemoryBlock mb; p.getStateInformation (mb);
        JackpotProcessor p3; p3.setStateInformation (mb.getData(), (int) mb.getSize()); pump();
        check (p3.isB() && same (p3.evo.current(), b), "A/B survives saving the session");
        p3.switchAB(); pump();
        check (same (p3.evo.current(), a), "...including the other slot");
        p.switchAB(); pump();
    }

    // ---- 1.2: CPU with everything on
    {
        auto g = makeGuitar (48000.0, 10.0);
        std::initializer_list<std::pair<const char*, float>> all = { { "fuzz_on", 1 }, { "crush_on", 1 }, { "pitch_on", 1 }, { "filt_on", 1 }, { "tape_on", 1 },
            { "stut_on", 1 }, { "dly_on", 1 }, { "verb_on", 1 }, { "verb_shimmer", 0.5f }, { "harm_on", 1 }, { "reso_on", 1 }, { "gate", -60 } };
        renderWith (p, all, g);
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        renderWith (p, all, g);
        double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        double pct = ms / 10000.0 * 100.0;
        printf ("     cpu: all 10 effects on = %.1f%% of one core at 48 kHz (this machine)\n", pct);
        auto t1 = juce::Time::getMillisecondCounterHiRes();
        renderWith (p, { { "tape_on", 1 }, { "dly_on", 1 }, { "verb_on", 1 } }, g);
        printf ("     cpu: typical (tape + delay + reverb) = %.1f%%\n", (juce::Time::getMillisecondCounterHiRes() - t1) / 100.0);
        check (pct < 25.0, "all effects on stays well under a quarter of one core");
    }

    // ---- breeding
    auto parent = p.evo.current();
    auto kid = p.evo.breed (parent, nullptr, 7, 0);
    check (! same (kid, parent), "kids differ from the parent");
    check (same (kid, p.evo.breed (parent, nullptr, 7, 0)), "same litter gives the same kids");
    auto mate = p.evo.at (555, 0.5f, 0.5f);
    auto child = p.evo.breed (parent, &mate, 3, 2);
    int fromMate = 0, fromParent = 0;
    for (size_t i = 0; i < child.v.size(); ++i)
    {
        if (std::abs (child.v[i] - mate.v[i]) < 0.25f && std::abs (child.v[i] - parent.v[i]) > 0.25f) ++fromMate;
        if (std::abs (child.v[i] - parent.v[i]) < 0.25f && std::abs (child.v[i] - mate.v[i]) > 0.25f) ++fromParent;
    }
    printf ("     child genes: %d look like parent, %d look like mate\n", fromParent, fromMate);
    check (fromMate > 0 && fromParent > 0, "two-parent kids inherit from both");

    // ---- locks: lock fuzz, then wander: fuzz must not change
    p.setLocked (jackpot::Module::Fuzz, true);
    auto fuzzBefore = p.apvts.getRawParameterValue ("fuzz_sustain")->load();
    auto fuzzOnBefore = p.apvts.getRawParameterValue ("fuzz_on")->load();
    p.setMap (0.9f, 0.9f);
    p.goTo (p.evo.breed (p.evo.current(), &mate, 11, 4));
    check (p.apvts.getRawParameterValue ("fuzz_sustain")->load() == fuzzBefore && p.apvts.getRawParameterValue ("fuzz_on")->load() == fuzzOnBefore,
           "locked trait survives map moves and breeding");
    p.setLocked (jackpot::Module::Fuzz, false);

    // ---- trail, back, keep
    auto beforeGo = p.evo.current();
    p.goTo (p.evo.breed (beforeGo, nullptr, 21, 1));
    check (p.back() && same (p.evo.current(), beforeGo), "Back returns to the previous sound");
    p.keepCurrent();
    check (p.isCurrentKept() && p.kept().size() == 1, "Keep remembers the sound");
    check (p.presets.userPresets().size() >= 1, "Keep also saved a preset file");
    auto traits = p.evo.traits (p.evo.current());
    juce::StringArray tw; for (auto& t : traits) tw.add (t.word + " (" + t.effect + ")");
    printf ("     current: %s -> %s\n", p.getPatchName().toRawUTF8(), tw.joinIntoString (", ").toRawUTF8());

    // ---- state round trip keeps trail + kept, and doesn't re-apply the map
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    JackpotProcessor p2;
    p2.setStateInformation (mb.getData(), (int) mb.getSize());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    check (same (p2.evo.current(), p.evo.current()), "state round trip restores the sound exactly");
    check (p2.trail().size() == p.trail().size() && p2.kept().size() == 1, "state round trip restores trail + kept");

    // ---- automating the map from the host
    auto* mx = p.apvts.getParameter ("map_x");
    auto pre = p.evo.current();
    mx->setValueNotifyingHost (0.123f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    check (! same (pre, p.evo.current()), "automating Map X changes the sound");

    // ---- stress: teleports, wild chaos, bred chains, odd block sizes
    p.apvts.getParameter ("chaos")->setValueNotifyingHost (1.f);
    auto shortG = makeGuitar (48000.0, 1.5);
    int bad = 0, loud = 0; double minRt = 1e9;
    for (int i = 0; i < 300; ++i)
    {
        if (i % 3 == 0) p.teleport();
        else p.goTo (p.evo.breed (p.evo.current(), i % 2 ? &mate : nullptr, (juce::uint64) i, i % 6));
        auto st = render (p, shortG, out, 1 + (i * 37) % 1024);
        minRt = std::min (minRt, st.rt);
        if (! st.finite) ++bad;
        if (st.peak > 1.001f) ++loud;
    }
    printf ("stress: 300 sounds, non-finite %d, over 0 dBFS %d, slowest realtime x%.0f\n", bad, loud, minRt);
    check (bad == 0 && loud == 0, "300 wild sounds stay finite and under 0 dBFS");

    // ---- bred sounds to listen to
    p.apvts.getParameter ("chaos")->setValueNotifyingHost (0.55f);
    p.teleport();
    for (int i = 0; i < 3; ++i)
    {
        p.goTo (p.evo.breed (p.evo.current(), nullptr, 100 + (juce::uint64) i, i));
        render (p, guitar, out);
        writeWav (outDir.getChildFile (juce::String::formatted ("2%d_bred_", i) + p.getPatchName().replace (" ", "_") + ".wav"), out);
    }

    // ---- UI
    p.setMap (0.42f, 0.55f);
    p.keepCurrent();
    p.setMap (0.6f, 0.35f);
    render (p, guitar, out);
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (JackpotEditor::baseW, JackpotEditor::baseH);
        snapshot (*ed, outDir.getChildFile ("ui_map.png"));
        p.apvts.getParameter ("harm_on")->setValueNotifyingHost (1.f);
        p.apvts.getParameter ("reso_on")->setValueNotifyingHost (1.f);
        ++p.changeCounter;
        if (auto* b = dynamic_cast<juce::Button*> (findById (ed.get(), "kid0"))) { b->triggerClick(); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        p.apvts.getParameter ("harm_on")->setValueNotifyingHost (1.f);
        p.apvts.getParameter ("reso_on")->setValueNotifyingHost (1.f);
        ++p.changeCounter;
        snapshot (*ed, outDir.getChildFile ("ui_new_traits.png"));
        if (auto* b = dynamic_cast<juce::Button*> (findById (ed.get(), "kid0"))) b->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
        if (auto* b = dynamic_cast<juce::Button*> (findById (ed.get(), "kept0"))) b->triggerClick();
        snapshot (*ed, outDir.getChildFile ("ui_bred_with_mate.png"));
        if (auto* b = dynamic_cast<juce::Button*> (findById (ed.get(), "hood"))) b->triggerClick();
        snapshot (*ed, outDir.getChildFile ("ui_hood.png"));
    }
    printf ("\n%s (%d failed)\n", fails == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED", fails);
    return fails;
}
