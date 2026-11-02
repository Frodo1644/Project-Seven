#pragma once

#include <cstdint>
#include <vector>

#include "params/Parameters.h"

namespace ps
{
// A single physically modelled steel string with realistic sustain.
//
// The model simulates a real electric guitar string:
// - Energy stays in the string for a long time (steel strings ring for seconds)
// - The magnetic field keeps picking up vibrations as long as the string moves
// - Only friction at the bridge/ nut and air resistance slowly dissipate energy
// - Palm muting increases damping near the bridge, shortening effective length
//
// This gives the characteristic "infinite sustain" feel of an electric guitar
// where notes can ring for many seconds, especially on low strings.

class StringModel
{
public:
    struct NoteSetup
    {
        int stringIndex = 0;
        int openMidiNote = 35;
        int midiNote = 35;
        float velocity = 0.8f;
        float pickPosition = 0.11f;   // fraction of scale length from bridge
        float pickupPosition = 0.12f; // fraction of scale length from bridge
        float pickIntensity = 0.7f;
        float palmMute = 0.0f;
        float baseInharmonicity = 4.0e-5f;
        ps::Articulation articulation = ps::Articulation::open;
        int harmonicNumber = 0;
        float bendSemitones = 0.0f;
        uint32_t seed = 0x12345678u;
        // Realistic sustain: strings ring until energy is truly gone
        // (magnetic field keeps changing = string keeps vibrating)
        float sustainLevel = 1.0f;  // 1.0 = full sustain, 0.0 = muted
    };

    StringModel() = default;
    StringModel (StringModel&&) noexcept = default;
    StringModel& operator= (StringModel&&) noexcept = default;
    StringModel (const StringModel&) = delete;
    StringModel& operator= (const StringModel&) = delete;

    void prepare (double sampleRate, int maxLoopSamples);
    void reset();

    void start (const NoteSetup& setup);
    void setBendSemitones (float semitones) noexcept;
    void setPalmMuteTarget (float amount) noexcept;
    void noteOff (float releaseSeconds) noexcept;
    void kill (float fadeSeconds) noexcept;

    bool isActive() const noexcept { return active; }
    bool isDying() const noexcept { return dying; }
    bool isReleasing() const noexcept { return releasing; }
    float getCurrentAmplitude() const noexcept { return lastAmplitude; }

    float processSample() noexcept;

private:
    float readFractional (double delay) const noexcept;
    float nextNoise() noexcept;
    static double onePolePhaseDelay (double coefficient, double omega) noexcept;
    static double allpassPhaseDelay (double magnitude, double omega) noexcept;
    double findDispersionCoefficient (double loopSamples, double inharmonicity) const noexcept;

    double fs = 48000.0;
    int bufferSize = 0;
    int bufferMask = 0;
    int maxLoop = 0;
    int writePos = 0;
    std::vector<float> delayLine;
    std::vector<float> excitationScratch;

    bool active = false;
    bool dying = false;
    bool releasing = false;
    uint32_t rng = 0x12345678u;

    double baseFrequency = 82.41;
    double baseTotalDelay = 585.0;
    double filterPhaseDelay = 0.0;
    double delaySamples = 585.0;
    double smoothDelay = 585.0;

    float bendTargetSemitones = 0.0f;
    float bendCurrentSemitones = 0.0f;
    float bendRatio = 1.0f;
    float bendRatioCurrent = 1.0f;

    // Loop filters - much gentler for realistic sustain
    float lp1CoeffBase = 0.5f;
    float lp1CoeffMute = 0.9f;
    float lp2CoeffBase = 0.05f;
    float lp2CoeffMute = 0.8f;
    float lp1State = 0.0f;
    float lp2State = 0.0f;
    float loopGainBase = 0.999f;
    float loopGainMute = 0.99f;
    float loopGainSustain = 0.9999f;
    float muteCurrent = 0.0f;
    float muteTarget = 0.0f;
    float muteSmoothCoeff = 0.0005f;
    float bendSmoothCoeff = 0.0014f;
    float delaySmoothCoeff = 0.0026f;

    static constexpr int kDispersionSections = 4;
    float dispersionCoeff = 0.0f;
    float dispersionState[kDispersionSections] {};

    float pickupTapFraction = 0.12f;
    float pickupDepth = 0.38f;

    float outputTrim = 0.55f;
    float releaseGain = 1.0f;
    float releaseCoeff = 1.0f;
    float fadeGain = 1.0f;
    float fadeCoeff = 1.0f;
    float lastAmplitude = 0.0f;

    // Short pick transient (supports the attack; the note itself comes from the
    // waveguide).
    int transientRemaining = 0;
    int transientLength = 1;
    float transientLevel = 0.0f;
    float transientNoisePrev = 0.0f;

    // Physical sustain model - simulates the string's natural ringing.
    // The key insight: magnetic pickups sense string movement, and steel strings
    // can vibrate for many seconds. The loop gain keeps the string alive much
    // longer than a typical Karplus-Strong implementation.
    float sustainGain = 1.0f;      // keeps string vibrating after note-off
    float sustainCoeff = 1.0f;     // how fast sustain decays (very slow)
    bool sustainMode = false;      // whether to use long sustain
};
} // namespace ps
