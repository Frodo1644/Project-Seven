#include "dsp/NeckPort.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace ps
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.283185307179586;

// Neck modes: measured 430 Hz (Fleischer, laser Doppler vibrometry) plus two
// ESTIMATED higher orders. Damping ratios and peak impedances are the same
// estimates used by BodyNeckModel so the two views of the neck agree.
struct NeckMode
{
    double frequency;
    double dampingRatio;
    double peakImpedance;
};

constexpr NeckMode kNeckModes[NeckPort::kModes] {
    { 430.0, 0.014, 10.0 },
    { 610.0, 0.013, 20.0 },
    { 880.0, 0.012, 24.0 }
};
} // namespace

void NeckPort::prepare (double sampleRate)
{
    fs = std::max (8000.0, sampleRate);
    const double k = 2.0 * fs;

    for (int m = 0; m < kModes; ++m)
    {
        const double w = kTwoPi * kNeckModes[m].frequency;
        const double zeta = kNeckModes[m].dampingRatio;
        const double c = kNeckModes[m].peakImpedance;

        const double mass = c / std::max (1.0e-9, 2.0 * zeta * w);
        const double a0 = k * k + 2.0 * zeta * w * k + w * w;

        b0[m] = (k / mass) / a0;
        a1[m] = (-2.0 * k * k + 2.0 * w * w) / a0;
        a2[m] = (k * k - 2.0 * zeta * w * k + w * w) / a0;
    }

    setPosition (1.0);
    reset();
}

void NeckPort::setPosition (double fretRatio)
{
    const double x = std::min (1.0, std::max (0.0, fretRatio));

    sumB0Weighted = 0.0;
    weight = 0.0;

    for (int m = 0; m < kModes; ++m)
    {
        const double shape = std::sin ((static_cast<double> (m) + 0.5) * kPi * x);
        const double w2 = shape * shape;

        b0Weighted[m] = b0[m] * w2;
        sumB0Weighted += b0Weighted[m];
        weight += w2;
    }

    weight /= static_cast<double> (kModes);
}

void NeckPort::reset()
{
    for (int m = 0; m < kModes; ++m)
    {
        state.velocityPrev1[m] = 0.0;
        state.velocityPrev2[m] = 0.0;
    }
    state.forcePrev2 = 0.0;
}

float NeckPort::reflect (float incidentWave, double stringImpedance) noexcept
{
    if (sumB0Weighted <= 0.0)
        return incidentWave;

    const double incident = static_cast<double> (incidentWave);

    double zeroInput = 0.0;
    for (int m = 0; m < kModes; ++m)
        zeroInput += -a1[m] * state.velocityPrev1[m]
                     - a2[m] * state.velocityPrev2[m];

    zeroInput -= sumB0Weighted * state.forcePrev2;

    const double force = (2.0 * incident - stringImpedance * zeroInput)
                       / (1.0 + stringImpedance * sumB0Weighted);

    double velocity = 0.0;
    for (int m = 0; m < kModes; ++m)
    {
        const double y = b0Weighted[m] * (force - state.forcePrev2)
                       - a1[m] * state.velocityPrev1[m]
                       - a2[m] * state.velocityPrev2[m];

        state.velocityPrev2[m] = state.velocityPrev1[m];
        state.velocityPrev1[m] = y;
        velocity += y;
    }

    state.forcePrev2 = force;

    return static_cast<float> (incident - stringImpedance * velocity);
}

std::complex<double> NeckPort::admittanceAt (double omega) const
{
    std::complex<double> admittance (0.0, 0.0);

    for (int m = 0; m < kModes; ++m)
    {
        if (b0Weighted[m] <= 0.0)
            continue;

        const double wm = kTwoPi * kNeckModes[m].frequency;
        const double zeta = kNeckModes[m].dampingRatio;
        const double c = kNeckModes[m].peakImpedance;
        const double mass = c / std::max (1.0e-9, 2.0 * zeta * wm);

        // Y(jw) = (jw/m) / (wm^2 - w^2 + j 2 zeta wm w)
        const std::complex<double> numerator (0.0, omega / mass);
        const std::complex<double> denominator (wm * wm - omega * omega,
                                                2.0 * zeta * wm * omega);
        const double scale = b0Weighted[m] / b0[m];

        admittance += scale * numerator / denominator;
    }

    return admittance;
}

double NeckPort::absorptionAt (double frequency, double stringImpedance) const
{
    if (frequency <= 0.0 || sumB0Weighted <= 0.0)
        return 1.0;

    const double omega = kTwoPi * frequency;
    const std::complex<double> z0 (stringImpedance, 0.0);
    const std::complex<double> admittance = admittanceAt (omega);
    const std::complex<double> reflection = (std::complex<double> (1.0, 0.0) - z0 * admittance)
                                          / (std::complex<double> (1.0, 0.0) + z0 * admittance);

    return std::abs (reflection);
}

double NeckPort::phaseDelayAt (double omega, double stringImpedance) const
{
    if (omega < 1.0e-9 || sumB0Weighted <= 0.0)
        return 0.0;

    const std::complex<double> z0 (stringImpedance, 0.0);
    const std::complex<double> admittance = admittanceAt (omega);
    const std::complex<double> reflection = (std::complex<double> (1.0, 0.0) - z0 * admittance)
                                          / (std::complex<double> (1.0, 0.0) + z0 * admittance);

    const double phase = std::arg (reflection);
    double delay = -phase / omega;

    const double period = kTwoPi / omega;
    while (delay < 0.0)
        delay += period;

    return (delay <= period * 0.5) ? delay : 0.0;
}

} // namespace ps
