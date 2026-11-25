#pragma once
// The Freak: 6-band dynamic EQ.
// Each band is a bell or shelf with a STATIC gain plus a DYNAMIC part that only moves when the band
// misbehaves compared with its own recent average (about 3 s): a negative DEPTH cuts when the band jumps
// more than THRESHOLD dB above that average, a positive DEPTH lifts when it sinks more than THRESHOLD below.
// Because the threshold is relative, a band set on a quiet intro still behaves in the loud chorus.
// Zero latency, stereo linked (detector on mid).
//
// KEY (per band): the band listens to the SIDECHAIN instead of its own signal, and moves by DEPTH while the
// key is playing in that range. That's unmasking: a guitar keyed by the vocal makes room in the vocal's
// range only while the singer sings, and keeps its full tone between the lines. With KEY on, THRESHOLD is how
// far under its normal level the key may drop and still count as playing.
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace freak
{
constexpr int kBands = 6;
enum Type { bell = 0, lowShelf = 1, highShelf = 2 };

struct Band
{
    bool on = false;
    int type = bell;
    float freq = 1000.0f, q = 1.0f, gainDb = 0.0f, depthDb = -6.0f, thrDb = 4.0f;
    bool key = false;        // listen to the sidechain
};

struct Params
{
    std::array<Band, kBands> bands;
    float amount = 1.0f;     // scales every band's DEPTH (0..2)
    float speed = 0.5f;      // 0 slow .. 1 fast
    float outputDb = 0.0f;
    int listen = 0;          // 0 off, 1..6 = hear that band's detector
};

struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

// RBJ cookbook. Shelves use Q as the shelf "slope" (0.707 = classic).
inline Coefs makeEq (int type, double fs, double f, double q, double gainDb)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    q = juce::jlimit (0.1, 20.0, q);
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs, cw = std::cos (w0), sw = std::sin (w0);
    Coefs c;
    double a0 = 1;
    if (type == bell)
    {
        const double al = sw / (2.0 * q);
        c.b0 = 1 + al * A; c.b1 = -2 * cw; c.b2 = 1 - al * A;
        a0 = 1 + al / A;   c.a1 = -2 * cw; c.a2 = 1 - al / A;
    }
    else
    {
        const double qs = juce::jlimit (0.3, 2.0, q);
        const double al = sw / (2.0 * qs), sa = 2.0 * std::sqrt (A) * al;
        if (type == lowShelf)
        {
            c.b0 = A * ((A + 1) - (A - 1) * cw + sa);
            c.b1 = 2 * A * ((A - 1) - (A + 1) * cw);
            c.b2 = A * ((A + 1) - (A - 1) * cw - sa);
            a0 = (A + 1) + (A - 1) * cw + sa;
            c.a1 = -2 * ((A - 1) + (A + 1) * cw);
            c.a2 = (A + 1) + (A - 1) * cw - sa;
        }
        else
        {
            c.b0 = A * ((A + 1) + (A - 1) * cw + sa);
            c.b1 = -2 * A * ((A - 1) + (A + 1) * cw);
            c.b2 = A * ((A + 1) + (A - 1) * cw - sa);
            a0 = (A + 1) - (A - 1) * cw + sa;
            c.a1 = 2 * ((A - 1) - (A + 1) * cw);
            c.a2 = (A + 1) - (A - 1) * cw - sa;
        }
    }
    c.b0 /= a0; c.b1 /= a0; c.b2 /= a0; c.a1 /= a0; c.a2 /= a0;
    return c;
}

// Detector filter: band-pass for bells, low-pass for a low shelf, high-pass for a high shelf
inline Coefs makeDetector (int type, double fs, double f, double q)
{
    f = juce::jlimit (10.0, fs * 0.45, f);
    const double w0 = juce::MathConstants<double>::twoPi * f / fs, cw = std::cos (w0), sw = std::sin (w0);
    Coefs c;
    double a0;
    if (type == bell)
    {
        const double al = sw / (2.0 * juce::jlimit (0.5, 12.0, q));
        c.b0 = al; c.b1 = 0; c.b2 = -al;  // 0 dB peak band-pass
        a0 = 1 + al; c.a1 = -2 * cw; c.a2 = 1 - al;
    }
    else
    {
        const double al = sw / (2.0 * 0.7071);
        if (type == lowShelf) { c.b0 = (1 - cw) / 2; c.b1 = 1 - cw; c.b2 = (1 - cw) / 2; }
        else                  { c.b0 = (1 + cw) / 2; c.b1 = -(1 + cw); c.b2 = (1 + cw) / 2; }
        a0 = 1 + al; c.a1 = -2 * cw; c.a2 = 1 - al;
    }
    c.b0 /= a0; c.b1 /= a0; c.b2 /= a0; c.a1 /= a0; c.a2 /= a0;
    return c;
}

// |H(f)| in dB, for drawing
inline double magnitudeDb (const Coefs& c, double fs, double f)
{
    const double w = juce::MathConstants<double>::twoPi * f / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
}

struct Biquad
{
    Coefs c;
    double z1 = 0, z2 = 0;
    inline float process (float x)
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
    void reset() { z1 = z2 = 0; }
};

// Modulation-safe EQ band (Andrew Simper / Cytomic state-variable form). Same response as makeEq() above,
// but it stays well behaved when the gain changes every few samples, which a dynamic EQ does all the time.
struct Svf
{
    double a1 = 1, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    double ic1 = 0, ic2 = 0;
    void set (int type, double fs, double f, double q, double gainDb)
    {
        f = juce::jlimit (10.0, fs * 0.45, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        double g = std::tan (juce::MathConstants<double>::pi * f / fs), k;
        if (type == bell)
        {
            k = 1.0 / (juce::jlimit (0.1, 20.0, q) * A);
            m0 = 1; m1 = k * (A * A - 1); m2 = 0;
        }
        else
        {
            k = 1.0 / juce::jlimit (0.3, 2.0, q);
            if (type == lowShelf) { g /= std::sqrt (A); m0 = 1; m1 = k * (A - 1); m2 = A * A - 1; }
            else                  { g *= std::sqrt (A); m0 = A * A; m1 = k * (1 - A) * A; m2 = 1 - A * A; }
        }
        a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline float process (float x)
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1;
        ic2 = 2 * v2 - ic2;
        return (float) (m0 * x + m1 * v1 + m2 * v2);
    }
    void reset() { ic1 = ic2 = 0; }
};

// KEY: how much of DEPTH to use, from how far the key's band sits under its own normal level (over, dB).
// Full depth while the key plays at its normal level, fading out over 6 dB once it drops more than THRESHOLD
// below it, none in silence.
inline float keyedGain (float overDb, float thrDb, float depthDb)
{
    const float presence = juce::jlimit (0.0f, 1.0f, (overDb + thrDb + 6.0f) / 6.0f);
    return depthDb * presence;
}

// The rule that turns "how far the band is from its own average" into a gain move.
// over = band level - band average, in dB. Returns the dynamic gain in dB.
inline float dynamicGain (float overDb, float thrDb, float depthDb)
{
    if (depthDb < 0.0f) return std::max (depthDb, -std::max (0.0f, overDb - thrDb) * 0.75f);            // cut when it jumps
    if (depthDb > 0.0f) return overDb < -30.0f ? 0.0f : std::min (depthDb, std::max (0.0f, -overDb - thrDb) * 0.5f); // lift when it sinks (not in silence)
    return 0.0f;
}

class Engine
{
public:
    static constexpr int kUpdate = 32;   // samples between gain / coefficient updates

    void prepare (double sampleRate, int)
    {
        fs = sampleRate;
        for (auto& b : st) { for (auto& f : b.eq) f.reset(); for (auto& f : b.listenF) f.reset(); b.det.reset(); b.keyDet.reset(); b.env = 0; b.avg = 0; b.age = 0; b.keyEnv = 0; b.keyAvg = 0; b.keyAge = 0; b.dyn = 0; b.applied = 1e9f; }
        keySilent = (juce::int64) (10.0 * fs); keyHeard = 0;
        countdown = 0;
        outGain = juce::Decibels::decibelsToGain (p.outputDb);
        avgCoef = std::exp (-1.0 / (3.0 * fs));    // ~3 s "what is normal for this band"
        fastCoef = std::exp (-1.0 / (0.15 * fs));
        warmSamples = (int) (0.5 * fs);
        primed = false;
        for (auto& d : dynDb) d.store (0.0f);
        for (auto& d : overDb) d.store (-100.0f);
    }

    void setParams (const Params& np)
    {
        for (int i = 0; i < kBands; ++i)
        {
            const auto& a = p.bands[(size_t) i];
            const auto& b = np.bands[(size_t) i];
            if (a.type != b.type || a.freq != b.freq || a.q != b.q || ! primed)
            {
                auto& s = st[(size_t) i];
                s.det.c = makeDetector (b.type, fs, b.freq, b.q);
                s.keyDet.c = s.det.c;
                const auto lc = makeDetector (b.type, fs, b.freq, b.q);
                for (auto& f : s.listenF) f.c = lc;
                s.applied = 1e9f;   // force an EQ coefficient update
            }
        }
        p = np;
        primed = true;
        const double spd = juce::jlimit (0.0f, 1.0f, p.speed);
        const double atkMs = 30.0 * std::pow (0.1, spd);        // 30 ms .. 3 ms
        const double relMs = 400.0 * std::pow (0.15, spd);      // 400 ms .. 60 ms
        atkCoef = std::exp (-1.0 / (atkMs * 0.001 * fs));
        relCoef = std::exp (-1.0 / (relMs * 0.001 * fs));
    }

    // in-place, ch = 1 or 2. side: the sidechain (key), or nullptr
    void process (float* const* data, int numCh, int n, const float* const* side = nullptr, int sideCh = 0)
    {
        const float targetOut = juce::Decibels::decibelsToGain (p.outputDb);
        float inPk = 0, outPk = 0;
        const int listen = p.listen;
        for (int i = 0; i < n; ++i)
        {
            if (countdown <= 0) { updateGains(); countdown = kUpdate; }
            --countdown;

            float x[2] = { data[0][i], numCh > 1 ? data[1][i] : data[0][i] };
            inPk = std::max ({ inPk, std::abs (x[0]), std::abs (x[1]) });
            const float mid = 0.5f * (x[0] + x[1]);
            float key = 0.0f;
            if (side != nullptr && sideCh > 0) key = sideCh > 1 ? 0.5f * (side[0][i] + side[1][i]) : side[0][i];
            if (std::abs (key) > 1e-4f) { keySilent = 0; ++keyHeard; } else ++keySilent;

            for (int b = 0; b < kBands; ++b)
            {
                auto& s = st[(size_t) b];
                const auto& bp = p.bands[(size_t) b];
                if (! bp.on && listen != b + 1) { s.env = s.avg = 0; s.age = 0; s.keyEnv = s.keyAvg = 0; s.keyAge = 0; continue; }
                if (bp.key)
                {
                    const float kd = s.keyDet.process (key);
                    const double kp = (double) kd * kd;
                    s.keyEnv = kp > s.keyEnv ? kp + atkCoef * (s.keyEnv - kp) : kp + relCoef * (s.keyEnv - kp);
                    if (s.keyEnv > 1e-8)   // the key's normal level in this band: learned only while it plays
                    {
                        const double eDb = 10.0 * std::log10 (s.keyEnv);
                        if (s.keyAge < warmSamples) { ++s.keyAge; s.keyAvg = s.keyAge == 1 ? eDb : eDb + fastCoef * (s.keyAvg - eDb); }
                        else s.keyAvg = eDb + avgCoef * (s.keyAvg - eDb);
                    }
                }
                const float d = s.det.process (mid);
                const double pw = (double) d * d;
                s.env = pw > s.env ? pw + atkCoef * (s.env - pw) : pw + relCoef * (s.env - pw);
                // "normal" for this band: only learned while the band has content (silence doesn't drag it down),
                // and learned quickly for the first half second so switching a band on doesn't cause a jump
                if (s.env > 1e-8)
                {
                    const double eDb = 10.0 * std::log10 (s.env);
                    if (s.age < warmSamples) { ++s.age; s.avg = s.age == 1 ? eDb : eDb + fastCoef * (s.avg - eDb); }
                    else s.avg = eDb + avgCoef * (s.avg - eDb);
                }
            }

            float y[2] = { x[0], x[1] };
            if (listen >= 1 && listen <= kBands)
            {
                auto& s = st[(size_t) listen - 1];
                if (p.bands[(size_t) listen - 1].key && side != nullptr && sideCh > 0)   // a KEY band: hear what it listens to, the key
                    for (int c = 0; c < 2; ++c) y[c] = s.listenF[(size_t) c].process (side[juce::jmin (c, sideCh - 1)][i]);
                else
                    for (int c = 0; c < 2; ++c) y[c] = s.listenF[(size_t) c].process (x[c]);
            }
            else
            {
                for (int b = 0; b < kBands; ++b)
                {
                    auto& s = st[(size_t) b];
                    if (! p.bands[(size_t) b].on) continue;
                    for (int c = 0; c < 2; ++c) y[c] = s.eq[(size_t) c].process (y[c]);
                }
            }
            outGain += (targetOut - outGain) * 0.002f;
            for (int c = 0; c < numCh; ++c) { data[c][i] = y[c] * outGain; outPk = std::max (outPk, std::abs (data[c][i])); }
        }
        inPeak.store (std::max (inPeak.load(), inPk));
        outPeak.store (std::max (outPeak.load(), outPk));
    }

    int getLatency() const { return 0; }
    bool keyActive() const { return keySilent < (juce::int64) (3.0 * fs); }   // something on the sidechain in the last 3 s
    double keySeconds() const { return (double) keyHeard / fs; }
    double getSampleRate() const { return fs; }
    const Params& getParams() const { return p; }

    // meters (written on the audio thread, read by the editor / link)
    std::array<std::atomic<float>, kBands> dynDb {};    // current dynamic move per band, dB
    std::array<std::atomic<float>, kBands> overDb {};   // band level vs its own average, dB
    std::atomic<float> inPeak { 0 }, outPeak { 0 };

private:
    void updateGains()
    {
        for (int b = 0; b < kBands; ++b)
        {
            auto& s = st[(size_t) b];
            const auto& bp = p.bands[(size_t) b];
            if (! bp.on) { s.dyn = 0; dynDb[(size_t) b].store (0.0f); overDb[(size_t) b].store (-100.0f); continue; }
            float target = 0.0f, over = -100.0f;
            if (bp.key)
            {
                if (s.keyAge >= warmSamples / 4 && s.keyEnv > 1e-8)
                {
                    over = (float) (10.0 * std::log10 (s.keyEnv) - s.keyAvg);
                    target = keyedGain (over, bp.thrDb, bp.depthDb * p.amount);
                }
            }
            else if (s.age >= warmSamples / 4)   // band has had content long enough to know what's normal
            {
                over = (float) (10.0 * std::log10 (std::max (1e-20, s.env)) - s.avg);
                target = dynamicGain (over, bp.thrDb, bp.depthDb * p.amount);
            }
            s.dyn += (target - s.dyn) * 0.3f;
            if (std::abs (s.dyn) < 1e-4f) s.dyn = 0;
            dynDb[(size_t) b].store (s.dyn);
            overDb[(size_t) b].store (over);
            const float total = bp.gainDb + s.dyn;
            if (std::abs (total - s.applied) > 0.005f)
            {
                s.applied = total;
                s.eq[0].set (bp.type, fs, bp.freq, bp.q, total);
                s.eq[1].set (bp.type, fs, bp.freq, bp.q, total);
            }
        }
    }

    struct State
    {
        std::array<Svf, 2> eq;
        std::array<Biquad, 2> listenF;
        Biquad det, keyDet;
        double env = 0, avg = 0;   // avg: the band's normal level, dB (learned only while it has content)
        double keyEnv = 0, keyAvg = 0;
        int age = 0, keyAge = 0;
        float dyn = 0, applied = 1e9f;
    };
    std::array<State, kBands> st;
    Params p;
    double fs = 48000.0, atkCoef = 0.99, relCoef = 0.999, avgCoef = 0.9999, fastCoef = 0.999;
    int warmSamples = 24000;
    float outGain = 1.0f;
    int countdown = 0;
    bool primed = false;
    juce::int64 keySilent = 0, keyHeard = 0;
};
} // namespace freak
