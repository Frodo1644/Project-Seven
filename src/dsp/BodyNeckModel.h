#pragma once

#include <array>

#include "params/Parameters.h"

namespace ps
{
// Reduced-order mechanical model of the solid-body guitar structure.
//
// The structure is a parallel bank of modes expressed as mechanical
// *admittances*, so it can be loaded by every string at once:
//
//   v = Y(s) F,     Y = sum_i (s / m_i) / (s^2 + 2 zeta_i w_i s + w_i^2)
//
// F is the total force the strings inject, v the common velocity of the
// termination.  Each string's reflected wave follows from the shared junction
// velocity, so energy a string sends into the structure is removed from that
// string and the structure's motion is returned to all strings: this is the
// mechanically mediated coupling, not a mix of independent outputs.
//
// Discretisation is bilinear, which preserves positive-realness, so the coupled
// system is passive and cannot generate energy.
//
// Approximation (documented in docs/PHYSICAL_COUPLING_SPECIFICATION.md): all
// strings are assumed to drive one effective point on the structure.  A
// position-dependent mobility matrix is the next refinement.
class BodyNeckModel
{
public:
    static constexpr int kNumModes = 9;

    struct Mode
    {
        double frequency;     // Hz
        double dampingRatio;  // zeta
        double peakImpedance; // N s / m at resonance
    };

    // State of the shared structure.
    struct Junction
    {
        double velocityPrev1[kNumModes] {};
        double velocityPrev2[kNumModes] {};
        double forcePrev1 = 0.0;
        double forcePrev2 = 0.0;
        double velocity = 0.0;
        double force = 0.0;
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void resetJunction (Junction& junction) const noexcept;

    // Sum of the direct (instantaneous) admittance terms.
    double directTerm() const noexcept { return sumB0; }

    // Response of the modes to their own past state with no new force.
    double zeroInputResponse (const Junction& junction) const noexcept;

    // Applies `force` to the structure and updates the mode states.
    void applyForce (Junction& junction, double force) const noexcept;

    static const std::array<Mode, kNumModes>& modeTable() noexcept;

private:
    double fs = 48000.0;
    double b0[kNumModes] {};
    double a1[kNumModes] {};
    double a2[kNumModes] {};
    double sumB0 = 0.0;
};
} // namespace ps
