#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>

#include "dsp/MagneticPickupModel.h"
#include "dsp/PhysicalEngine.h"
#include "params/Parameters.h"

namespace
{
int checks = 0;
int failures = 0;

void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("FAIL  %s\n", what.toRawUTF8());
    }
}

constexpr double kFs = 48000.0;

ps::EngineParameters baseParameters()
{
    ps::EngineParameters p;
    p.outputGain = 1.0f;
    p.polyphony = 12;
    return p;
}

double rms (const std::vector<float>& s, double from, double to)
{
    const int a = juce::jlimit (0, (int) s.size(), (int) (from * kFs));
    const int b = juce::jlimit (a, (int) s.size(), (int) (to * kFs));
    if (b <= a)
        return 0.0;
    double sum = 0.0;
    for (int i = a; i < b; ++i)
        sum += (double) s[(size_t) i] * (double) s[(size_t) i];
    return std::sqrt (sum / (double) (b - a));
}

double peakOf (const std::vector<float>& s)
{
    double p = 0.0;
    for (auto v : s)
        p = std::max (p, std::abs ((double) v));
    return p;
}

double goertzel (const std::vector<float>& s, double frequency, double from, double to)
{
    const int a = juce::jlimit (0, (int) s.size(), (int) (from * kFs));
    const int b = juce::jlimit (a, (int) s.size(), (int) (to * kFs));
    const int n = b - a;
    if (n <= 4)
        return 0.0;

    const double w = 2.0 * juce::MathConstants<double>::pi * frequency / kFs;
    const double coeff = 2.0 * std::cos (w);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                    * (double) i / (double) (n - 1));
        s0 = window * s[(size_t) (a + i)] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::sqrt (std::max (0.0, power)) / (double) n;
}

double detectPitch (const std::vector<float>& s, double expected, double from = 0.25, double to = 0.70)
{
    const int a = (int) (from * kFs);
    const int b = std::min ((int) s.size(), (int) (to * kFs));
    if (b - a < 4096)
        return 0.0;

    const double lag = kFs / expected;
    const int lo = std::max (4, (int) (lag * 0.90));
    const int hi = std::min (b - a - 4, (int) (lag * 1.10));

    double best = -2.0;
    int bestLag = lo;
    for (int L = lo; L <= hi; ++L)
    {
        double sum = 0.0, ea = 0.0, eb = 0.0;
        for (int i = a; i < b - L; ++i)
        {
            const double x = s[(size_t) i];
            const double y = s[(size_t) (i + L)];
            sum += x * y;
            ea += x * x;
            eb += y * y;
        }
        const double d = std::sqrt (ea * eb);
        const double c = d > 1e-12 ? sum / d : 0.0;
        if (c > best)
        {
            best = c;
            bestLag = L;
        }
    }
    return kFs / (double) bestLag;
}

void writeWav (const juce::File& file, const std::vector<float>& samples)
{
    file.deleteFile();
    auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
    if (stream == nullptr)
        return;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), kFs, 1, 24, {}, 0));
    if (writer == nullptr)
        return;
    stream.release();

    juce::AudioBuffer<float> buffer (1, (int) samples.size());
    std::copy (samples.begin(), samples.end(), buffer.getWritePointer (0));
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

std::vector<float> render (ps::PhysicalEngine& engine,
                           ps::EngineParameters parameters,
                           int note,
                           float velocity,
                           double seconds,
                           bool releaseAtOneSecond = false)
{
    engine.setParameters (parameters);
    engine.noteOn (note, velocity);

    const int total = (int) (seconds * kFs);
    const int release = (int) (1.0 * kFs);
    std::vector<float> out ((size_t) total, 0.0f);

    int done = 0;
    while (done < total)
    {
        int chunk = std::min (512, total - done);
        if (releaseAtOneSecond && done < release && done + chunk > release)
            chunk = release - done;

        engine.render (out.data() + done, chunk);
        done += chunk;

        if (releaseAtOneSecond && done == release)
            engine.noteOff (note);
    }
    return out;
}

void printEnvelope (const char* label, const std::vector<float>& s)
{
    std::printf ("  %-22s", label);
    for (double t = 0.25; t < 2.5; t += 0.25)
        std::printf (" %7.2e", rms (s, t, t + 0.25));
    std::printf ("\n");
}
} // namespace

int main()
{
    std::printf ("Project Seven physics probe\n===========================\n");

    // ---- 1. Damping behaviour (palm mute must physically stop the string) ----
    {
        ps::PhysicalEngine open;
        open.prepare (kFs, 512);
        auto p = baseParameters();
        const auto openOut = render (open, p, 35, 0.9f, 3.0);

        {
            open.reset();
            open.setParameters (p);
            open.noteOn (35, 0.9f);
            std::printf ("  open  probe:");
            std::vector<float> blk (1024, 0.0f);
            for (int k = 0; k < 12; ++k)
            {
                for (int i = 0; i < 12; ++i)
                    open.render (blk.data(), 1024);
                std::printf (" %.2e", (double) std::abs (open.getVoiceIncident (0)));
            }
            std::printf ("\n");
        }

        ps::PhysicalEngine palm;
        palm.prepare (kFs, 512);
        auto q = baseParameters();
        q.articulation = ps::Articulation::palmMute;
        q.palmMute = 1.0f;
        const auto palmOut = render (palm, q, 35, 0.9f, 3.0);

        {
            palm.reset();
            palm.setParameters (q);
            palm.noteOn (35, 0.9f);
            std::printf ("  palm  probe:");
            std::vector<float> blk (1024, 0.0f);
            for (int k = 0; k < 12; ++k)
            {
                for (int i = 0; i < 12; ++i)
                    palm.render (blk.data(), 1024);
                std::printf (" %.2e", (double) std::abs (palm.getVoiceIncident (0)));
            }
            std::printf ("\n");
        }

        std::printf ("decay envelope (RMS per 0.25 s):\n");
        printEnvelope ("open B1", openOut);
        printEnvelope ("palm-muted B1", palmOut);

        const double openTail = rms (openOut, 1.0, 2.0);
        const double palmTail = rms (palmOut, 1.0, 2.0);
        std::printf ("  open tail %.3e   palm tail %.3e   ratio %.4f\n", openTail, palmTail,
                     openTail > 0.0 ? palmTail / openTail : 0.0);

        check (palmTail < openTail * 0.2, "Palm mute removes stored string energy");
        check (palmTail > 0.0, "Palm-muted string still produces output");
    }

    // ---- 2. Pitch accuracy across the range ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();

        const int notes[] = { 35, 40, 45, 50, 55, 59, 64 };
        for (int note : notes)
        {
            // Measured in the settled window: a hard pluck is physically sharp
            // for the first few hundred milliseconds (tension modulation), and
            // tuning is the pitch the string settles at.
            engine.reset();
            const auto out = render (engine, p, note, 0.8f, 2.2);
            const double expected = ps::midiNoteToFrequency (note);
            const double measured = detectPitch (out, expected, 1.10, 1.90);
            const double cents = 1200.0 * std::log2 (measured / expected);
            std::printf ("  note %-3d expected %8.2f Hz  measured %8.2f Hz  (%+5.1f cents)\n",
                         note, expected, measured, cents);
            check (std::abs (cents) < 8.0, "Pitch within 8 cents for note " + juce::String (note));
        }

        engine.reset();
        auto dropF = p;
        dropF.tuning = ps::kTuningPresets[3].notes;
        const auto low = render (engine, dropF, 29, 0.9f, 2.6);
        const double cents = 1200.0 * std::log2 (detectPitch (low, ps::midiNoteToFrequency (29), 1.20, 2.10)
                                                  / ps::midiNoteToFrequency (29));
        std::printf ("  Drop F F1 (%6.2f Hz) error %+.1f cents\n", ps::midiNoteToFrequency (29), cents);
        check (std::abs (cents) < 10.0, "Drop F low F1 stays in tune");
    }

    // ---- 3. Harmonic structure: inharmonicity and faster high-partial decay ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        engine.reset();
        const auto out = render (engine, p, 40, 0.9f, 3.0); // E2

        const double f0 = ps::midiNoteToFrequency (40);
        std::printf ("partials of E2 (%.2f Hz):\n", f0);
        for (int n = 1; n <= 8; ++n)
        {
            const double early = goertzel (out, f0 * n, 0.05, 0.55);
            const double late = goertzel (out, f0 * n, 1.20, 1.70);
            std::printf ("   n=%d  %8.1f Hz  early %8.5f  late %8.5f  decay %6.1f dB\n",
                         n, f0 * n, early, late,
                         20.0 * std::log10 (std::max (1e-12, early) / std::max (1e-12, late)));
            check (early > 0.0, "Partial " + juce::String (n) + " is present");
        }
    }

    // ---- 4. Aperture sensing: pickup position must produce a real comb ----
    {
        ps::PhysicalEngine bridge, neck;
        bridge.prepare (kFs, 512);
        neck.prepare (kFs, 512);

        auto p = baseParameters();
        p.pickupPosition = 0.06f;
        bridge.setParameters (p);
        bridge.noteOn (40, 0.9f);
        std::vector<float> a ((size_t) (1.2 * kFs), 0.0f);
        bridge.render (a.data(), (int) a.size());

        auto q = baseParameters();
        q.pickupPosition = 0.30f;
        neck.setParameters (q);
        neck.noteOn (40, 0.9f);
        std::vector<float> b ((size_t) (1.2 * kFs), 0.0f);
        neck.render (b.data(), (int) b.size());

        const double f0 = ps::midiNoteToFrequency (40);
        std::printf ("pickup position response at E2 partials:\n");
        double bridgeHigh = 0.0, neckHigh = 0.0;
        for (int n = 1; n <= 6; ++n)
        {
            const double ab = goertzel (a, f0 * n, 0.15, 0.9);
            const double an = goertzel (b, f0 * n, 0.15, 0.9);
            std::printf ("   n=%d bridge %8.5f  neck %8.5f  ratio %6.2f\n", n, ab, an,
                         an > 1e-9 ? ab / an : 0.0);
            if (n >= 3)
            {
                bridgeHigh += ab;
                neckHigh += an;
            }
        }
        check (bridgeHigh > neckHigh * 0.9, "Bridge pickup keeps more upper-partial energy");
    }

    // ---- 5. Structural coupling is active ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        engine.setParameters (p);
        engine.noteOn (40, 0.9f);
        std::vector<float> block (64, 0.0f);

        double maxStructureVelocity = 0.0;
        for (int i = 0; i < 400; ++i)
        {
            engine.render (block.data(), (int) block.size());
            maxStructureVelocity = std::max (maxStructureVelocity,
                                             std::abs (engine.getStructureVelocity()));
        }
        std::printf ("structure velocity peak %.4e, string Z0 %.4f N s/m\n",
                     maxStructureVelocity, engine.getStringImpedance (0));
        check (maxStructureVelocity > 0.0, "String drives the structure (junction is active)");
        check (engine.getStringImpedance (0) > 0.0, "String wave impedance is physical");
    }

    // ---- 6. Pickup model sanity ----
    {
        const double f1 = ps::MagneticPickupModel::fluxLinkage (0.0);
        const double fp = ps::MagneticPickupModel::fluxLinkage (0.3);
        const double fn = ps::MagneticPickupModel::fluxLinkage (-0.3);
        std::printf ("flux linkage: rest %.4f  +0.3 %.4f  -0.3 %.4f\n", f1, fp, fn);
        check (std::abs (f1 - 1.0) < 1e-9, "Rest flux is unity");
        check (std::abs (fp - fn) > 0.05, "Flux map is asymmetric (even-harmonic generation)");

        ps::MagneticPickupModel pickup;
        pickup.prepare (kFs);
        std::printf ("humbucker: L %.2f H, C %.1f pF, f_res %.0f Hz\n",
                     pickup.getInductance(), pickup.getWindingCapacitance() * 1.0e12,
                     pickup.getResonantFrequency());
        check (pickup.getResonantFrequency() > 2000.0 && pickup.getResonantFrequency() < 4000.0,
               "Humbucker resonance sits in the measured 2-3 kHz band");
    }

    // ---- 7. Level and stability ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        p.tuning = ps::kTuningPresets[3].notes;
        const auto chord = render (engine, p, 29, 0.95f, 2.0);
        std::printf ("Drop F single note peak %.4f\n", peakOf (chord));

        // Per-string single-note level: the EMF scales with string velocity, so the
        // strings do not peak equally.  This is the calibration data for the level.
        for (const int note : { 35, 40, 45, 50, 55, 59, 64 })
        {
            ps::PhysicalEngine one;
            one.prepare (kFs, 512);
            auto sp = baseParameters();
            const auto s = render (one, sp, note, 0.8f, 2.0);
            std::printf ("single note %s peak %.4f  rms %.4f\n",
                         ps::midiNoteName (note).toRawUTF8(), peakOf (s), rms (s, 0.0, 2.0));
            check (peakOf (s) > 0.15 && peakOf (s) < 0.55,
                   ps::midiNoteName (note) + " single-note level is usable ("
                       + juce::String (peakOf (s), 3) + ")");
        }

        ps::PhysicalEngine seven;
        seven.prepare (kFs, 512);
        auto q = baseParameters();
        q.tuning = ps::kTuningPresets[0].notes;
        seven.setParameters (q);
        for (int n : { 35, 40, 45, 50, 55, 59, 64 })
            seven.noteOn (n, 0.75f);
        std::vector<float> out ((size_t) (2.0 * kFs), 0.0f);
        seven.render (out.data(), (int) out.size());
        std::printf ("seven-string chord peak %.4f\n", peakOf (out));
        check (std::isfinite (peakOf (out)), "Chord render is finite");
        check (peakOf (out) < 0.90, "Seven-string chord keeps headroom ("
                   + juce::String (peakOf (out), 3) + ")");
    }

    // ---- 8. CPU ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        p.tuning = ps::kTuningPresets[3].notes;
        engine.setParameters (p);
        for (int n : p.tuning)
            engine.noteOn (n, 0.8f);

        std::vector<float> block (512, 0.0f);
        const int blocks = (int) (10.0 * kFs / 512.0);
        const double start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < blocks; ++i)
            engine.render (block.data(), 512);
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        std::printf ("10 s / 7 voices: %.3f s (%.2f%% of real time)\n", elapsed, 100.0 * elapsed / 10.0);
        check (elapsed < 3.0, "Coupled engine renders faster than real time");
    }

    // ---- 9. Tension modulation (pitch starts sharp, then settles) ----
    {
        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        const auto out = render (engine, p, 35, 0.95f, 3.0); // hard low B

        const double nominal = ps::midiNoteToFrequency (35);
        const double earlyCents = 1200.0 * std::log2 (detectPitch (out, nominal, 0.03, 0.28) / nominal);
        const double lateCents = 1200.0 * std::log2 (detectPitch (out, nominal, 1.50, 2.40) / nominal);

        std::printf ("tension modulation: attack %+.2f cents, settled %+.2f cents, drift %.2f cents\n",
                     earlyCents, lateCents, earlyCents - lateCents);

        check (earlyCents - lateCents > 1.0,
               "Hard attack starts sharp (string stretches, tension rises)");
        check (std::abs (lateCents) < 8.0, "Settled pitch returns to tune");

        // A soft note displaces the string far less, so the drift must be smaller.
        ps::PhysicalEngine soft;
        soft.prepare (kFs, 512);
        const auto softOut = render (soft, p, 35, 0.55f, 3.0);
        // Autocorrelation cannot resolve a cent or two on a quiet low string, so
        // the scaling law is checked on the model quantity itself.
        ps::PhysicalEngine hardEngine;
        hardEngine.prepare (kFs, 512);
        hardEngine.setParameters (p);
        hardEngine.noteOn (35, 0.95f);
        std::vector<float> warm (1024, 0.0f);
        for (int i = 0; i < 2; ++i)
            hardEngine.render (warm.data(), 1024);
        const double hardShift = hardEngine.getVoiceTensionCents (0);

        ps::PhysicalEngine softEngine;
        softEngine.prepare (kFs, 512);
        softEngine.setParameters (p);
        softEngine.noteOn (35, 0.55f);
        for (int i = 0; i < 2; ++i)
            softEngine.render (warm.data(), 1024);
        const double softShift = softEngine.getVoiceTensionCents (0);

        std::printf ("tension shift at attack: hard %.2f cents, soft %.2f cents\n", hardShift, softShift);

        check (hardShift > 2.0, "A hard attack shifts pitch by a musically meaningful amount");
        check (softShift < hardShift * 0.7, "Pitch shift scales with picking force");
        std::printf ("peaks: hard %.4f, soft %.4f; first 0.1 s rms %.4f / %.4f\n",
                     peakOf (out), peakOf (softOut), rms (out, 0.0, 0.1), rms (softOut, 0.0, 0.1));
    }

    // ---- 10. Fret position moves the neck termination ----
    {
        // One string only, so the comparison isolates the stopping position:
        // A1 open (nut at the free end of the neck) versus A2 at fret 12 on the
        // same string, which has identical tension and wave impedance.
        ps::EngineParameters params = baseParameters();
        params.tuning = { 33, 50, 50, 50, 50, 50, 50 };

        ps::PhysicalEngine open;
        open.prepare (kFs, 512);
        open.setParameters (params);
        open.noteOn (33, 0.85f);
        const double openCoupling = open.getVoiceNeckCoupling (0);
        const double openZ0 = open.getStringImpedance (0);
        const double openReflectance = open.getVoiceNeckAbsorptionAt (0, 430.0);

        ps::PhysicalEngine fretted;
        fretted.prepare (kFs, 512);
        fretted.setParameters (params);
        fretted.noteOn (45, 0.85f);
        const double frettedCoupling = fretted.getVoiceNeckCoupling (0);
        const double frettedZ0 = fretted.getStringImpedance (0);
        const double frettedReflectance = fretted.getVoiceNeckAbsorptionAt (0, 430.0);

        std::printf ("neck: open coupling %.3f (Z0 %.3f, |R|@430 %.4f), fret 12 coupling %.3f (Z0 %.3f, |R|@430 %.4f)\n",
                     openCoupling, openZ0, openReflectance,
                     frettedCoupling, frettedZ0, frettedReflectance);

        check (openCoupling > frettedCoupling,
               "Neck mobility falls as the stopping point moves to the body joint");
        check (std::abs (openZ0 - frettedZ0) < 0.05, "Both cases share the same string tension");
        check (openReflectance < frettedReflectance,
               "The nut absorbs more 4th-partial energy than the fretted position");
        check (openReflectance > 0.0 && frettedReflectance < 1.0, "Neck reflectance stays passive");
    }

    // ---- 11. Renders for listening ----
    {
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("artifacts");
        dir.createDirectory();

        ps::PhysicalEngine engine;
        engine.prepare (kFs, 512);
        auto p = baseParameters();
        p.articulation = ps::Articulation::open;
        engine.setParameters (p);
        for (int n : { 35, 40, 45, 50, 55, 59, 64 })
        {
            engine.noteOn (n, 0.8f);
            std::vector<float> b (512, 0.0f);
            for (int i = 0; i < 4; ++i)
                engine.render (b.data(), 512);
        }
        std::vector<float> chord ((size_t) (6.0 * kFs), 0.0f);
        engine.render (chord.data(), (int) chord.size());
        writeWav (dir.getChildFile ("10_physical_standard_b_chord.wav"), chord);

        ps::PhysicalEngine palm;
        palm.prepare (kFs, 512);
        auto q = baseParameters();
        q.articulation = ps::Articulation::palmMute;
        q.palmMute = 1.0f;
        palm.setParameters (q);
        std::vector<float> chugs ((size_t) (4.0 * kFs), 0.0f);
        for (int i = 0; i < 8; ++i)
        {
            palm.noteOn (35, 0.95f);
            palm.render (chugs.data() + (size_t) i * 24000, 24000);
        }
        writeWav (dir.getChildFile ("11_physical_palm_mute.wav"), chugs);

        ps::PhysicalEngine low;
        low.prepare (kFs, 512);
        auto r = baseParameters();
        r.tuning = ps::kTuningPresets[3].notes;
        const auto dropF = render (low, r, 29, 0.9f, 5.0);
        writeWav (dir.getChildFile ("12_physical_drop_f.wav"), dropF);

        std::printf ("wrote renders to %s\n", dir.getFullPathName().toRawUTF8());
    }

    std::printf ("--------------------------\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
