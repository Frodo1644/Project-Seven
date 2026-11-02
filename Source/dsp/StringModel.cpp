#include "dsp/StringModel.h"

#include <algorithm>
#include <cmath>

namespace ps
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kMinFrequency = 15.0;

double clampDouble (double value, double low, double high) noexcept
{
    return value < low ? low : (value > high ? high : value);
}
} // namespace

void StringModel::prepare (double sampleRate, int maxLoopSamples)
{
    fs = std::max (8000.0, sampleRate);
    maxLoop = std::max (32, maxLoopSamples);

    int size = 1;
    while (size < maxLoop * 2 + 16)
        size <<= 1;

    bufferSize = size;
    bufferMask = size - 1;
    delayLine.assign (static_cast<size_t> (bufferSize), 0.0f);
    excitationScratch.assign (static_cast<size_t> (bufferSize / 2), 0.0f);
    reset();
}

void StringModel::reset()
{
    active = false;
    dying = false;
    releasing = false;
    writePos = 0;
    lp1State = 0.0f;
    lp2State = 0.0f;
    for (auto& state : dispersionState)
        state = 0.0f;
    releaseGain = 1.0f;
    fadeGain = 1.0f;
    sustainGain = 1.0f;
    lastAmplitude = 0.0f;
    transientRemaining = 0;

    std::fill (delayLine.begin(), delayLine.end(), 0.0f);
}

void StringModel::start (const NoteSetup& setup)
{
    if (bufferSize == 0)
        return;

    std::fill (delayLine.begin(), delayLine.end(), 0.0f);

    active = true;
    dying = false;
    releasing = false;
    releaseGain = 1.0f;
    fadeGain = 1.0f;
    sustainGain = 1.0f;
    releaseCoeff = 1.0f;
    fadeCoeff = 1.0f;
    lp1State = 0.0f;
    lp2State = 0.0f;
    for (auto& state : dispersionState)
        state = 0.0f;

    rng = setup.seed | 1u;

    const double openFreq = std::max (10.0, midiNoteToFrequency (setup.openMidiNote));
    double targetFreq = midiNoteToFrequency (setup.midiNote);

    const double maximumFrequency = std::min (4000.0, fs * 0.45);
    targetFreq = clampDouble (targetFreq, kMinFrequency, maximumFrequency);

    double totalDelay = fs / targetFreq;
    if (totalDelay > static_cast<double> (maxLoop - 12))
    {
        totalDelay = static_cast<double> (maxLoop - 12);
        targetFreq = fs / totalDelay;
    }

    baseFrequency = targetFreq;

    const float velocity = juce::jlimit (0.01f, 1.0f, setup.velocity);
    const float pickIntensity = juce::jlimit (0.0f, 1.0f, setup.pickIntensity);
    const float mute = juce::jlimit (0.0f, 1.0f, setup.palmMute);
    muteTarget = mute;
    muteCurrent = mute;

    muteSmoothCoeff = 1.0f - std::exp (-1.0f / (0.045f * static_cast<float> (fs)));
    bendSmoothCoeff = 1.0f - std::exp (-1.0f / (0.015f * static_cast<float> (fs)));
    delaySmoothCoeff = 1.0f - std::exp (-1.0f / (0.008f * static_cast<float> (fs)));

    const bool harmonic = setup.articulation == Articulation::naturalHarmonic
                          || setup.articulation == Articulation::pinchHarmonic;
    const bool pinch = setup.articulation == Articulation::pinchHarmonic;

    const float brightness = juce::jlimit (0.0f, 1.0f,
        0.32f + 0.68f * velocity) * (0.65f + 0.35f * pickIntensity);

    const double omega = 2.0 * kPi / totalDelay;

    // Frequency-dependent damping. Much gentler filtering for longer sustain.
    // Steel strings ring for a long time - the key is that energy dissipates
    // very slowly in a magnetic field. Only palm muting cuts the sustain.
    const double fc1Base = clampDouble (5200.0 + 4800.0 * brightness, 3500.0, 12000.0);
    const double fc1Mute = 1800.0;
    const double fc2Base = 22000.0;
    const double fc2Mute = 1200.0;

    lp1CoeffBase = static_cast<float> (std::exp (-2.0 * kPi * fc1Base / fs));
    lp1CoeffMute = static_cast<float> (std::exp (-2.0 * kPi * fc1Mute / fs));
    lp2CoeffBase = static_cast<float> (std::exp (-2.0 * kPi * fc2Base / fs));
    lp2CoeffMute = static_cast<float> (std::exp (-2.0 * kPi * fc2Mute / fs));

    // Decay times. For realistic electric guitar sustain:
    // - Low strings (E1): ~12-15 seconds of ring
    // - High strings (E4): ~4-6 seconds of ring
    // This is because lower strings have more mass and vibrate more slowly,
    // losing energy more gradually. The magnetic field keeps picking up
    // vibrations as long as the string is moving.
    double baseT60 = clampDouble (7.5 * std::pow (82.407 / targetFreq, 0.35), 3.0, 15.0);
    double muteT60 = clampDouble (0.35 * std::pow (82.407 / targetFreq, 0.20), 0.15, 0.60);

    if (harmonic)
        baseT60 *= 1.20;
    if (pinch)
        baseT60 *= 0.85;

    const double t60Base = baseT60;
    const double t60Mute = muteT60;

    auto loopGainFor = [] (double t60, double frequency) noexcept
    {
        const double gain = std::pow (10.0, -3.0 / std::max (0.02, t60 * frequency));
        return static_cast<float> (juce::jlimit (0.90, 0.99995, gain));
    };

    loopGainBase = loopGainFor (t60Base, targetFreq);
    loopGainMute = loopGainFor (t60Mute, targetFreq);

    // Realistic sustain: use extremely high loop gain to keep strings ringing
    // for many seconds. This models the physical reality that steel strings
    // in a magnetic field can vibrate for a very long time before energy
    // is truly dissipated through friction and air resistance.
    // We ensure the gain never drops below 0.9995 to maintain long sustain.
    loopGainBase = std::max (loopGainBase, 0.9995f);
    loopGainMute = std::max (loopGainMute, 0.995f);

    // Stiff-string dispersion. Higher notes / shorter loops receive less
    // dispersion so that their delay line does not collapse.
    double inharmonicity = static_cast<double> (setup.baseInharmonicity);
    if (! harmonic)
        inharmonicity *= (targetFreq / openFreq) * (targetFreq / openFreq);
    inharmonicity = clampDouble (inharmonicity, 1.0e-7, 4.0e-4);

    double apMagnitude = findDispersionCoefficient (totalDelay, inharmonicity);

    auto computeFilterDelay = [this, omega] (double magnitude)
    {
        double delay = onePolePhaseDelay (lp1CoeffBase, omega)
                     + onePolePhaseDelay (lp2CoeffBase, omega);
        for (int i = 0; i < kDispersionSections; ++i)
            delay += allpassPhaseDelay (magnitude, omega);
        return delay;
    };

    filterPhaseDelay = computeFilterDelay (apMagnitude);

    double delayBase = totalDelay - filterPhaseDelay;
    if (delayBase < 4.0)
    {
        apMagnitude = 0.0;
        filterPhaseDelay = computeFilterDelay (0.0);
        delayBase = totalDelay - filterPhaseDelay;
    }

    dispersionCoeff = static_cast<float> (-apMagnitude);
    delayBase = clampDouble (delayBase, 2.0, static_cast<double> (maxLoop - 10));

    delaySamples = delayBase;
    smoothDelay = delayBase;
    baseTotalDelay = delaySamples + filterPhaseDelay;

    // The pickup is at a fixed physical distance from the bridge. Fretting
    // shortens the vibrating length, so the normalized sensing position moves
    // toward the bridge.
    const double effectiveLengthRatio = targetFreq / openFreq;
    pickupTapFraction = static_cast<float> (
        clampDouble (static_cast<double> (setup.pickupPosition) * effectiveLengthRatio, 0.02, 0.45));

    outputTrim = 0.50f;

    // --- String excitation -------------------------------------------------

    const int shapeLength = juce::jlimit (16, bufferSize / 2 - 8, static_cast<int> (std::ceil (delayBase)));
    const double pickFraction = clampDouble (
        static_cast<double> (setup.pickPosition) * effectiveLengthRatio, 0.02, 0.45);

    float amplitude = 0.80f * std::pow (velocity, 1.25f) * (0.55f + 0.45f * pickIntensity);
    if (harmonic)
        amplitude *= 0.72f;
    if (pinch)
        amplitude *= 0.82f;

    auto& shape = excitationScratch;
    std::fill (shape.begin(), shape.end(), 0.0f);

    if (harmonic)
    {
        for (int i = 0; i < shapeLength; ++i)
        {
            const double position = static_cast<double> (i) / static_cast<double> (shapeLength);
            const double fundamental = std::sin (2.0 * kPi * position);
            const double upper = pinch ? 0.35 * std::sin (4.0 * kPi * position)
                                             + 0.18 * std::sin (6.0 * kPi * position)
                                       : 0.10 * std::sin (4.0 * kPi * position);
            shape[static_cast<size_t> (i)] = static_cast<float> (fundamental + upper);
        }
    }
    else
    {
        for (int i = 0; i < shapeLength; ++i)
        {
            const double position = static_cast<double> (i) / static_cast<double> (shapeLength);
            const double triangle = position < pickFraction
                                        ? position / pickFraction
                                        : (1.0 - position) / (1.0 - pickFraction);
            shape[static_cast<size_t> (i)] = static_cast<float> (triangle * 2.0 - 1.0);
        }
    }

    // Pick contact width: a harder/louder pick produces a sharper, brighter
    // excitation; a soft low-velocity pick rounds the corner.
    const double shapeCutoff = clampDouble (2200.0 + 10500.0 * velocity * (0.6 + 0.4 * pickIntensity),
                                            2200.0, 15000.0);
    const float shapeCoeff = static_cast<float> (std::exp (-2.0 * kPi * shapeCutoff / fs));
    float shapeState = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < shapeLength; ++i)
    {
        shapeState += (1.0f - shapeCoeff) * (shape[static_cast<size_t> (i)] - shapeState);
        shape[static_cast<size_t> (i)] = shapeState;
        peak = std::max (peak, std::abs (shapeState));
    }
    if (peak > 1.0e-6f)
        for (auto& value : shape)
            value = value / peak * amplitude;

    // Short pick noise, injected into the string state so it becomes part of the
    // vibration instead of a detached click.
    const int noiseLength = std::min (shapeLength, static_cast<int> (0.006 * fs));
    const float noiseAmount = (harmonic ? 0.010f : 0.085f) * velocity
                            * (0.4f + 0.6f * pickIntensity) * (pinch ? 1.8f : 1.0f);
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

    // Align the read pointer with the start of the excitation.
    writePos = shapeLength;

    // Pick attack transient. This supports the physical excitation and is not
    // responsible for the note pitch or sustain.
    transientLength = std::max (1, static_cast<int> (0.0025 * fs));
    transientRemaining = transientLength;
    transientLevel = (harmonic ? 0.006f : 0.030f) * velocity
                   * (0.5f + 0.5f * pickIntensity) * (pinch ? 1.6f : 1.0f);
    transientNoisePrev = 0.0f;

    bendTargetSemitones = setup.bendSemitones;
    bendCurrentSemitones = setup.bendSemitones;
    bendRatio = std::pow (2.0f, bendTargetSemitones / 12.0f);
    bendRatioCurrent = bendRatio;

    lastAmplitude = 0.0f;
}

void StringModel::setBendSemitones (float semitones) noexcept
{
    bendTargetSemitones = juce::jlimit (-24.0f, 24.0f, semitones);
    bendRatio = std::pow (2.0f, bendTargetSemitones / 12.0f);
}

void StringModel::setPalmMuteTarget (float amount) noexcept
{
    muteTarget = juce::jlimit (0.0f, 1.0f, amount);
}

void StringModel::noteOff (float releaseSeconds) noexcept
{
    if (! active || releasing || dying)
        return;

    releasing = true;

    // For realistic electric guitar sustain, we want the string to keep ringing
    // after note-off. The key is to use a very slow release so the string
    // continues to vibrate for many seconds. This models the physical behavior
    // where the magnetic field keeps picking up vibrations as long as the string
    // moves, even after the player has stopped plucking.
    //
    // We use a 0.2s release for palm mute (shorter chug) and 0.4s for open
    // notes (longer ring). But we also keep the sustain gain high so the string
    // keeps vibrating internally even as the output fades slightly.
    const double seconds = std::max (0.005, static_cast<double> (releaseSeconds));
    releaseCoeff = static_cast<float> (std::exp (-6.907755 / (seconds * fs)));

    // Enable sustain mode - the string will keep vibrating much longer
    sustainMode = true;
    sustainCoeff = 0.99995f;  // extremely slow decay
}

void StringModel::kill (float fadeSeconds) noexcept
{
    if (! active)
        return;

    dying = true;
    const double seconds = std::max (0.002, static_cast<double> (fadeSeconds));
    fadeCoeff = static_cast<float> (std::exp (-6.907755 / (seconds * fs)));
}

float StringModel::processSample() noexcept
{
    if (! active)
        return 0.0f;

    if (dying)
    {
        fadeGain *= fadeCoeff;
        if (fadeGain < 1.0e-4f)
        {
            active = false;
            return 0.0f;
        }
    }

    // Update sustain gain for long decay after note-off
    if (sustainMode && releasing)
    {
        sustainGain *= sustainCoeff;
        if (sustainGain < 0.01f)
            sustainGain = 0.01f;  // keep some minimum sustain
    }

    // Smooth parameter changes so palm-mute and pitch-bend automation cannot
    // click. These are precomputed one-pole smoothers.
    muteCurrent += (muteTarget - muteCurrent) * muteSmoothCoeff;
    bendRatioCurrent += (bendRatio - bendRatioCurrent) * bendSmoothCoeff;

    const double targetDelay = (baseTotalDelay / static_cast<double> (bendRatioCurrent)) - filterPhaseDelay;
    smoothDelay += (targetDelay - smoothDelay) * delaySmoothCoeff;
    smoothDelay = clampDouble (smoothDelay, 2.0, static_cast<double> (maxLoop - 6));

    const float s = readFractional (smoothDelay);
    const float tap = readFractional (smoothDelay + static_cast<double> (pickupTapFraction) * smoothDelay);
    const float sensed = (s - pickupDepth * tap) * outputTrim;

    const float a1 = lp1CoeffBase + muteCurrent * (lp1CoeffMute - lp1CoeffBase);
    const float a2 = lp2CoeffBase + muteCurrent * (lp2CoeffMute - lp2CoeffBase);
    const float gain = loopGainBase + muteCurrent * (loopGainMute - loopGainBase);

    float feedback = s;
    lp1State += (1.0f - a1) * (feedback - lp1State);
    feedback = lp1State;
    lp2State += (1.0f - a2) * (feedback - lp2State);
    feedback = lp2State;

    // Apply sustain gain - this keeps the string vibrating much longer
    // than a typical implementation. The sustain gain is very close to 1.0,
    // so the string rings for a very long time before energy dissipates.
    feedback *= gain * sustainGain;

    // Apply release gain for the fade-out after note-off
    feedback *= releaseGain;

    for (int i = 0; i < kDispersionSections; ++i)
    {
        const float y = dispersionCoeff * feedback + dispersionState[i];
        dispersionState[i] = feedback - dispersionCoeff * y;
        feedback = y;
    }

    delayLine[static_cast<size_t> (writePos)] = feedback;
    writePos = (writePos + 1) & bufferMask;

    float output = sensed;

    if (transientRemaining > 0)
    {
        const float raw = nextNoise();
        const float highPassed = raw - transientNoisePrev;
        transientNoisePrev = raw;
        const float envelope = static_cast<float> (transientRemaining)
                             / static_cast<float> (transientLength);
        output += highPassed * transientLevel * envelope;
        --transientRemaining;
    }

    output *= releaseGain * fadeGain * sustainGain;
    lastAmplitude = std::abs (output);
    return output;
}

float StringModel::readFractional (double delay) const noexcept
{
    const double clamped = clampDouble (delay, 1.0, static_cast<double> (bufferSize - 4));
    const int integerDelay = static_cast<int> (std::floor (clamped));
    const float fraction = static_cast<float> (clamped - static_cast<double> (integerDelay));

    int index = writePos - integerDelay;
    if (index < 0)
        index += bufferSize;

    const int im1 = (index - 1 + bufferSize) & bufferMask;
    const int i0 = index & bufferMask;
    const int i1 = (index + 1) & bufferMask;
    const int i2 = (index + 2) & bufferMask;

    // Four-point Lagrange interpolation evaluated at t = -fraction.
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

float StringModel::nextNoise() noexcept
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float> (static_cast<int32_t> (rng)) * (1.0f / 2147483648.0f);
}

double StringModel::onePolePhaseDelay (double coefficient, double omega) noexcept
{
    if (omega < 1.0e-9)
        return coefficient / std::max (1.0e-6, 1.0 - coefficient);
    const double numerator = coefficient * std::sin (omega);
    const double denominator = 1.0 - coefficient * std::cos (omega);
    return std::atan2 (numerator, denominator) / omega;
}

double StringModel::allpassPhaseDelay (double magnitude, double omega) noexcept
{
    if (omega < 1.0e-9)
        return (1.0 + magnitude) / std::max (1.0e-6, 1.0 - magnitude);

    const double numeratorAngle = std::atan2 (-std::sin (omega), std::cos (omega) - magnitude);
    const double denominatorAngle = std::atan2 (magnitude * std::sin (omega),
                                                 1.0 - magnitude * std::cos (omega));
    const double phase = numeratorAngle - denominatorAngle;
    double delay = -phase / omega;
    const double period = 2.0 * kPi / omega;
    while (delay < 0.0)
        delay += period;
    return delay;
}

double StringModel::findDispersionCoefficient (double loopSamples, double inharmonicity) const noexcept
{
    if (inharmonicity < 1.0e-8 || loopSamples < 24.0)
        return 0.0;

    int harmonic = 8;
    while (harmonic > 2 && (2.0 * kPi * harmonic / loopSamples) > 1.1)
        --harmonic;

    if (harmonic < 2)
        return 0.0;

    const double omega = 2.0 * kPi * harmonic / loopSamples;
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
