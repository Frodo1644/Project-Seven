#pragma once

#include <cstdint>
#include <vector>

#include "dsp/GuitarSpec.h"
#include "params/Parameters.h"

namespace ps
{
// One steel electric-guitar string as a stiff digital waveguide.
//
// - Loop delay is set from the *physical* pitch (T = 4 L^2 f^2 mu), not a
//   lookup of previous output.
// - Inharmonicity comes from the string's own stiffness,
//   B = (pi^3 E d^4)/(64 T L^2), realised with an all-pass dispersion chain and
//   compensated by phase-delay measurement so the pitch stays exact.
// - Loss is split into the string's internal damping (frequency-dependent loop
//   filter) and the structural loss, which arrives as the reflected wave from
//   BodyNeckModel. The string itself never fades an envelope.
// - The termination is supplied by the engine, so the string exposes the
//   incident wave and accepts the reflected wave (two-phase rendering) instead
//   of reflecting off a rigid end.
// - Excitation is a pick/string collision: a triangular displacement imposed
//   over the pick's contact width, released into the waveguide.  Contact width,
//   depth and edge sharpness all follow the pick and the velocity.
class PhysicalString
{
public:
    struct Setup
    {
        int stringIndex = 0;
        int openMidiNote = 35;
        int midiNote = 35;
        float velocity = 0.8f;
        float pickPosition = 0.11f;   // fraction of scale length from the bridge
        float pickupPosition = 0.12f; // fraction of scale length from the bridge
        float pickIntensity = 0.7f;
        float palmMute = 0.0f;
        float bentSemitones = 0.0f;
        int harmonicNumber = 0;       // 0 = fundamental excitation
        bool pinch = false;
        float pickThicknessMm = 0.9f;
        double apertureWidthMetres = 0.0635; // humbucker pole footprint (measured, 2.5 in)
        uint32_t seed = 1u;
    };

    void prepare (double sampleRate, int maxLoopSamples);
    void reset();

    void start (const Setup& setup, const phys::StringDesign& design, const phys::StringState& state);

    // Phase one of the coupled render: returns the wave arriving at the bridge.
    float pullIncident() noexcept;

    // Phase two: writes the wave reflected by the loaded structure back into
    // the string and advances the loop by one sample.
    void pushReflected (float reflected) noexcept;

    // Aperture-averaged string displacement at the pickup, in millimetres.
    float senseAperture() noexcept;

    double getImpedance() const noexcept { return waveImpedance; }
    bool isActive() const noexcept { return active; }
    bool isDying() const noexcept { return dying; }
    float getAmplitude() const noexcept { return lastAmplitude; }

    void setBentSemitones (float semitones) noexcept;
    void setPalmMuteTarget (float amount) noexcept;
    void noteOff (float t60Seconds) noexcept;
    void kill (float fadeSeconds) noexcept;

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
    std::vector<float> shapeScratch;

    bool active = false;
    bool dying = false;
    uint32_t rng = 1u;

    double waveImpedance = 0.4;   // Z0 = sqrt(T mu)
    double baseTotalDelay = 585.0;
    double filterPhaseDelay = 0.0;
    double smoothDelay = 585.0;
    double delaySmoothCoeff = 0.002;
    double vibratingLength = phys::kScaleLengthMetres;

    float bendTargetSemitones = 0.0f;
    float bendRatio = 1.0f;
    float bendRatioCurrent = 1.0f;
    float bendSmoothCoeff = 0.0014f;

    // Loop loss: internal damping only. Structural loss is in the junction.
    float loopLossInternal = 0.99999f;
    float loopLossDamped = 0.9990f;
    float dampCurrent = 0.0f;
    float dampTarget = 0.0f;
    float dampSmoothCoeff = 0.0006f;

    float lp1CoeffBase = 0.5f;
    float lp1CoeffDamped = 0.9f;
    float lp2Coeff = 0.2f;
    float lp1State = 0.0f;
    float lp2State = 0.0f;

    static constexpr int kDispersionSections = 4;
    float dispersionCoeff = 0.0f;
    float dispersionState[kDispersionSections] {};

    float apertureFractions[5] {};
    int apertureTaps = 5;

    float fadeGain = 1.0f;
    float fadeCoeff = 1.0f;
    float lastAmplitude = 0.0f;
    float incidentOut = 0.0f;

    // Tension modulation. Stretching a string raises its tension, so the pitch
    // starts sharp after a hard pluck and settles as the note decays.
    //   strain = A^2 pi^2 / (4 L^2),  dT = E A_core strain,  df/f = dT / (2T)
    //   =>  df/f = beta A^2,   beta = pi^2 E A_core / (8 L^2 T)
    double tensionBeta = 0.0;        // 1/m^2
    float pluckDepthMetres = 0.0f;   // physical peak displacement at note start
    float senseEnvelope = 0.0f;      // smoothed |sensed displacement|, model units
    float senseReference = 0.0f;     // its peak, used to track the decay
    float tensionSmoothCoeff = 0.002f;
};
} // namespace ps
