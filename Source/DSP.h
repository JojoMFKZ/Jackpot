#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <array>

// ============================================================================
//  MANTIS DSP modules. All process a stereo block in place.
//  Parameters are set from the audio thread once per block (setters), and
//  smoothed per sample inside each module where it matters.
// ============================================================================
namespace jackpot::dsp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float onePoleCoef (float timeSec, double sr) { return timeSec <= 0.f ? 1.f : 1.f - std::exp (-1.f / (timeSec * (float) sr)); }
inline float lpCoef (float hz, double sr) { return 1.f - std::exp (-kTwoPi * hz / (float) sr); }

struct Smooth
{
    float v = 0.f, target = 0.f, c = 0.01f;
    void setTime (float sec, double sr) { c = onePoleCoef (sec, sr); }
    void snap (float x) { v = target = x; }
    float next() { v += c * (target - v); return v; }
};

struct DCBlock
{
    float x1 = 0.f, y1 = 0.f, r = 0.995f;
    void prepare (double sr) { r = 1.f - (kTwoPi * 12.f / (float) sr); }
    void reset() { x1 = y1 = 0.f; }
    float process (float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

struct OnePoleLP
{
    float z = 0.f, a = 1.f;
    void set (float hz, double sr) { a = lpCoef (juce::jlimit (5.f, (float) sr * 0.49f, hz), sr); }
    float process (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void set (float hz, double sr) { lp.set (hz, sr); }
    float process (float x) { return x - lp.process (x); }
    void reset() { lp.reset(); }
};

// Smooth random function: glides between random points. Used for drift,
// tape wow and anything that should never repeat.
struct SmoothNoise
{
    juce::Random rng;
    float from = 0.f, to = 0.f, phase = 1.f;
    void seed (juce::int64 s) { rng.setSeed (s); to = rng.nextFloat() * 2.f - 1.f; }
    // inc = rate / sampleRate (or per-block increment)
    float next (float inc)
    {
        phase += inc;
        if (phase >= 1.f)
        {
            phase -= std::floor (phase);
            from = to;
            to = rng.nextFloat() * 2.f - 1.f;
        }
        float t = phase * phase * (3.f - 2.f * phase); // smoothstep
        return from + (to - from) * t;
    }
};

// circular buffer with cubic (Hermite) interpolated reads
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int size = 1;
        while (size < minSize) size <<= 1;
        buf.assign ((size_t) size, 0.f);
        mask = size - 1;
        w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }
    int size() const { return mask + 1; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float at (int i) const { return buf[(size_t) (i & mask)]; }

    // read 'delay' samples behind the most recently written sample
    float read (float delay) const
    {
        delay = juce::jlimit (1.f, (float) (mask - 4), delay);
        float pos = (float) w - 1.f - delay;
        int i = (int) std::floor (pos);
        float f = pos - (float) i;
        float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + c0;
    }
};

// ============================================================================
//  NOISE GATE — on the input, so hum and hiss between notes don't get fuzzed,
//  resonated and reverbed. Opens fast, holds a moment, closes smoothly, with
//  6 dB of hysteresis so it doesn't chatter. Off at the bottom of the knob.
// ============================================================================
class Gate
{
public:
    float thresholdDb = -80.f;

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { env = 0.f; gain = 1.f; open = true; holdLeft = 0; }
    bool isOpen() const { return open; }
    float currentGain() const { return gain; }

    void process (float* L, float* R, int n)
    {
        if (thresholdDb <= -79.5f) { gain = 1.f; open = true; return; }   // off
        const float thrOpen = juce::Decibels::decibelsToGain (thresholdDb);
        const float thrClose = thrOpen * 0.5f;
        const float rel = std::exp (-1.f / (0.05f * (float) sr));      // envelope release 50 ms
        const float gAtk = 1.f - std::exp (-1.f / (0.0005f * (float) sr));
        const float gRel = 1.f - std::exp (-1.f / (0.08f * (float) sr));
        const int hold = (int) (0.06 * sr);
        for (int i = 0; i < n; ++i)
        {
            const float x = juce::jmax (std::abs (L[i]), std::abs (R[i]));
            env = x > env ? x : env * rel;
            if (env > thrOpen) { open = true; holdLeft = hold; }
            else if (env < thrClose) { if (holdLeft > 0) --holdLeft; else open = false; }
            const float target = open ? 1.f : 0.f;
            gain += (target > gain ? gAtk : gRel) * (target - gain);
            L[i] *= gain; R[i] *= gain;
        }
    }

private:
    double sr = 44100.0;
    float env = 0.f, gain = 1.f;
    bool open = true;
    int holdLeft = 0;
};

// ============================================================================
//  FUZZ — two cascaded clipping stages, coupling caps, scooped tone stack.
//  Starve = dying-battery sputter, Octave = rectifier octave-up.
// ============================================================================
class Fuzz
{
public:
    float sustain = 0.6f, tone = 0.5f, starve = 0.f, octave = 0.f, blend = 1.f;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        os.initProcessing ((size_t) maxBlock);
        dry.setSize (2, maxBlock);
        double osr = sr * 4.0;
        for (auto& c : ch)
        {
            c.inHP.set (90.f, osr);
            c.coupleHP.set (160.f, osr);
            c.toneLP.set (480.f, osr);
            c.toneHP.set (1100.f, osr);
            c.post.set (7000.f, osr);
            c.env = 0.f;
        }
        envA = onePoleCoef (0.002f, osr);
        envR = onePoleCoef (0.060f, osr);
        sm.setTime (0.03f, osr);
        reset();
    }

    void reset()
    {
        os.reset();
        for (auto& c : ch) { c.inHP.reset(); c.coupleHP.reset(); c.toneLP.reset(); c.toneHP.reset(); c.post.reset(); c.env = 0.f; c.gate = 1.f; }
    }

    void process (float* L, float* R, int n)
    {
        dry.copyFrom (0, 0, L, n);
        dry.copyFrom (1, 0, R, n);

        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        auto up = os.processSamplesUp (block);

        const float g1 = juce::Decibels::decibelsToGain (6.f + sustain * 42.f);
        const float g2 = juce::Decibels::decibelsToGain (4.f + sustain * 22.f);
        const float thr = 0.002f + starve * starve * 0.09f;
        const float bias = starve * 0.45f;
        sm.target = tone;

        for (int c = 0; c < 2; ++c)
        {
            auto* d = up.getChannelPointer ((size_t) c);
            auto& s = ch[c];
            float tsm = sm.v;
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float x = s.inHP.process (d[i]);

                // envelope for the starve gate
                float a = std::abs (x);
                s.env += (a > s.env ? envA : envR) * (a - s.env);

                // rectifier octave
                x = x + octave * (std::abs (x) * 1.7f - x);

                // stage 1 — asymmetric soft diode clip
                float y = x * g1 + bias;
                y = y > 0.f ? std::tanh (y) : std::tanh (y * 0.7f) * 1.2f;
                y -= std::tanh (bias);
                y = s.coupleHP.process (y);

                // stage 2 — symmetric
                y = std::tanh (y * g2);

                // starve gate: sputters as the note decays
                if (starve > 0.001f)
                {
                    float gt = juce::jlimit (0.f, 1.f, (s.env - thr) / thr);
                    s.gate += 0.02f * (gt - s.gate);
                    y *= s.gate;
                }

                // muff-style tone: LP <-> HP crossfade leaves a mid scoop at noon
                tsm += sm.c * (sm.target - tsm);
                float lo = s.toneLP.process (y);
                float hi = s.toneHP.process (y);
                y = lo * (1.f - tsm) + hi * tsm * 1.5f;
                d[i] = s.post.process (y) * 0.6f;
            }
            if (c == 1) sm.v = tsm;
        }

        os.processSamplesDown (block);

        if (blend < 0.999f)
            for (int i = 0; i < n; ++i)
            {
                L[i] = dry.getSample (0, i) * (1.f - blend) + L[i] * blend;
                R[i] = dry.getSample (1, i) * (1.f - blend) + R[i] * blend;
            }
    }

private:
    struct Ch { OnePoleHP inHP, coupleHP, toneHP; OnePoleLP toneLP, post; float env = 0.f, gate = 1.f; };
    std::array<Ch, 2> ch;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::AudioBuffer<float> dry;
    double sr = 44100.0;
    float envA = 0.1f, envR = 0.01f;
    Smooth sm;
};

// ============================================================================
//  CRUSH — "character" box: sample-rate reduction, bit depth, wavefolder,
//  ring modulator. Aliasing is on purpose.
// ============================================================================
class Crush
{
public:
    float bits = 8.f, rate = 0.2f, fold = 0.f, ring = 0.f, ringFreq = 220.f;

    void prepare (double sampleRate, int) { sr = sampleRate; fSm.setTime (0.05f, sr); reset(); }
    void reset() { for (auto& c : ch) { c.hold = 0.f; c.count = 0.f; } ringPh = 0.f; fSm.snap (ringFreq); }

    void process (float* L, float* R, int n)
    {
        const float factor = 1.f + rate * rate * 63.f;
        const float levels = std::pow (2.f, bits - 1.f);
        const float foldG = 1.f + fold * 7.f;
        const float foldMix = juce::jmin (1.f, fold * 4.f);
        fSm.target = ringFreq;
        float* io[2] = { L, R };

        for (int i = 0; i < n; ++i)
        {
            float f = fSm.next();
            ringPh += f / (float) sr;
            if (ringPh >= 1.f) ringPh -= 1.f;

            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                float x = io[c][i];

                s.count += 1.f;
                if (s.count >= factor) { s.count -= factor; s.hold = x; }
                float y = s.hold;

                if (bits < 15.9f)
                    y = std::round (y * levels) / levels;

                if (fold > 0.001f)
                {
                    float folded = std::sin (y * foldG * kPi * 0.5f);
                    y += foldMix * (folded - y);
                }

                if (ring > 0.001f)
                {
                    float m = std::sin (kTwoPi * (ringPh + (c ? 0.25f : 0.f)));
                    y = y * (1.f - ring) + y * m * ring * 1.4f;
                }
                io[c][i] = y;
            }
        }
    }

private:
    struct Ch { float hold = 0.f, count = 0.f; };
    std::array<Ch, 2> ch;
    double sr = 44100.0;
    float ringPh = 0.f;
    Smooth fSm;
};

// ============================================================================
//  Two-head granular pitch shifter (the classic delay-line kind).
// ============================================================================
class GrainShifter
{
public:
    void prepare (double sampleRate) { sr = sampleRate; line.prepare ((int) (sr * 0.5)); phase = 0.f; }
    void reset() { line.reset(); phase = 0.f; }

    // ratio: playback speed, window in samples
    float process (float x, float ratio, float window)
    {
        line.push (x);
        phase += (1.f - ratio) / window;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.f) p2 -= 1.f;
        float a = line.read (phase * window + 1.f);
        float b = line.read (p2 * window + 1.f);
        float wa = std::sin (kPi * phase), wb = std::sin (kPi * p2);
        return a * wa * wa + b * wb * wb;
    }

private:
    DelayLine line;
    double sr = 44100.0;
    float phase = 0.f;
};

class Pitch
{
public:
    float semis = 12.f, detuneCents = 0.f, grain = 0.4f, feedback = 0.f, mix = 0.5f;

    void prepare (double sampleRate, int) { sr = sampleRate; for (auto& s : sh) s.prepare (sr); semSm.setTime (0.08f, sr); reset(); }
    void reset() { for (auto& s : sh) s.reset(); fb[0] = fb[1] = 0.f; semSm.snap (semis); jitter.seed (juce::Random::getSystemRandom().nextInt64()); }

    void process (float* L, float* R, int n)
    {
        semSm.target = semis;
        float* io[2] = { L, R };
        const float baseWin = (0.012f + grain * grain * 0.16f) * (float) sr;
        for (int i = 0; i < n; ++i)
        {
            float st = semSm.next();
            // tiny window jitter so grains never line up the same way twice
            float win = baseWin * (1.f + 0.15f * grain * jitter.next (3.f / (float) sr));
            for (int c = 0; c < 2; ++c)
            {
                float cents = c == 0 ? detuneCents : -detuneCents;
                float ratio = std::pow (2.f, (st + cents / 100.f) / 12.f);
                float x = io[c][i];
                float wet = sh[c].process (x + std::tanh (fb[c] * feedback), ratio, win);
                fb[c] = wet;
                io[c][i] = x * (1.f - mix) + wet * mix;
            }
        }
    }

private:
    std::array<GrainShifter, 2> sh;
    double sr = 44100.0;
    float fb[2] = { 0.f, 0.f };
    Smooth semSm;
    SmoothNoise jitter;
};

// ============================================================================
//  FILTER — morphing state-variable filter (LP > BP > HP > Notch) with an
//  envelope follower and an LFO that morphs from sine to sample & hold.
// ============================================================================
class Filter
{
public:
    float cutoff = 1200.f, reso = 0.4f, morph = 0.f, envAmt = 0.5f, lfoRate = 1.f, lfoDepth = 0.f, lfoShape = 0.f;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        cutSm.setTime (0.02f, sr);
        morphSm.setTime (0.05f, sr);
        envA = onePoleCoef (0.004f, sr);
        envR = onePoleCoef (0.15f, sr);
        reset();
    }
    void reset()
    {
        for (auto& s : st) { s.ic1 = s.ic2 = 0.f; s.env = 0.f; s.sh = 0.f; s.shSm = 0.f; }
        lfoPh = 0.f; cutSm.snap (cutoff); morphSm.snap (morph);
    }

    void process (float* L, float* R, int n)
    {
        cutSm.target = cutoff;
        morphSm.target = morph;
        float* io[2] = { L, R };
        const float k = 2.f - 1.96f * reso;
        const float shSmooth = onePoleCoef (0.004f, sr);

        for (int i = 0; i < n; ++i)
        {
            float base = cutSm.next();
            float m = morphSm.next();
            float prev = lfoPh;
            lfoPh += lfoRate / (float) sr;
            if (lfoPh >= 1.f) lfoPh -= 1.f;

            // morph weights
            float w[4] = { 0, 0, 0, 0 };
            int mi = juce::jlimit (0, 2, (int) m);
            float mf = juce::jlimit (0.f, 1.f, m - (float) mi);
            w[mi] = 1.f - mf; w[mi + 1] = mf;

            for (int c = 0; c < 2; ++c)
            {
                auto& s = st[c];
                float x = io[c][i];
                float a = std::abs (x);
                s.env += (a > s.env ? envA : envR) * (a - s.env);

                float ph = lfoPh + (c ? 0.25f : 0.f);
                if (ph >= 1.f) ph -= 1.f;
                // new S&H value at each cycle start (per channel offset)
                float pph = prev + (c ? 0.25f : 0.f); if (pph >= 1.f) pph -= 1.f;
                if (ph < pph) s.sh = rng.nextFloat() * 2.f - 1.f;
                s.shSm += shSmooth * (s.sh - s.shSm);
                float lfo = std::sin (kTwoPi * ph) * (1.f - lfoShape) + s.shSm * lfoShape;

                float envN = juce::jmin (1.5f, s.env * 5.f);
                float oct = envAmt * envN * 5.f + lfo * lfoDepth * 4.f;
                float fc = juce::jlimit (20.f, (float) sr * 0.45f, base * std::pow (2.f, oct));

                float g = std::tan (kPi * fc / (float) sr);
                float a1 = 1.f / (1.f + g * (g + k));
                float a2 = g * a1, a3 = g * a2;
                float v0 = std::tanh (x * 0.9f) / 0.9f;
                float v3 = v0 - s.ic2;
                float v1 = a1 * s.ic1 + a2 * v3;
                float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
                s.ic1 = 2.f * v1 - s.ic1;
                s.ic2 = 2.f * v2 - s.ic2;
                // keep it from exploding at extreme settings
                s.ic1 = juce::jlimit (-8.f, 8.f, s.ic1);
                s.ic2 = juce::jlimit (-8.f, 8.f, s.ic2);

                float lp = v2, bp = v1 * k, hp = v0 - k * v1 - v2, notch = lp + hp;
                io[c][i] = lp * w[0] + bp * w[1] * 1.3f + hp * w[2] + notch * w[3];
            }
        }
    }

private:
    struct St { float ic1 = 0.f, ic2 = 0.f, env = 0.f, sh = 0.f, shSm = 0.f; };
    std::array<St, 2> st;
    double sr = 44100.0;
    float envA = 0.1f, envR = 0.01f, lfoPh = 0.f;
    Smooth cutSm, morphSm;
    juce::Random rng;
};

// ============================================================================
//  TAPE — wow (slow random + sine), flutter, saturation, age (darkening,
//  hiss) and random dropouts.
// ============================================================================
class Tape
{
public:
    float wow = 0.2f, flutter = 0.15f, sat = 0.3f, age = 0.2f, dropout = 0.f;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& c : ch) { c.line.prepare ((int) (sr * 0.25)); c.dc.prepare (sr); }
        dropSm.setTime (0.006f, sr);
        reset();
    }
    void reset()
    {
        for (auto& c : ch) { c.line.reset(); c.lp1.reset(); c.lp2.reset(); c.hp.reset(); c.dc.reset(); c.hiss.reset(); }
        auto& r = juce::Random::getSystemRandom();
        wowN.seed (r.nextInt64()); flutN.seed (r.nextInt64());
        wowPh = flutPh = 0.f; dropLeft = 0; dropSm.snap (1.f);
    }

    void process (float* L, float* R, int n)
    {
        const float cutoff = 18000.f * std::pow (2400.f / 18000.f, age);
        const float hpHz = 20.f + age * 110.f;
        for (auto& c : ch) { c.lp1.set (cutoff, sr); c.lp2.set (cutoff * 1.3f, sr); c.hp.set (hpHz, sr); c.hiss.set (6000.f, sr); }
        const float drive = 1.f + sat * 9.f;
        const float norm = 1.f / std::sqrt (drive);
        const float hissAmt = age * age * 0.012f;
        const float baseDelay = 0.006f * (float) sr;
        const float wowDepth = wow * wow * 0.007f * (float) sr;
        const float flutDepth = flutter * 0.0005f * (float) sr;
        const float wowRate = 0.35f + wow * 0.9f;
        const float dropChance = dropout * dropout * 2.5f / (float) sr;
        float* io[2] = { L, R };

        for (int i = 0; i < n; ++i)
        {
            wowPh += wowRate / (float) sr; if (wowPh >= 1.f) wowPh -= 1.f;
            flutPh += (8.f + 3.f * flutN.next (0.7f / (float) sr)) / (float) sr; if (flutPh >= 1.f) flutPh -= 1.f;
            float wowV = 0.5f * (std::sin (kTwoPi * wowPh) + wowN.next (wowRate * 0.6f / (float) sr)) * 0.5f + 0.5f;
            float flV = std::sin (kTwoPi * flutPh) * 0.5f + 0.5f;

            // dropouts: the tape loses contact for a moment
            if (dropLeft <= 0 && dropout > 0.001f && rng.nextFloat() < dropChance)
            {
                dropLeft = (int) ((0.03f + rng.nextFloat() * 0.25f) * (float) sr);
                dropSm.target = rng.nextFloat() * 0.35f;
            }
            if (dropLeft > 0 && --dropLeft == 0) dropSm.target = 1.f;
            float dg = dropSm.next();

            float d = baseDelay + wowV * wowDepth + flV * flutDepth + (1.f - dg) * 0.0006f * (float) sr;

            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                s.line.push (io[c][i]);
                float y = s.line.read (d + (c ? 3.f : 0.f));
                y = std::tanh (y * drive + 0.06f * sat) * norm;
                y = s.dc.process (y);
                y = s.lp2.process (s.lp1.process (y));
                y = s.hp.process (y);
                y += s.hiss.process (rng.nextFloat() * 2.f - 1.f) * hissAmt;
                io[c][i] = y * dg;
            }
        }
    }

private:
    struct Ch { DelayLine line; OnePoleLP lp1, lp2, hiss; OnePoleHP hp; DCBlock dc; };
    std::array<Ch, 2> ch;
    double sr = 44100.0;
    SmoothNoise wowN, flutN;
    float wowPh = 0.f, flutPh = 0.f;
    int dropLeft = 0;
    Smooth dropSm;
    juce::Random rng;
};

// ============================================================================
//  STUTTER — tempo-synced buffer repeats. Each repeat rolls its own dice for
//  reverse, half/double speed or a tape-stop slowdown.
// ============================================================================
class Stutter
{
public:
    float size = 0.5f, prob = 0.3f, reverse = 0.2f, warp = 0.f;
    double bpm = 120.0;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& b : rec) b.prepare ((int) (sr * 6.0));
        mixSm.setTime (0.003f, sr);
        reset();
    }
    void reset() { for (auto& b : rec) b.reset(); counter = 0; repeating = false; repeatsLeft = 0; mixSm.snap (0.f); }

    void process (float* L, float* R, int n)
    {
        static const float divs[] = { 1.f / 64.f, 1.f / 32.f, 1.f / 16.f, 1.f / 8.f, 1.f / 4.f, 1.f / 2.f };
        int di = juce::jlimit (0, 5, juce::roundToInt (size * 5.f));
        int sliceLen = juce::jmax (64, (int) (divs[di] * 4.0 * 60.0 / juce::jlimit (30.0, 300.0, bpm) * sr));
        sliceLen = juce::jmin (sliceLen, rec[0].size() / 2 - 8);
        const float fade = (float) juce::jmin (sliceLen / 4, (int) (0.003 * sr));
        float* io[2] = { L, R };

        for (int i = 0; i < n; ++i)
        {
            rec[0].push (L[i]);
            rec[1].push (R[i]);

            if (++counter >= sliceLen)
            {
                counter = 0;
                if (repeating && repeatsLeft > 0)
                {
                    --repeatsLeft;
                    rollDice();
                }
                else if (rng.nextFloat() < prob)
                {
                    repeating = true;
                    loopStart = rec[0].w - sliceLen;     // the slice that just went by
                    loopLen = sliceLen;
                    repeatsLeft = 1 + rng.nextInt (1 + (int) (prob * 6.f));
                    rollDice();
                }
                else
                {
                    repeating = false;
                }
                playPos = 0.f;
            }

            mixSm.target = repeating ? 1.f : 0.f;
            float m = mixSm.next();
            if (m < 1e-4f) continue;

            // read position inside the captured slice
            float p = playPos;
            if (tapeStop) speed = juce::jmax (0.f, speed - 1.f / (float) loopLen);
            playPos += speed;
            if (playPos >= (float) loopLen) playPos -= (float) loopLen;

            float local = rev ? (float) loopLen - 1.f - p : p;
            float win = juce::jmin (1.f, juce::jmin (p, (float) loopLen - p) / juce::jmax (1.f, fade));
            int idx = loopStart + (int) local;
            float frac = local - std::floor (local);

            for (int c = 0; c < 2; ++c)
            {
                float a = rec[c].at (idx), b = rec[c].at (idx + 1);
                float y = (a + (b - a) * frac) * win;
                io[c][i] = io[c][i] * (1.f - m) + y * m;
            }
        }
    }

private:
    void rollDice()
    {
        rev = rng.nextFloat() < reverse;
        tapeStop = false;
        speed = 1.f;
        if (rng.nextFloat() < warp)
        {
            static const float speeds[] = { 0.5f, 2.f, 0.75f, 1.5f, 0.25f };
            if (rng.nextFloat() < 0.3f) tapeStop = true;
            else speed = speeds[rng.nextInt (5)];
        }
    }

    std::array<DelayLine, 2> rec;
    double sr = 44100.0;
    int counter = 0, loopStart = 0, loopLen = 1, repeatsLeft = 0;
    bool repeating = false, rev = false, tapeStop = false;
    float playPos = 0.f, speed = 1.f;
    Smooth mixSm;
    juce::Random rng;
};

// ============================================================================
//  DELAY — tape-ish delay with warble, a reverse head and a saturating
//  feedback loop that can be pushed past 100% into self-oscillation.
// ============================================================================
class Delay
{
public:
    float timeMs = 380.f, feedback = 0.35f, tone = 0.6f, warble = 0.2f, reverse = 0.f, mix = 0.3f;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& c : ch) { c.line.prepare ((int) (sr * 5.0)); c.dc.prepare (sr); }
        timeSm.setTime (0.25f, sr);
        reset();
    }
    void reset()
    {
        for (auto& c : ch) { c.line.reset(); c.lp.reset(); c.hp.reset(); c.dc.reset(); c.fb = 0.f; c.rk = 0.f; c.rD = 1000.f; }
        timeSm.snap (timeMs * 0.001f * (float) sr);
        wN.seed (juce::Random::getSystemRandom().nextInt64());
        wPh = 0.f;
    }

    void process (float* L, float* R, int n)
    {
        timeSm.target = timeMs * 0.001f * (float) sr;
        const float lpHz = 900.f * std::pow (18.f, tone);
        for (auto& c : ch) { c.lp.set (lpHz, sr); c.hp.set (70.f, sr); }
        const float wRate = 0.4f + warble * 2.5f;
        float* io[2] = { L, R };

        for (int i = 0; i < n; ++i)
        {
            float D = timeSm.next();
            wPh += wRate / (float) sr; if (wPh >= 1.f) wPh -= 1.f;
            float mod = (std::sin (kTwoPi * wPh) * 0.6f + wN.next (wRate / (float) sr) * 0.4f) * warble * warble * 0.006f * (float) sr;

            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[c];
                float Dc = juce::jmax (8.f, D * (c ? 1.06f : 1.f) + mod * (c ? -1.f : 1.f));
                float fwd = s.line.read (Dc);

                float tap = fwd;
                if (reverse > 0.001f)
                {
                    // two crossfaded backwards-reading heads
                    if (s.rk >= s.rD) { s.rk = 0.f; s.rD = Dc; }
                    float k1 = s.rk, k2 = std::fmod (s.rk + s.rD * 0.5f, s.rD);
                    float r1 = s.line.read (2.f * k1 + 2.f), r2 = s.line.read (2.f * k2 + 2.f);
                    float w1 = std::sin (kPi * k1 / s.rD), w2 = std::sin (kPi * k2 / s.rD);
                    float rv = r1 * w1 * w1 + r2 * w2 * w2;
                    s.rk += 1.f;
                    tap = fwd + reverse * (rv - fwd);
                }

                float x = io[c][i];
                float f = s.hp.process (s.lp.process (tap));
                f = std::tanh (f * feedback * 1.1f) / 1.1f;
                s.line.push (s.dc.process (x + f));
                io[c][i] = x * (1.f - 0.4f * mix) + tap * mix;
            }
        }
    }

private:
    struct Ch { DelayLine line; OnePoleLP lp; OnePoleHP hp; DCBlock dc; float fb = 0.f, rk = 0.f, rD = 1000.f; };
    std::array<Ch, 2> ch;
    double sr = 44100.0;
    Smooth timeSm;
    SmoothNoise wN;
    float wPh = 0.f;
};

// ============================================================================
//  HARMONY — two extra voices at musical intervals (pitch-shifted copies),
//  spread across the stereo field. Turns single notes into chords.
// ============================================================================
struct HarmonyShape { const char* name; float a, b; };
inline const std::array<HarmonyShape, 8>& harmonyShapes()
{
    static const std::array<HarmonyShape, 8> s { {
        { "Octaves", 12.f, -12.f }, { "Fifth", 7.f, 12.f }, { "Power", -12.f, 7.f }, { "Major", 4.f, 7.f },
        { "Minor", 3.f, 7.f }, { "Sus", 5.f, 7.f }, { "Major 7", 4.f, 11.f }, { "Wide", 7.f, 19.f } } };
    return s;
}

class Harmony
{
public:
    float shape = 1.f, detune = 0.2f, spread = 0.6f, tone = 0.6f, mix = 0.5f;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& v : voice) v.prepare (sr);
        for (auto& sm : semSm) sm.setTime (0.06f, sr);
        reset();
    }
    void reset()
    {
        for (auto& v : voice) v.reset();
        for (auto& l : lp) l.reset();
        auto& sh = harmonyShapes()[(size_t) juce::jlimit (0, 7, (int) std::round (shape))];
        semSm[0].snap (sh.a); semSm[1].snap (sh.b);
    }

    void process (float* L, float* R, int n)
    {
        auto& sh = harmonyShapes()[(size_t) juce::jlimit (0, 7, (int) std::round (shape))];
        semSm[0].target = sh.a; semSm[1].target = sh.b;
        const float cents = detune * 18.f;
        const float lpHz = 1500.f * std::pow (10.f, tone);
        for (auto& l : lp) l.set (lpHz, sr);
        const float win = 0.045f * (float) sr;
        // voice A leans left, voice B leans right
        const float pa = 0.5f - 0.5f * spread, pb = 0.5f + 0.5f * spread;
        const float gA = 0.75f, gB = 0.6f;
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.5f * (L[i] + R[i]);
            float ra = std::pow (2.f, (semSm[0].next() + cents / 100.f) / 12.f);
            float rb = std::pow (2.f, (semSm[1].next() - cents / 100.f) / 12.f);
            float a = lp[0].process (voice[0].process (x, ra, win)) * gA;
            float b = lp[1].process (voice[1].process (x, rb, win)) * gB;
            float wl = a * std::sqrt (1.f - pa) + b * std::sqrt (1.f - pb);
            float wr = a * std::sqrt (pa) + b * std::sqrt (pb);
            L[i] = L[i] * (1.f - 0.3f * mix) + wl * mix;
            R[i] = R[i] * (1.f - 0.3f * mix) + wr * mix;
        }
    }

private:
    std::array<GrainShifter, 2> voice;
    std::array<OnePoleLP, 2> lp;
    std::array<Smooth, 2> semSm;
    double sr = 44100.0;
};

// ============================================================================
//  RESONATOR — four tuned "strings" per side (comb filters with damping),
//  tuned to a chord in a key. Whatever goes in makes them ring in tune, so
//  noise, drums, synths and fuzz all come out melodic.
// ============================================================================
struct ChordShape { const char* name; std::array<float, 4> semis; };
inline const std::array<ChordShape, 8>& resonatorChords()
{
    static const std::array<ChordShape, 8> c { {
        { "Root + 5th", { 0.f, 7.f, 12.f, 19.f } }, { "Major", { 0.f, 4.f, 7.f, 12.f } }, { "Minor", { 0.f, 3.f, 7.f, 12.f } },
        { "Sus2", { 0.f, 2.f, 7.f, 12.f } }, { "Major 7", { 0.f, 4.f, 7.f, 11.f } }, { "Minor 7", { 0.f, 3.f, 7.f, 10.f } },
        { "Octaves", { -12.f, 0.f, 12.f, 24.f } }, { "Fifths", { 0.f, 7.f, 14.f, 21.f } } } };
    return c;
}

class Resonator
{
public:
    float key = 4.f, chord = 0.f, reg = 0.5f, decay = 0.5f, bright = 0.6f, mix = 0.5f;

    void prepare (double sampleRate, int)
    {
        sr = sampleRate;
        for (auto& s : str) s.line.prepare ((int) (sr / 20.0) + 8);
        inHP[0].set (60.f, sr); inHP[1].set (60.f, sr);
        reset();
    }
    void reset()
    {
        for (auto& s : str) { s.line.reset(); s.lp.reset(); s.dc.reset(); s.dc.prepare (sr); s.lenSm.snap (0.f); }
        for (auto& h : inHP) h.reset();
        first = true;
    }

    void process (float* L, float* R, int n)
    {
        auto& ch = resonatorChords()[(size_t) juce::jlimit (0, 7, (int) std::round (chord))];
        const int k = ((int) std::round (key) % 12 + 12) % 12;
        const int octave = 1 + juce::jlimit (0, 3, (int) std::floor (reg * 3.999f));   // root in octave 1..4
        const float t60 = 0.15f * std::pow (40.f, decay);                                // 0.15 s .. 6 s
        const float lpHz = 900.f * std::pow (16.f, bright);
        for (int s = 0; s < 8; ++s)
        {
            auto& st = str[(size_t) s];
            const float midi = 12.f * (float) (octave + 1) + (float) k + ch.semis[(size_t) (s % 4)] + (s < 4 ? -0.04f : 0.04f);
            const float f = juce::jlimit (25.f, 3000.f, 440.f * std::pow (2.f, (midi - 69.f) / 12.f));
            const float len = (float) sr / f;
            if (first) st.lenSm.snap (len); else st.lenSm.target = len;
            st.lenSm.setTime (0.03f, sr);
            st.g = std::pow (0.001f, 1.f / (f * t60));
            st.in = std::sqrt (1.f - st.g * st.g) * 0.5f;
            st.lp.set (juce::jmax (lpHz, f * 1.5f), sr);
        }
        first = false;

        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float x = inHP[c].process (io[c][i]);
                float wet = 0.f;
                for (int s = c * 4; s < c * 4 + 4; ++s)
                {
                    auto& st = str[(size_t) s];
                    float y = x * st.in + st.g * st.lp.process (st.line.read (st.lenSm.next() - 1.f));
                    y = soft (y);
                    st.line.push (y);
                    wet += st.dc.process (y);
                }
                io[c][i] = io[c][i] * (1.f - 0.6f * mix) + wet * 0.55f * mix;
            }
    }

private:
    static float soft (float y) { return std::abs (y) < 1.2f ? y : std::copysign (1.2f + 0.3f * std::tanh ((std::abs (y) - 1.2f) / 0.3f), y); }
    struct Str { DelayLine line; OnePoleLP lp; DCBlock dc; Smooth lenSm; float g = 0.f, in = 0.f; };
    std::array<Str, 8> str;
    std::array<OnePoleHP, 2> inHP;
    double sr = 44100.0;
    bool first = true;
};

// ============================================================================
//  REVERB — Freeverb core with an octave-up shimmer feedback loop and freeze.
// ============================================================================
class Verb
{
public:
    float size = 0.6f, damp = 0.5f, width = 1.f, shimmer = 0.f, mix = 0.25f;
    bool freeze = false;
    float testFb = -1.f; // calibration only

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        rv.setSampleRate (sr);
        wet.setSize (2, maxBlock);
        shimBuf.setSize (2, maxBlock);
        for (auto& s : sh) s.prepare (sr);
        reset();
    }
    void reset()
    {
        rv.reset(); for (auto& s : sh) s.reset(); shimBuf.clear(); prevN = 0;
        for (auto& h : hp) h.reset(); for (auto& l : lp) l.reset();
        envFb = 0.f;
    }

    void process (float* L, float* R, int n)
    {
        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + size * 0.69f;
        p.damping = damp;
        p.width = width;
        p.wetLevel = 1.f;
        p.dryLevel = 0.f;
        p.freezeMode = freeze ? 1.f : 0.f;
        rv.setParameters (p);
        for (auto& h : hp) h.set (250.f, sr);
        for (auto& l : lp) l.set (7000.f, sr);

        // Shimmer is a loop: reverb out -> octave up -> back into the reverb.
        // Big, undamped rooms amplify a lot, so the loop gain comes from a
        // measured table of the most each size/damping can take without running
        // away (tools/RenderTest calibrate), with 30% headroom. Tails always die.
        const float shim = juce::jlimit (0.f, 1.f, shimmer);
        float fbGain = shim * juce::jmin (0.6f, 0.7f * stableShimmer (size, damp));
        if (envFb > 0.2f) fbGain *= 0.2f / envFb;               // backstop: never let the loop get loud
        if (testFb >= 0.f) fbGain = testFb;

        float* w0 = wet.getWritePointer (0);
        float* w1 = wet.getWritePointer (1);
        const float* s0 = shimBuf.getReadPointer (0);
        const float* s1 = shimBuf.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            float a = i < prevN ? s0[i] : 0.f, b = i < prevN ? s1[i] : 0.f;
            w0[i] = L[i] + a * fbGain;
            w1[i] = R[i] + b * fbGain;
        }
        rv.processStereo (w0, w1, n);

        const float win = 0.08f * (float) sr;
        float pfb = 0.f;
        for (int i = 0; i < n; ++i)
        {
            if (shim > 0.001f)
            {
                float a = lp[0].process (hp[0].process (sh[0].process (w0[i], 2.f, win)));
                float b = lp[1].process (hp[1].process (sh[1].process (w1[i], 2.f, win)));
                shimBuf.setSample (0, i, a);
                shimBuf.setSample (1, i, b);
                pfb += (a * a + b * b) * fbGain * fbGain;
            }
            L[i] = L[i] * (1.f - 0.5f * mix) + w0[i] * mix * 0.7f;
            R[i] = R[i] * (1.f - 0.5f * mix) + w1[i] * mix * 0.7f;
        }
        prevN = shim > 0.001f ? n : 0;

        const float kf = 1.f - std::exp (-(float) n / (0.1f * (float) sr));
        envFb  += kf * (std::sqrt (pfb / (2.f * (float) n)) - envFb);
    }

    // largest stable shimmer loop gain, measured at size 0/.25/.5/.75/.9/1 x damp 0/.5/1
    static float stableShimmer (float size, float damp)
    {
        static const float sz[6] = { 0.f, 0.25f, 0.5f, 0.75f, 0.9f, 1.f };
        static const float t[6][3] = { { 1.178f, 1.491f, 1.711f }, { 0.973f, 1.409f, 1.632f }, { 0.729f, 1.289f, 1.550f },
                                       { 0.410f, 1.148f, 1.406f }, { 0.249f, 1.052f, 1.280f }, { 0.164f, 0.861f, 0.987f } };
        size = juce::jlimit (0.f, 1.f, size); damp = juce::jlimit (0.f, 1.f, damp);
        int i = 0; while (i < 4 && size > sz[i + 1]) ++i;
        float fs = (size - sz[i]) / (sz[i + 1] - sz[i]);
        int j = damp < 0.5f ? 0 : 1; float fd = damp < 0.5f ? damp * 2.f : (damp - 0.5f) * 2.f;
        auto at = [&] (int a, int b) { return t[a][b] + (t[a][b + 1] - t[a][b]) * fd; };
        return at (i, j) + (at (i + 1, j) - at (i, j)) * fs;
    }

private:
    juce::Reverb rv;
    juce::AudioBuffer<float> wet;
    std::array<GrainShifter, 2> sh;
    std::array<OnePoleHP, 2> hp;
    std::array<OnePoleLP, 2> lp;
    juce::AudioBuffer<float> shimBuf;
    int prevN = 0;
    float envFb = 0.f;
    double sr = 44100.0;
};
} // namespace jackpot::dsp
