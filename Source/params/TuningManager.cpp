#include "params/TuningManager.h"

#include <cmath>

namespace ps
{
namespace
{
double centsBetween (double a, double b) noexcept
{
    return std::abs (1200.0 * std::log2 (a / b));
}
} // namespace

double TuningManager::frequencyForNote (int midiNote) noexcept
{
    return midiNoteToFrequency (midiNote);
}

StringAssignment TuningManager::assignNote (int midiNote,
                                            const std::array<int, kNumStrings>& tuning,
                                            const std::array<bool, kNumStrings>& busyStrings,
                                            int maxFret)
{
    StringAssignment best;
    double bestScore = 1.0e30;

    // Primary pass: playable fret positions, prefer open strings and low frets,
    // then lower strings on ties. Busy strings are strongly discouraged so
    // chords spread across strings like a real guitar.
    for (int s = 0; s < kNumStrings; ++s)
    {
        const int fret = midiNote - tuning[static_cast<size_t> (s)];
        if (fret < 0 || fret > maxFret)
            continue;

        double score = static_cast<double> (fret) * 4.0 + static_cast<double> (s) * 0.05;
        if (busyStrings[static_cast<size_t> (s)])
            score += 1000.0;

        if (score < bestScore)
        {
            bestScore = score;
            best.stringIndex = s;
            best.fret = fret;
            best.harmonic = false;
            best.harmonicNumber = 0;
        }
    }

    if (best.stringIndex >= 0)
        return best;

    // Out-of-range note: keep it playable on the nearest string rather than
    // silently dropping it. Pitch is still generated from the MIDI note.
    for (int s = 0; s < kNumStrings; ++s)
    {
        const int fret = midiNote - tuning[static_cast<size_t> (s)];
        double score = static_cast<double> (std::abs (fret)) * 4.0 + static_cast<double> (s) * 0.05;
        if (busyStrings[static_cast<size_t> (s)])
            score += 1000.0;

        if (score < bestScore)
        {
            bestScore = score;
            best.stringIndex = s;
            best.fret = juce::jlimit (0, 36, fret);
            best.harmonic = false;
            best.harmonicNumber = 0;
        }
    }

    return best;
}

bool TuningManager::findHarmonic (int midiNote,
                                  const std::array<int, kNumStrings>& tuning,
                                  const std::array<bool, kNumStrings>& busyStrings,
                                  bool pinch,
                                  StringAssignment& result)
{
    const double target = frequencyForNote (midiNote);
    const int minHarmonic = pinch ? 3 : 2;
    const int maxHarmonic = pinch ? 9 : 12;

    double bestScore = 1.0e30;
    StringAssignment best;

    for (int s = 0; s < kNumStrings; ++s)
    {
        const double open = frequencyForNote (tuning[static_cast<size_t> (s)]);
        if (open <= 0.0)
            continue;

        const double ratio = target / open;
        const int n = static_cast<int> (std::llround (ratio));
        if (n < minHarmonic || n > maxHarmonic)
            continue;

        const double cents = centsBetween (target, open * static_cast<double> (n));
        if (cents > 45.0)
            continue;

        double score = cents + static_cast<double> (n) * 0.2;
        if (busyStrings[static_cast<size_t> (s)])
            score += 50.0;

        if (score < bestScore)
        {
            bestScore = score;
            best.stringIndex = s;
            best.fret = 0;
            best.harmonic = true;
            best.harmonicNumber = n;
        }
    }

    if (best.stringIndex < 0)
        return false;

    result = best;
    return true;
}

} // namespace ps
