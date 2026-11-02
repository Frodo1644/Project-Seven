#include "dsp/PickupModel.h"

#include <algorithm>
#include <cmath>

namespace ps
{
namespace
{
constexpr double kPi = 3.14159265358979323846;

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

void makeBandPass (double sampleRate, double frequency, double q,
                   double& b0, double& b1, double& b2, double& a0, double& a1, double& a2)
{
    const double w0 = 2.0 * kPi * frequency / sampleRate;
    const double cosW = std::cos (w0);
    const double alpha = std::sin (w0) / (2.0 * q);

    b0 = alpha;
    b1 = 0.0;
    b2 = -alpha;
    a0 = 1.0 + alpha;
    a1 = -2.0 * cosW;
    a2 = 1.0 - alpha;
}
} // namespace

void PickupModel::prepare (double sampleRate)
{
    fs = std::max (8000.0, sampleRate);
    updateCoefficients (fs);
    reset();
}

void PickupModel::reset()
{
    highPass.reset();
    resonance.reset();
    topRolloff.reset();
    bodyResonance.reset();
    bodySustain.reset();
    bodyAir.reset();
    dcBlockX = 0.0f;
    dcBlockY = 0.0f;
}

void PickupModel::updateCoefficients (double sampleRate)
{
    double b0, b1, b2, a0, a1, a2;

    // A magnetic pickup does not reproduce sub-audio DC or the very lowest
    // frequencies at full level. Keep the corner low enough for Drop F.
    makeHighPass (sampleRate, 28.0, 0.707, b0, b1, b2, a0, a1, a2);
    highPass.setCoefficients (b0, b1, b2, a0, a1, a2);

    // Coil/cable resonance. This is the characteristic electric-guitar upper-mid
    // peak; it is part of the instrument, not an EQ effect.
    const double resonanceFrequency = std::min (4300.0, sampleRate * 0.20);
    makeLowPass (sampleRate, resonanceFrequency, 1.28, b0, b1, b2, a0, a1, a2);
    resonance.setCoefficients (b0, b1, b2, a0, a1, a2);

    // Gentle top-end rolloff above the pickup's usable range.
    const double topFrequency = std::min (9500.0, sampleRate * 0.38);
    makeLowPass (sampleRate, topFrequency, 0.62, b0, b1, b2, a0, a1, a2);
    topRolloff.setCoefficients (b0, b1, b2, a0, a1, a2);

    // Guitar body resonance. The body acts as a physical resonator that colors
    // the sound. Modeled as resonant filters at body modes.
    // Fundamental body mode: 60-120 Hz (depends on guitar size/shape)
    // Using a bandpass at 80 Hz with moderate Q to simulate body fundamental
    makeBandPass (sampleRate, 80.0, 1.8, b0, b1, b2, a0, a1, a2);
    bodyResonance.setCoefficients (b0, b1, b2, a0, a1, a2);

    // Sustain nodes: 200-400 Hz (hollow body response)
    // Using a bandpass at 300 Hz with higher Q for sustain enhancement
    makeBandPass (sampleRate, 300.0, 2.2, b0, b1, b2, a0, a1, a2);
    bodySustain.setCoefficients (b0, b1, b2, a0, a1, a2);

    // Air resonance: 800-1200 Hz (body cavity)
    // Using a bandpass at 1000 Hz with moderate Q
    makeBandPass (sampleRate, 1000.0, 1.5, b0, b1, b2, a0, a1, a2);
    bodyAir.setCoefficients (b0, b1, b2, a0, a1, a2);

    outputTrim = 0.85f;  // compensate for added body resonance gain
}

float PickupModel::processSample (float input) noexcept
{
    // Apply pickup electrical response
    float x = highPass.process (input);
    x = resonance.process (x);
    x = topRolloff.process (x);

    // Apply guitar body resonance (the body colors the sound)
    x = bodyResonance.process (x);
    x = bodySustain.process (x);
    x = bodyAir.process (x);

    // Remove any residual DC without affecting audio-band content.
    const float y = x - dcBlockX + 0.9995f * dcBlockY;
    dcBlockX = x;
    dcBlockY = y;

    return y * outputTrim;
}
} // namespace ps
