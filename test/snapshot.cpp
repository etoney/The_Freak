// Headless checks (run locally and in CI):
//  1) DSP: all bands off = untouched, static gain is exact, a band cuts when its range jumps and
//     leaves the rest of the spectrum alone, a positive depth lifts when the range sinks,
//     AMOUNT 0 = static only, LISTEN solos the band, nothing blows up under abuse, zero latency
//  2) factory + user presets, state round trip, link parameter ids
//  3) renders the editor to PNGs
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

static bool writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    if (auto os = f.createOutputStream()) return juce::PNGImageFormat().writeImageToStream (img, *os);
    return false;
}

static void setP (FreakProcessor& p, const juce::String& id, float v)
{
    auto* rp = p.apvts.getParameter (id);
    jassert (rp != nullptr);
    rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

// Generates a test signal through the real processBlock. levelFn(t) gives the amplitude of tone A at time t;
// tone B (optional) is steady. Returns the left channel.
struct Render { std::vector<float> in, out; double sr; };
static Render render (FreakProcessor& p, double seconds, float fA, std::function<float (double)> ampA, float fB = 0, float ampB = 0, double sr = 48000.0)
{
    p.prepareToPlay (sr, 512);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    Render r; r.sr = sr;
    const int total = (int) (seconds * sr);
    double phA = 0, phB = 0;
    for (int done = 0; done < total; done += 512)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (double) (done + i) / sr;
            const float s = ampA ((float) t) * (float) std::sin (phA) + ampB * (float) std::sin (phB);
            phA += juce::MathConstants<double>::twoPi * fA / sr;
            phB += juce::MathConstants<double>::twoPi * fB / sr;
            buf.setSample (0, i, s); buf.setSample (1, i, s);
            r.in.push_back (s);
        }
        p.processBlock (buf, midi);
        r.out.insert (r.out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
    }
    return r;
}

// level of one frequency in a time window (Goertzel-ish DFT with Hann), dB
static double toneDb (const std::vector<float>& v, double sr, double f, double t0, double t1)
{
    const int a = (int) (t0 * sr), b = std::min ((int) v.size(), (int) (t1 * sr));
    double re = 0, im = 0, ws = 0;
    for (int i = a; i < b; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * (double) (i - a) / (double) (b - a - 1));
        re += v[(size_t) i] * w * std::cos (juce::MathConstants<double>::twoPi * f * (double) i / sr);
        im -= v[(size_t) i] * w * std::sin (juce::MathConstants<double>::twoPi * f * (double) i / sr);
        ws += w;
    }
    return 20.0 * std::log10 (std::max (1e-12, 2.0 * std::sqrt (re * re + im * im) / ws));
}
static double gainAt (const Render& r, double f, double t0, double t1) { return toneDb (r.out, r.sr, f, t0, t1) - toneDb (r.in, r.sr, f, t0, t1); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory());
    dir.createDirectory();
    int failures = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (! ok) ++failures; };
    auto fmt = [] (double v) { return juce::String (v, 2); };

    // ---------------- DSP
    {
        FreakProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        check (p.getProgramName (p.getCurrentProgram()) == "Init (All Bands Off)", "opens on Init (All Bands Off)");
        check (p.getLatencySamples() == 0, "zero latency");

        auto r = render (p, 1.0, 1000.0f, [] (double) { return 0.3f; }, 100.0f, 0.2f);
        double maxDiff = 0;
        for (size_t i = 0; i < r.in.size(); ++i) maxDiff = std::max (maxDiff, (double) std::abs (r.in[i] - r.out[i]));
        check (maxDiff < 1e-6, "all bands off: output == input (max diff " + juce::String (maxDiff, 8) + ")");

        // static gain
        setP (p, "b3on", 1); setP (p, "b3f", 1000); setP (p, "b3q", 1.0f); setP (p, "b3g", 6.0f); setP (p, "b3d", 0.0f);
        r = render (p, 1.0, 1000.0f, [] (double) { return 0.2f; });
        const double sg = gainAt (r, 1000, 0.5, 1.0);
        check (std::abs (sg - 6.0) < 0.15, "static +6 dB bell at 1 kHz gives " + fmt (sg) + " dB");

        // dynamic cut: steady 1 kHz, then a +12 dB jump from 4.0 to 4.4 s, with a steady 100 Hz tone underneath
        setP (p, "b3g", 0.0f); setP (p, "b3d", -9.0f); setP (p, "b3t", 3.0f);
        auto jump = [] (double t) { return (t > 4.0 && t < 4.4) ? 0.4f : 0.1f; };
        r = render (p, 5.0, 1000.0f, jump, 100.0f, 0.2f);
        const double steady = gainAt (r, 1000, 3.0, 3.9);
        const double hit = gainAt (r, 1000, 4.1, 4.38);
        const double low = gainAt (r, 100, 4.1, 4.38);
        check (std::abs (steady) < 0.5, "steady band is left alone (" + fmt (steady) + " dB)");
        check (hit < -4.0 && hit > -9.5, "band cuts when it jumps 12 dB (" + fmt (hit) + " dB, depth -9)");
        check (std::abs (low) < 0.5, "100 Hz untouched while the 1 kHz band cuts (" + fmt (low) + " dB)");
        const double after = gainAt (r, 1000, 4.8, 5.0);
        check (std::abs (after) < 1.0, "band lets go after the jump (" + fmt (after) + " dB)");

        // AMOUNT 0 = static only
        setP (p, "amount", 0.0f);
        r = render (p, 5.0, 1000.0f, jump);
        const double hit0 = gainAt (r, 1000, 4.1, 4.38);
        check (std::abs (hit0) < 0.3, "AMOUNT 0%: no dynamic move (" + fmt (hit0) + " dB)");
        setP (p, "amount", 100.0f);

        // positive depth lifts when the band sinks 12 dB
        setP (p, "b3d", 6.0f); setP (p, "b3t", 3.0f);
        auto dip = [] (double t) { return (t > 4.0 && t < 5.0) ? 0.025f : 0.1f; };   // a quiet passage, 1 s
        r = render (p, 5.0, 1000.0f, dip);
        const double lift = gainAt (r, 1000, 4.5, 4.98);
        const double liftSteady = gainAt (r, 1000, 3.0, 3.9);
        check (lift > 2.5 && lift < 6.3, "band lifts when it sinks 12 dB (" + fmt (lift) + " dB, depth +6)");
        check (std::abs (liftSteady) < 0.5, "lift band leaves the steady part alone (" + fmt (liftSteady) + " dB)");

        // silence doesn't count as "sinking": no lift into a pause
        r = render (p, 5.0, 1000.0f, [] (double t) { return (t > 4.0 && t < 4.4) ? 0.0f : 0.1f; });
        float mx = 0; for (size_t i = (size_t) (4.1 * 48000); i < (size_t) (4.38 * 48000); ++i) mx = std::max (mx, std::abs (r.out[i]));
        check (mx < 1e-3f, "no lift into silence");

        // low shelf cut reacts to a bass jump, leaves 3 kHz alone
        setP (p, "b3on", 0);
        setP (p, "b1on", 1); setP (p, "b1type", 1); setP (p, "b1f", 120); setP (p, "b1d", -8.0f); setP (p, "b1t", 3.0f);
        r = render (p, 5.0, 60.0f, jump, 3000.0f, 0.1f);
        const double shelfHit = gainAt (r, 60, 4.1, 4.38), hi = gainAt (r, 3000, 4.1, 4.38);
        check (shelfHit < -3.5, "low shelf cuts a bass jump (" + fmt (shelfHit) + " dB)");
        check (std::abs (hi) < 0.3, "3 kHz untouched by the low shelf (" + fmt (hi) + " dB)");

        // listen: hear only band 1's detector (low-pass at 120 Hz) -> 3 kHz mostly gone
        setP (p, "listen", 1);
        r = render (p, 1.0, 60.0f, [] (double) { return 0.1f; }, 3000.0f, 0.1f);
        const double lstHi = gainAt (r, 3000, 0.5, 1.0), lstLo = gainAt (r, 60, 0.5, 1.0);
        check (lstHi < -30.0 && std::abs (lstLo) < 1.5, "LISTEN solos the band (3 kHz " + fmt (lstHi) + " dB, 60 Hz " + fmt (lstLo) + " dB)");
        setP (p, "listen", 0);

        // abuse: every band on, random sweeps of everything, loud noise
        {
            juce::Random rnd (7);
            for (int b = 0; b < freak::kBands; ++b) setP (p, FreakProcessor::bandId (b, "on"), 1);
            p.prepareToPlay (44100.0, 256);
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            bool finite = true; float peakOut = 0;
            for (int blk = 0; blk < 2000; ++blk)
            {
                if (blk % 20 == 0)
                    for (int b = 0; b < freak::kBands; ++b)
                    {
                        setP (p, FreakProcessor::bandId (b, "f"), 20.0f * std::pow (1000.0f, rnd.nextFloat()));
                        setP (p, FreakProcessor::bandId (b, "q"), 0.3f + rnd.nextFloat() * 7.7f);
                        setP (p, FreakProcessor::bandId (b, "g"), rnd.nextFloat() * 24.0f - 12.0f);
                        setP (p, FreakProcessor::bandId (b, "d"), rnd.nextFloat() * 30.0f - 18.0f);
                        setP (p, FreakProcessor::bandId (b, "type"), (float) rnd.nextInt (3));
                    }
                for (int i = 0; i < 256; ++i) { const float s = (rnd.nextFloat() * 2.0f - 1.0f) * (blk % 300 < 150 ? 0.9f : 0.05f); buf.setSample (0, i, s); buf.setSample (1, i, -s); }
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) { const float y = buf.getSample (c, i); finite &= std::isfinite (y); peakOut = std::max (peakOut, std::abs (y)); }
            }
            check (finite && peakOut < 40.0f, "survives random sweeps with every band on (peak " + juce::String (peakOut, 2) + ")");
        }
    }

    // ---------------- presets, state, link
    {
        FreakProcessor p;
        bool allLoad = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            allLoad &= p.getCurrentPresetName() == p.getProgramName (i);
        }
        check (allLoad && p.getNumPrograms() >= 8, juce::String (p.getNumPrograms()) + " factory presets load");
        for (auto& pr : freak::factoryPresets())
            for (auto& [id, v] : pr.values)
                if (p.apvts.getParameter (id) == nullptr) check (false, "preset " + pr.name + " uses unknown id " + id);

        bool idsOk = true;
        for (auto& id : FreakProcessor::linkIds()) idsOk &= p.apvts.getParameter (id) != nullptr;
        check (idsOk && FreakProcessor::linkIds().size() == 45, "link exposes all " + juce::String (FreakProcessor::linkIds().size()) + " band + global controls");

        p.setCurrentProgram (1);
        setP (p, "b2g", 3.3f);
        p.bandNotes.set (1, "Mud builds up in choruses");
        juce::MemoryBlock mb; p.getStateInformation (mb);
        FreakProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (std::abs (q.apvts.getRawParameterValue ("b2g")->load() - 3.3f) < 0.01f && q.bandNotes[1] == "Mud builds up in choruses"
               && q.getCurrentPresetName() == p.getCurrentPresetName(), "state round trip (params, notes, preset name)");

        auto folder = FreakProcessor::getUserPresetFolder();
        const auto name = juce::String ("zz test ") + juce::String (juce::Random::getSystemRandom().nextInt (100000));
        setP (p, "b5f", 4321.0f); setP (p, "listen", 2);
        const bool saved = p.saveUserPreset (name);
        p.setCurrentProgram (0);
        const bool loaded = p.loadUserPreset (FreakProcessor::fileForPresetName (name));
        check (saved && loaded && std::abs (p.apvts.getRawParameterValue ("b5f")->load() - 4321.0f) < 1.0f
               && (int) p.apvts.getRawParameterValue ("listen")->load() == 0, "user preset round trip (and never saves LISTEN)");
        FreakProcessor::fileForPresetName (name).deleteFile();
        juce::ignoreUnused (folder);
    }

    // ---------------- GUI
    {
        FreakProcessor p;
        p.setPlayConfigDetails (2, 2, 48000.0, 512);
        p.setCurrentProgram (1);   // Master: Polish
        p.bandNotes.set (4, "Harsh 3.8 kHz: jumps 7 dB in the loud parts");
        // feed some music-ish noise so the spectrum and the meters have something to show
        p.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (3);
        float lp = 0, ph = 0;
        for (int blk = 0; blk < 200; ++blk)
        {
            for (int i = 0; i < 512; ++i)
            {
                lp += (rnd.nextFloat() * 2.0f - 1.0f - lp) * 0.08f;
                ph += 0.0072f;
                const float loud = blk > 150 ? 0.9f : 0.25f;
                const float s = (lp * 0.6f + 0.05f * (rnd.nextFloat() - 0.5f) + 0.2f * std::sin (ph * 3.0f)) * loud;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            p.processBlock (buf, midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* fe = dynamic_cast<FreakEditor*> (ed.get());
        fe->selectBand (4);
        for (int i = 0; i < 12; ++i) fe->pumpForSnapshot();
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        check (writePng (img, dir.getChildFile ("gui.png")), "rendered gui.png (" + juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()) + ")");
        const auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        check (writePng (img2, dir.getChildFile ("gui@2x.png")), "rendered gui@2x.png");

        p.setCurrentProgram (0);
        fe->selectBand (0);
        for (int i = 0; i < 4; ++i) fe->pumpForSnapshot();
        check (writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("gui_init.png")), "rendered gui_init.png");
    }

    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
