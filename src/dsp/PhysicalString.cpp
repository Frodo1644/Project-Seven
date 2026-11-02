#include "dsp/PhysicalString.h"

#include <algorithm>
#include <cmath>

namespace ps
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;

double clampd (double value, double low, double high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}
} // namespace

void PhysicalString::prepare (double sampleRate, int maxLoopSamples)
{
    fs = std::max (8000.0, sampleRate);
    maxLoop = std::max (64, maxLoopSamples);

    int size = 1;
    while (size < maxLoop * 2 + 16)
        size <<= 1;

    bufferSize = size;
    bufferMask = size - 1;
    delayLine.assign (static_cast<size_t> (bufferSize), 0.0f);
    shapeScratch.assign (static_cast<size_t> (bufferSize / 2), 0.0f);
    reset();
}

void PhysicalString::reset()
{
    active = false;
    dying = false;
    writePos = 0;
    lp1State = 0.0f;
    lp2State = 0.0f;
    for (auto& state : dispersionState)
        state = 0.0f;
    fadeGain = 1.0f;
    lastAmplitude = 0.0f;
    incidentOut = 0.0f;
    dampCurrent = 0.0f;
    std::fill (delayLine.begin(), delayLine.end(), 0.0f);
}

void PhysicalString::start (const Setup& setup,
                            const phys::StringDesign& design,
                            const phys::StringState& state)
{
    if (bufferSize == 0)
        return;

    std::fill (delayLine.begin(), delayLine.end(), 0.0f);

    active = true;
    dying = false;
    fadeGain = 1.0f;
    lp1State = 0.0f;
    lp2State = 0.0f;
    for (auto& s : dispersionState)
        s = 0.0f;

    rng = setup.seed | 1u;

    const float velocity = juce::jlimit (0.02f, 1.0f, setup.velocity);
    const float intensity = juce::jlimit (0.0f, 1.0f, setup.pickIntensity);

    vibratingLength = std::max (0.05, state.vibratingLength);
    waveImpedance = std::max (1.0e-4, state.impedance);

    double targetFrequency = std::max (10.0, state.frequency);
    targetFrequency = std::min (targetFrequency, std::min (4000.0, fs * 0.45));

    double totalDelay = fs / targetFrequency;
    if (totalDelay > static_cast<double> (maxLoop - 16))
    {
        totalDelay = static_cast<double> (maxLoop - 16);
        targetFrequency = fs / totalDelay;
    }

    bendTargetSemitones = setup.bentSemitones;
    bendRatio = std::pow (2.0f, bendTargetSemitones / 12.0f);
    bendRatioCurrent = bendRatio;
    bendSmoothCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.012 * fs)));
    delaySmoothCoeff = static_cast<double> (1.0 - std::exp (-1.0 / (0.010 * fs)));
    dampSmoothCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.030 * fs)));

    // --- Loop loss: string internal damping only --------------------------
    //
    // Measured behaviour (Paté et al. 2014; Grzybowski et al. 2025): a solid
    // body sustains for seconds, low strings longer than high strings, and
    // higher partials always decay faster.  T60 here is the *isolated string*
    // figure; the structural loss is added by the junction at run time, so the
    // final decay is shorter than this and varies with the body's impedance.
    const float brightness = juce::jlimit (0.0f, 1.0f,
        0.30f + 0.70f * velocity) * (0.60f + 0.40f * intensity);

    // The loop filter multiplies the circulating wave once per ROUND TRIP, not
    // once per sample, so the T60 gain is referred to the fundamental:
    //   g^(T60 * f0) = 10^-3   =>   g = 10^(-3 / (T60 * f0))
    // (Using fs here would make the string ring ~fs/f0 times too long.)
    const double t60Internal = clampd (12.0 * std::pow (82.407 / targetFrequency, 0.26), 4.0, 16.0);
    loopLossInternal = static_cast<float> (std::pow (10.0, -3.0 / (t60Internal * targetFrequency)));

    // Palm/release damping: ESTIMATED T60 from the physical process of a hand
    // resting on the string near the bridge (much shorter for low strings).
    // Palm contact: the hand rests on the string just in front of the bridge.
    // ESTIMATED T60 0.25-0.9 s - a palm-muted chug rings for roughly half a
    // second before the contact absorbs the energy (measured decay of palm-muted
    // electric guitar notes is of this order).
    const double palmT60 = clampd (0.55 * std::pow (82.407 / targetFrequency, 0.18), 0.25, 0.90);
    loopLossDamped = static_cast<float> (std::pow (10.0, -3.0 / (palmT60 * targetFrequency)));

    dampTarget = juce::jlimit (0.0f, 1.0f, setup.palmMute);
    dampCurrent = dampTarget;

    // Frequency-dependent internal loss: the loop filter removes upper partials
    // faster, which is what measured spectra of plucked strings show.
    const double fc1 = clampd (2400.0 + 5200.0 * brightness, 2200.0, 11000.0);
    const double fc2 = std::min (15000.0, fs * 0.42);
    lp1CoeffBase = static_cast<float> (std::exp (-kTwoPi * fc1 / fs));
    lp1CoeffDamped = static_cast<float> (std::exp (-kTwoPi * 850.0 / fs));
    lp2Coeff = static_cast<float> (std::exp (-kTwoPi * fc2 / fs));

    // --- Stiff-string dispersion -----------------------------------------
    const double omega = kTwoPi / totalDelay;
    double inharmonicity = clampd (state.inharmonicity,
                                   phys::kMinInharmonicity,
                                   phys::kMaxInharmonicity);

    double apMagnitude = findDispersionCoefficient (totalDelay, inharmonicity);

    auto filterDelayFor = [this, omega] (double magnitude)
    {
        double delay = onePolePhaseDelay (lp1CoeffBase, omega)
                     + onePolePhaseDelay (lp2Coeff, omega);
        for (int i = 0; i < kDispersionSections; ++i)
            delay += allpassPhaseDelay (magnitude, omega);
        return delay;
    };

    filterPhaseDelay = filterDelayFor (apMagnitude);

    double delayBase = totalDelay - filterPhaseDelay;
    if (delayBase < 4.0)
    {
        apMagnitude = 0.0;
        filterPhaseDelay = filterDelayFor (0.0);
        delayBase = totalDelay - filterPhaseDelay;
    }

    dispersionCoeff = static_cast<float> (-apMagnitude);
    delayBase = clampd (delayBase, 2.0, static_cast<double> (maxLoop - 10));
    smoothDelay = delayBase;
    baseTotalDelay = delayBase + filterPhaseDelay;

    // --- Aperture geometry ------------------------------------------------
    //
    // The pickup averages the string over its pole footprint (Tillman;
    // Lemme).  Width is a measured 2.5 in for a humbucker, expressed as a
    // fraction of the *vibrating* length so fretting moves the comb correctly.
    const double apertureFraction = clampd (setup.apertureWidthMetres / vibratingLength, 0.01, 0.30);
    const double pickupFraction = clampd (
        static_cast<double> (setup.pickupPosition) * (phys::kScaleLengthMetres / vibratingLength),
        0.01, 0.45);

    apertureTaps = 5;
    for (int i = 0; i < apertureTaps; ++i)
    {
        const double offset = (static_cast<double> (i) - 2.0) * 0.25 * apertureFraction;
        apertureFractions[i] = static_cast<float> (clampd (pickupFraction + offset, 0.004, 0.60));
    }

    // --- Pick / string collision ------------------------------------------
    //
    // A pick displaces the string over its own contact width and releases it.
    // Thickness therefore sets the width of the imposed displacement, and a
    // harder collision (higher velocity) shortens the effective contact time,
    // which brightens the excitation.  This is what velocity must change, not
    // just the output level.
    const int shapeLength = juce::jlimit (16, bufferSize / 2 - 8,
                                          static_cast<int> (std::ceil (delayBase)));

    const double pickFraction = clampd (
        static_cast<double> (setup.pickPosition) * (phys::kScaleLengthMetres / vibratingLength),
        0.02, 0.45);

    const double contactFraction = clampd ((setup.pickThicknessMm * 1.0e-3) / vibratingLength,
                                           0.001, 0.05);

    auto& shape = shapeScratch;
    std::fill (shape.begin(), shape.end(), 0.0f);

    const bool harmonic = setup.harmonicNumber >= 2;

    if (harmonic)
    {
        const double n = static_cast<double> (setup.harmonicNumber);
        for (int i = 0; i < shapeLength; ++i)
        {
            const double p = static_cast<double> (i) / static_cast<double> (shapeLength);
            double value = std::sin (n * kTwoPi * 0.5 * p);

            // A lightly touched harmonic still contains neighbouring partials in
            // proportion to how far the touch point is from their nodes.
            value += setup.pinch ? 0.22 * std::sin ((n + 1.0) * kTwoPi * 0.5 * p)
                                 : 0.08 * std::sin ((n + 2.0) * kTwoPi * 0.5 * p);

            shape[static_cast<size_t> (i)] = static_cast<float> (value);
        }
    }
    else
    {
        for (int i = 0; i < shapeLength; ++i)
        {
            const double p = static_cast<double> (i) / static_cast<double> (shapeLength);
            const double triangle = p < pickFraction
                                        ? p / std::max (1.0e-4, pickFraction)
                                        : (1.0 - p) / std::max (1.0e-4, 1.0 - pickFraction);
            shape[static_cast<size_t> (i)] = static_cast<float> (triangle);
        }
    }

    // Contact width: box-average over the pick's footprint (the physical
    // aperture of the collision itself).
    const int contactSamples = std::max (1, static_cast<int> (std::lround (contactFraction
                                                                          * static_cast<double> (shapeLength))));
    if (contactSamples > 1)
    {
        float running = 0.0f;
        std::vector<float> temp (static_cast<size_t> (shapeLength), 0.0f);
        for (int i = 0; i < shapeLength; ++i)
        {
            running += shape[static_cast<size_t> (i)];
            if (i >= contactSamples)
                running -= shape[static_cast<size_t> (i - contactSamples)];
            temp[static_cast<size_t> (i)] = running / static_cast<float> (contactSamples);
        }
        std::copy (temp.begin(), temp.end(), shape.begin());
    }

    // Release sharpness: a fast, hard pick contact leaves a sharper corner.
    const double contactSeconds = clampd (2.4e-4 + 9.0e-4 * (1.0 - velocity) * (0.5 + 0.5 * intensity),
                                          1.6e-4, 2.5e-3);
    const double releaseCutoff = clampd (1.0 / (kTwoPi * contactSeconds), 600.0, 14000.0);
    const float releaseCoeff = static_cast<float> (std::exp (-kTwoPi * std::min (releaseCutoff, fs * 0.4) / fs));

    float state1 = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < shapeLength; ++i)
    {
        state1 += (1.0f - releaseCoeff) * (shape[static_cast<size_t> (i)] - state1);
        shape[static_cast<size_t> (i)] = state1;
        peak = std::max (peak, std::abs (state1));
    }

    // Pluck depth in millimetres.  ESTIMATED: a firm pick on a low string
    // displaces it roughly 1-2 mm; the model's pickup gap is 2 mm.
    float amplitude = 1.35f * std::pow (velocity, 1.30f) * (0.55f + 0.45f * intensity);
    if (harmonic)
        amplitude *= 0.85f;

    if (peak > 1.0e-6f)
        for (int i = 0; i < shapeLength; ++i)
            shape[static_cast<size_t> (i)] = shape[static_cast<size_t> (i)] / peak * amplitude;

    // Pick scrape: a very short noise burst injected into the string state, so
    // it is part of the vibration rather than a detached click.
    const int noiseLength = std::min (shapeLength, static_cast<int> (0.005 * fs));
    const float noiseAmount = (harmonic ? 0.008f : 0.055f) * velocity
                            * (0.4f + 0.6f * intensity) * (setup.pinch ? 1.6f : 1.0f);

    float previousNoise = 0.0f;
    for (int i = 0; i < noiseLength; ++i)
    {
        const float raw = nextNoise();
        const float highPassed = raw - previousNoise;
        previousNoise = raw;
        const float envelope = 1.0f - static_cast<float> (i) / static_cast<float> (noiseLength);
        shape[static_cast<size_t> (i)] += highPassed * noiseAmount * envelope * envelope * amplitude;
    }

    std::copy (shape.begin(), shape.begin() + shapeLength, delayLine.begin());
    writePos = shapeLength;

    lastAmplitude = 0.0f;
    incidentOut = 0.0f;
}

void PhysicalString::setBentSemitones (float semitones) noexcept
{
    bendTargetSemitones = juce::jlimit (-24.0f, 24.0f, semitones);
    bendRatio = std::pow (2.0f, bendTargetSemitones / 12.0f);
}

void PhysicalString::setPalmMuteTarget (float amount) noexcept
{
    dampTarget = juce::jlimit (0.0f, 1.0f, amount);
}

void PhysicalString::noteOff (float t60Seconds) noexcept
{
    const double seconds = std::max (0.02, static_cast<double> (t60Seconds));
    const double fundamental = std::max (10.0, fs / std::max (2.0, baseTotalDelay));
    loopLossDamped = static_cast<float> (std::pow (10.0, -3.0 / (seconds * fundamental)));
    dampTarget = 1.0f;
}

void PhysicalString::kill (float fadeSeconds) noexcept
{
    if (! active)
        return;

    dying = true;
    const double seconds = std::max (0.002, static_cast<double> (fadeSeconds));
    fadeCoeff = static_cast<float> (std::exp (-6.907755 / (seconds * fs)));
}

float PhysicalString::pullIncident() noexcept
{
    if (! active)
    {
        incidentOut = 0.0f;
        return 0.0f;
    }

    if (dying)
    {
        fadeGain *= fadeCoeff;
        if (fadeGain < 1.0e-4f)
        {
            active = false;
            incidentOut = 0.0f;
            return 0.0f;
        }
    }

    dampCurrent += (dampTarget - dampCurrent) * dampSmoothCoeff;
    bendRatioCurrent += (bendRatio - bendRatioCurrent) * bendSmoothCoeff;

    const double targetDelay = baseTotalDelay / static_cast<double> (bendRatioCurrent) - filterPhaseDelay;
    smoothDelay += (targetDelay - smoothDelay) * delaySmoothCoeff;
    smoothDelay = clampd (smoothDelay, 2.0, static_cast<double> (maxLoop - 6));

    float s = readFractional (smoothDelay);

    const float a1 = lp1CoeffBase + dampCurrent * (lp1CoeffDamped - lp1CoeffBase);
    const float gain = loopLossInternal + dampCurrent * (loopLossDamped - loopLossInternal);

    lp1State += (1.0f - a1) * (s - lp1State);
    float f = lp1State;
    lp2State += (1.0f - lp2Coeff) * (f - lp2State);
    f = lp2State;
    f *= gain;

    for (int i = 0; i < kDispersionSections; ++i)
    {
        const float y = dispersionCoeff * f + dispersionState[i];
        dispersionState[i] = f - dispersionCoeff * y;
        f = y;
    }

    incidentOut = f;
    return f;
}

void PhysicalString::pushReflected (float reflected) noexcept
{
    if (! active)
        return;

    delayLine[static_cast<size_t> (writePos)] = reflected * fadeGain;
    writePos = (writePos + 1) & bufferMask;
}

float PhysicalString::senseAperture() noexcept
{
    if (! active)
        return 0.0f;

    // Magnetic sensing at a distance x from the bridge sees the difference
    // between the wave now and the wave that travelled to the pickup and back,
    // i.e.  y(x) = s[n] - s[n - 2x fs / c].  The round trip 2x/c is q * P
    // samples for q = x/L, so the nulls fall exactly on the harmonics that have
    // a node at the pickup (q = k/n), which is the measured behaviour
    // (Tillman; Lemme).  Averaging several such taps across the pole footprint
    // adds the real aperture roll-off of a humbucker.
    const float base = readFractional (smoothDelay);
    double sum = 0.0;
    for (int i = 0; i < apertureTaps; ++i)
    {
        const double q = static_cast<double> (apertureFractions[i]);
        sum += static_cast<double> (base)
             - static_cast<double> (readFractional (smoothDelay * (1.0 + q)));
    }

    const float displacement = static_cast<float> (sum / static_cast<double> (apertureTaps));
    lastAmplitude = std::abs (displacement);
    return displacement;
}

float PhysicalString::readFractional (double delay) const noexcept
{
    const double clamped = clampd (delay, 1.0, static_cast<double> (bufferSize - 4));
    const int integerDelay = static_cast<int> (std::floor (clamped));
    const float fraction = static_cast<float> (clamped - static_cast<double> (integerDelay));

    int index = writePos - integerDelay;
    if (index < 0)
        index += bufferSize;

    const int im1 = (index - 1 + bufferSize) & bufferMask;
    const int i0 = index & bufferMask;
    const int i1 = (index + 1) & bufferMask;
    const int i2 = (index + 2) & bufferMask;

    const float t = -fraction;
    const float cM1 = -t * (t - 1.0f) * (t - 2.0f) / 6.0f;
    const float c0 = (t + 1.0f) * (t - 1.0f) * (t - 2.0f) / 2.0f;
    const float c1 = -(t + 1.0f) * t * (t - 2.0f) / 2.0f;
    const float c2 = (t + 1.0f) * t * (t - 1.0f) / 6.0f;

    return cM1 * delayLine[static_cast<size_t> (im1)]
         + c0 * delayLine[static_cast<size_t> (i0)]
         + c1 * delayLine[static_cast<size_t> (i1)]
         + c2 * delayLine[static_cast<size_t> (i2)];
}

float PhysicalString::nextNoise() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float> (static_cast<int32_t> (rng)) * (1.0f / 2147483648.0f);
}

double PhysicalString::onePolePhaseDelay (double coefficient, double omega) noexcept
{
    if (omega < 1.0e-9)
        return coefficient / std::max (1.0e-6, 1.0 - coefficient);

    const double numerator = coefficient * std::sin (omega);
    const double denominator = 1.0 - coefficient * std::cos (omega);
    return std::atan2 (numerator, denominator) / omega;
}

double PhysicalString::allpassPhaseDelay (double magnitude, double omega) noexcept
{
    if (omega < 1.0e-9)
        return (1.0 + magnitude) / std::max (1.0e-6, 1.0 - magnitude);

    const double numeratorAngle = std::atan2 (-std::sin (omega), std::cos (omega) - magnitude);
    const double denominatorAngle = std::atan2 (magnitude * std::sin (omega),
                                                 1.0 - magnitude * std::cos (omega));
    const double phase = numeratorAngle - denominatorAngle;
    double delay = -phase / omega;
    const double period = 2.0 * kTwoPi / omega;
    while (delay < 0.0)
        delay += period;
    return delay;
}

double PhysicalString::findDispersionCoefficient (double loopSamples, double inharmonicity) const noexcept
{
    if (inharmonicity < 1.0e-8 || loopSamples < 24.0)
        return 0.0;

    int harmonic = 8;
    while (harmonic > 2 && (kTwoPi * static_cast<double> (harmonic) / loopSamples) > 1.1)
        --harmonic;

    if (harmonic < 2)
        return 0.0;

    const double omega = kTwoPi * static_cast<double> (harmonic) / loopSamples;

    // Stiff-string partials: f_n = n f0 sqrt(1 + B n^2), so the round-trip delay
    // at harmonic n is shorter by loopSamples * B * n^2 / 2 samples.
    const double targetDelayReduction = loopSamples * inharmonicity
                                      * static_cast<double> (harmonic * harmonic) * 0.5;

    if (targetDelayReduction < 0.02)
        return 0.0;

    auto reductionFor = [omega] (double magnitude)
    {
        const double atDc = (1.0 + magnitude) / std::max (1.0e-6, 1.0 - magnitude);
        const double atOmega = allpassPhaseDelay (magnitude, omega);
        return (atDc - atOmega) * static_cast<double> (kDispersionSections);
    };

    double low = 0.0;
    double high = 0.97;

    if (reductionFor (high) < targetDelayReduction)
        return high;

    for (int i = 0; i < 24; ++i)
    {
        const double mid = 0.5 * (low + high);
        if (reductionFor (mid) < targetDelayReduction)
            low = mid;
        else
            high = mid;
    }

    return 0.5 * (low + high);
}

} // namespace ps
