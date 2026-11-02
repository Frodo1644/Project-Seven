#pragma once

#include <complex>

#include "params/Parameters.h"

namespace ps
{
// The stopped end of a string is not rigid.  The neck moves, and that motion
// absorbs string energy — Fleischer's measured dead-spot mechanism: the loss at
// the fret (or nut) depends on how far the stopping point sits from a node of
// the neck's modes, and it removes energy from specific partials rather than
// from the note as a whole.
//
// Reduced-order model, consistent with the structure's own neck modes:
//
//   phi_m(x) = sin((m - 1/2) pi x / L)          clamped-free neck shape
//   Y_m(s)   = phi_m^2 (s/m_m) / (s^2 + 2 zeta_m w_m s + w_m^2)
//
// with x measured from the body joint, so x/L = 2^(-fret/12): the nut (fret 0)
// is the free end with maximum mobility and the body joint is stiff.
//
// Scaling a positive-real admittance by a non-negative weight keeps it
// positive-real, so the reflectance (1 - Z0 Y)/(1 + Z0 Y) still satisfies
// |R| <= 1 and the termination can only absorb energy.
//
// The reflectance is applied inside the string loop, so |R| at each partial
// sets that partial's decay.  Its phase is compensated at the fundamental, so
// tuning is unaffected.
class NeckPort
{
public:
    static constexpr int kModes = 3;

    void prepare (double sampleRate);
    void reset();

    // fretRatio = 2^(-fret/12), i.e. the vibrating length as a fraction of the
    // open string, which equals the stopping point's position along the neck.
    void setPosition (double fretRatio);

    float reflect (float incident, double stringImpedance) noexcept;

    // Extra phase delay introduced at this frequency, used for tuning
    // compensation when the note is started.
    double phaseDelayAt (double omega, double stringImpedance) const;

    // Magnitude of the force-wave reflectance at a frequency. Below 1 by
    // construction (passive), and lower where the neck absorbs more energy.
    double absorptionAt (double frequency, double stringImpedance) const;

    // Total mode-shape weight, for diagnostics and tests.
    double couplingWeight() const noexcept { return weight; }

private:
    struct State
    {
        double velocityPrev1[kModes] {};
        double velocityPrev2[kModes] {};
        double forcePrev2 = 0.0;
    };

    std::complex<double> admittanceAt (double omega) const;

    double fs = 48000.0;
    double b0[kModes] {};
    double a1[kModes] {};
    double a2[kModes] {};
    double b0Weighted[kModes] {};
    double sumB0Weighted = 0.0;
    double weight = 0.0;
    State state;
};
} // namespace ps
