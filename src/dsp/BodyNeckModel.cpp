#include "dsp/BodyNeckModel.h"

#include <algorithm>
#include <cmath>

namespace ps
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;
}

// Mode table.
//
// Modes 1-5 are MEASURED: full-instrument modal analysis of a hard-maple
// solid-body electric guitar (free suspension, roving hammer, 148 points,
// Penn State / Russell): 55.3, 160.1, 189.4, 300.5, 369.7 Hz.
// Mode 6 is MEASURED: out-of-plane neck/fingerboard resonance at 430 Hz
// (Fleischer 1999, laser Doppler vibrometry, instrument in playing position).
// Modes 7-9 are ESTIMATED: higher neck orders and the bridge/saddle assembly,
// from the general trend of published neck measurements.
//
// Damping ratios are ESTIMATED: the modal study does not publish Q values, and
// solid-body electrics are reported as lightly damped. zeta = 0.012-0.030 sits
// in the published 0.01-0.04 band, higher for the bridge assembly.
//
// Peak impedances are ESTIMATED from the target reflectance at resonance,
// |R| = (c - Z0)/(c + Z0) with design reference Z0 = 0.45 N s/m:
//   |R| 0.90 -> 8.6   0.92 -> 10.8   0.95 -> 17.6   0.97 -> 29.6
// Narrow absorption bands are exactly the mechanism behind dead spots and
// per-partial decay differences; a broadband curve would be arbitrary EQ.
const std::array<BodyNeckModel::Mode, BodyNeckModel::kNumModes>& BodyNeckModel::modeTable() noexcept
{
    static const std::array<Mode, kNumModes> modes { {
        {  55.3, 0.022,  9.0 },  // body bending (measured)
        { 160.1, 0.020, 12.0 },  // body 2nd bending (measured)
        { 189.4, 0.018, 10.5 },  // body bending + neck twist (measured)
        { 300.5, 0.016, 15.0 },  // body bending + neck twist (measured)
        { 369.7, 0.015, 16.5 },  // body torsion (measured)
        { 430.0, 0.014, 10.0 },  // neck out-of-plane (measured)
        { 610.0, 0.013, 20.0 },  // neck 2nd order (estimated)
        { 880.0, 0.012, 24.0 },  // neck 3rd order (estimated)
        {1200.0, 0.030, 28.0 }   // bridge / saddle assembly (estimated)
    } };
    return modes;
}

void BodyNeckModel::prepare (double sampleRate) noexcept
{
    fs = std::max (8000.0, sampleRate);

    const double k = 2.0 * fs; // bilinear factor
    sumB0 = 0.0;

    const auto& modes = modeTable();
    for (int i = 0; i < kNumModes; ++i)
    {
        const double w = kTwoPi * modes[static_cast<size_t> (i)].frequency;
        const double zeta = modes[static_cast<size_t> (i)].dampingRatio;
        const double c = modes[static_cast<size_t> (i)].peakImpedance;

        // m = c / (2 zeta w) gives the requested peak admittance 1/c.
        const double m = c / std::max (1.0e-9, 2.0 * zeta * w);

        const double a0 = k * k + 2.0 * zeta * w * k + w * w;

        b0[i] = (k / m) / a0;
        a1[i] = (-2.0 * k * k + 2.0 * w * w) / a0;
        a2[i] = (k * k - 2.0 * zeta * w * k + w * w) / a0;

        sumB0 += b0[i];
    }

    reset();
}

void BodyNeckModel::reset() noexcept {}

void BodyNeckModel::resetJunction (Junction& junction) const noexcept
{
    for (int i = 0; i < kNumModes; ++i)
    {
        junction.velocityPrev1[i] = 0.0;
        junction.velocityPrev2[i] = 0.0;
    }
    junction.forcePrev1 = 0.0;
    junction.forcePrev2 = 0.0;
    junction.velocity = 0.0;
    junction.force = 0.0;
}

double BodyNeckModel::zeroInputResponse (const Junction& junction) const noexcept
{
    double sum = 0.0;
    for (int i = 0; i < kNumModes; ++i)
        sum += -a1[i] * junction.velocityPrev1[i]
               - a2[i] * junction.velocityPrev2[i];

    // The numerator carries a z^-2 term (b2 = -b0).
    sum -= sumB0 * junction.forcePrev2;
    return sum;
}

void BodyNeckModel::applyForce (Junction& junction, double force) const noexcept
{
    double velocity = 0.0;

    for (int i = 0; i < kNumModes; ++i)
    {
        const double y = b0[i] * (force - junction.forcePrev2)
                       - a1[i] * junction.velocityPrev1[i]
                       - a2[i] * junction.velocityPrev2[i];

        junction.velocityPrev2[i] = junction.velocityPrev1[i];
        junction.velocityPrev1[i] = y;
        velocity += y;
    }

    junction.forcePrev2 = junction.forcePrev1;
    junction.forcePrev1 = force;
    junction.velocity = velocity;
    junction.force = force;
}

} // namespace ps
