#pragma once

#include <array>

#include "params/Parameters.h"

namespace ps
{
struct StringAssignment
{
    int stringIndex = -1;
    int fret = 0;
    bool harmonic = false;
    int harmonicNumber = 0;
};

// Deterministic MIDI-note to string/fret resolution and harmonic lookup.
class TuningManager
{
public:
    static StringAssignment assignNote (int midiNote,
                                        const std::array<int, kNumStrings>& tuning,
                                        const std::array<bool, kNumStrings>& busyStrings,
                                        int maxFret = 30);

    // Finds a string and harmonic number whose nth partial matches the requested
    // sounding pitch. Returns false when the note cannot be a natural/pinch
    // harmonic on any string.
    static bool findHarmonic (int midiNote,
                              const std::array<int, kNumStrings>& tuning,
                              const std::array<bool, kNumStrings>& busyStrings,
                              bool pinch,
                              StringAssignment& result);

    static double frequencyForNote (int midiNote) noexcept;
};
} // namespace ps
