#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include "dsp/BodyNeckModel.h"
#include "dsp/MagneticPickupModel.h"
#include "dsp/PhysicalString.h"
#include "params/Parameters.h"
#include "params/TuningManager.h"

namespace ps
{
// Coupled physical engine.
//
// One shared structure (BodyNeckModel) is loaded by all voices at every
// sample: each string contributes the wave arriving at the bridge, the junction
// solves the common termination velocity, and every string receives the
// reflected wave.  Energy therefore flows string -> structure -> string, which
// is what produces decay differences, dead-spot behaviour and mechanically
// mediated inter-string interaction.
//
// The pickup senses actual string displacement over its pole footprint,
// converts flux linkage to EMF, and passes the summed EMF through the passive
// humbucker's electrical network.  No EQ or body-resonance filter is applied to
// the output: a magnetic pickup does not hear the body.
class PhysicalEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParameters (const EngineParameters& newParameters);
    const EngineParameters& getParameters() const noexcept { return parameters; }

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff (bool immediate = false);

    void setPitchBend (float normalizedBend);
    void setSustainPedal (bool down);

    void render (float* output, int numSamples);

    int getActiveVoiceCount() const noexcept;
    int getLastAssignedString() const noexcept { return lastAssignedString.load (std::memory_order_relaxed); }
    int getLastAssignedFret() const noexcept { return lastAssignedFret.load (std::memory_order_relaxed); }
    int getLastHarmonic() const noexcept { return lastHarmonic.load (std::memory_order_relaxed); }

    // Diagnostics for the validation tests.
    double getStructureVelocity() const noexcept { return junction.velocity; }
    double getStringImpedance (int voiceSlot) const noexcept;
    float getVoiceIncident (int voiceSlot) const noexcept;

private:
    struct Voice
    {
        PhysicalString string;
        bool active = false;
        bool dying = false;
        bool sustained = false;
        int note = -1;
        int stringIndex = -1;
        int fret = 0;
        int harmonicNumber = 0;
        float velocity = 0.0f;
        float incident = 0.0f;
        double fluxPrev = 1.0;
        uint64_t age = 0;
    };

    static constexpr int kVoicePoolSize = kMaxPolyphony + 12;

    void startVoice (Voice& voice, int midiNote, float velocity,
                     const StringAssignment& assignment, Articulation articulation);
    void releaseVoice (Voice& voice, float t60) noexcept;
    void killVoice (Voice& voice, float seconds) noexcept;
    int findFreeSlot() noexcept;
    int findOldestActiveSlot() const noexcept;
    float articulationDamping (Articulation articulation) const noexcept;

    double fs = 48000.0;
    std::vector<Voice> voices;
    BodyNeckModel structure;
    BodyNeckModel::Junction junction;
    MagneticPickupModel pickup;
    std::array<phys::StringDesign, kNumStrings> stringSet = phys::defaultStringSet();
    EngineParameters parameters;
    float bendNormalized = 0.0f;
    bool sustainDown = false;
    uint64_t noteCounter = 0;
    double emfScale = 1.0;
    std::atomic<int> lastAssignedString { -1 };
    std::atomic<int> lastAssignedFret { 0 };
    std::atomic<int> lastHarmonic { 0 };
};
} // namespace ps
