#include "dsp/MagneticPickupModel.h"

#include <algorithm>
#include <cmath>

namespace ps
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

void makeLowPass (double sampleRate, double frequency, double q,
                  double& b0, double& b1, double& b2, double& a0, double& a1, double& a2)
{
    const double w0 = 2.0 * kPi * frequency / sampleRate;
    const double cosW = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    b0 = (1.0 - cosW) * 0.5;
    b1 = 1.0 - cosW;
    b2 = (1.0 - cosW) * 0.5;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosW;
    a2 = 1.0 - alpha;
}

void makeHighPass (double sampleRate, double frequency, double q,
                   double& b0, double& b1, double& b2, double& a0, double& a1, double& a2)
{
    const double w0 = 2.0 * kPi * frequency / sampleRate;
    const double cosW = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    b0 = (1.0 + cosW) * 0.5;
    b1 = -(1.0 + cosW);
    b2 = (1.0 + cosW) * 0.5;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosW;
    a2 = 1.0 - alpha;
}
} // namespace

double MagneticPickupModel::fluxLinkage (double displacement) noexcept
{
    // Normalised pole gap.  A string displacement of 1.0 corresponds to the
    // pole-to-string gap, so the field can never be pushed past the pole face.
    const double y = std::clamp (displacement, -0.6, 0.6);
    return 1.0 + y + y * y;
}

void MagneticPickupModel::prepare (double sampleRate)
{
    fs = std::max (8000.0, sampleRate);

    // Load the coil: 500 k volume pot in parallel with the amp input, plus the
    // winding/cable capacitance.  The resonant frequency follows from
    // f = 1 / (2 pi sqrt(L C)); C is solved from the measured in-guitar peak so
    // the model reproduces the measured part rather than a guess.
    resonanceFrequency = std::min (2760.0, fs * 0.22);
    totalCapacitance = 1.0 / (4.0 * kPi * kPi * resonanceFrequency * resonanceFrequency * inductance);

    updateCoefficients (fs);
    reset();
}

void MagneticPickupModel::reset()
{
    coilResonance.reset();
    eddyRolloff.reset();
    dcBlock.reset();
}

void MagneticPickupModel::updateCoefficients (double sampleRate)
{
    double b0, b1, b2, a0, a1, a2;

    makeLowPass (sampleRate, resonanceFrequency, qualityFactor, b0, b1, b2, a0, a1, a2);
    coilResonance.set (b0, b1, b2, a0, a1, a2);

    // Eddy-current pole: steepens the top-end to the ~18 dB/oct slope measured
    // on real humbuckers and damps the peak.
    const double eddyFrequency = std::min (6500.0, sampleRate * 0.42);
    makeLowPass (sampleRate, eddyFrequency, 0.707, b0, b1, b2, a0, a1, a2);
    eddyRolloff.set (b0, b1, b2, a0, a1, a2);

    // DC / subsonic block only.  A passive magnetic pickup has no LF roll-off
    // in the audio band, so this sits below any playable fundamental.
    makeHighPass (sampleRate, 5.0, 0.707, b0, b1, b2, a0, a1, a2);
    dcBlock.set (b0, b1, b2, a0, a1, a2);

    // Level calibration: a fixed instrument trim (not tonal shaping) so a
    // hard-picked note peaks near -16 dBFS and a seven-string chord keeps
    // headroom before the user's output control. Measured in the physics probe.
    outputTrim = 0.55f;
}

float MagneticPickupModel::processSample (float emf) noexcept
{
    float x = coilResonance.process (emf);
    x = eddyRolloff.process (x);
    x = dcBlock.process (x);
    return x * outputTrim;
}

} // namespace ps
