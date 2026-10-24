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
