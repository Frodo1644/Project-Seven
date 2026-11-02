#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "dsp/PickupModel.h"
#include "dsp/StringModel.h"
#include "params/Parameters.h"
#include "params/TuningManager.h"

namespace ps
{
// Owns the voice pool, string assignment, articulation state and shared pickup
// response. All buffers are allocated in prepare(); render() and MIDI handling
// never allocate.
class SevenAudioEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParameters (const EngineParameters& newParameters);
    const EngineParameters& getParameters() const noexcept { return parameters; }

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff (bool immediate = false);

    void setPitchBend (float normalizedBend); // -1 .. +1
    void setSustainPedal (bool down);

    void render (float* output, int numSamples);

    int getActiveVoiceCount() const noexcept;
    int getLastAssignedString() const noexcept { return lastAssignedString.load (std::memory_order_relaxed); }
    int getLastAssignedFret() const noexcept { return lastAssignedFret.load (std::memory_order_relaxed); }
    int getLastHarmonic() const noexcept { return lastHarmonic.load (std::memory_order_relaxed); }
    double getSampleRate() const noexcept { return fs; }

private:
    struct Voice
    {
        StringModel string;
        bool active = false;
        bool dying = false;
        bool sustained = false;
        int note = -1;
        int stringIndex = -1;
        int fret = 0;
        int harmonicNumber = 0;
        float velocity = 0.0f;
        uint64_t age = 0;
    };

    static constexpr int kVoicePoolSize = kMaxPolyphony + 12;

    void startVoice (Voice& voice,
                     int midiNote,
                     float velocity,
                     const StringAssignment& assignment,
                     Articulation articulation);
    void releaseVoice (Voice& voice, float seconds) noexcept;
    void killVoice (Voice& voice, float seconds) noexcept;
    int findFreeSlot() noexcept;
    int findOldestActiveSlot() const noexcept;
    float baseInharmonicityForString (int stringIndex) const noexcept;
    float articulationMute (Articulation articulation) const noexcept;

    double fs = 48000.0;
    std::vector<Voice> voices;
    PickupModel pickup;
    EngineParameters parameters;
    float bendNormalized = 0.0f;
    bool sustainDown = false;
    uint64_t noteCounter = 0;
    std::atomic<int> lastAssignedString { -1 };
    std::atomic<int> lastAssignedFret { 0 };
    std::atomic<int> lastHarmonic { 0 };
};
} // namespace ps
