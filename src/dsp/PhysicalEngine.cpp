#include "dsp/PhysicalEngine.h"

#include <algorithm>
#include <cmath>

#include "params/TuningManager.h"

namespace ps
{
namespace
{
constexpr double kTwoPi = 6.283185307179586;

// Pickup gap: ESTIMATED at 2 mm (typical pole-to-string clearance is 1.5-3 mm
// and the flux map is normalised by the gap).
constexpr double kPickupGapMm = 2.0;

// Output calibration for the induced EMF.  This is a fixed gain, not tonal
// shaping: it converts d(lambda)/dt into the plugin's nominal signal level.
constexpr double kEmfReferenceHz = 1000.0;
}

void PhysicalEngine::prepare (double sampleRate, int maxBlockSize)
{
    juce::ignoreUnused (maxBlockSize);

    fs = std::max (8000.0, sampleRate);

    const int maxLoopSamples = static_cast<int> (fs / 12.0) + 128;

    voices.clear();
    voices.resize (static_cast<size_t> (kVoicePoolSize));
    for (auto& voice : voices)
        voice.string.prepare (fs, maxLoopSamples);

    structure.prepare (fs);
    pickup.prepare (fs);

    emfScale = fs / (kTwoPi * kEmfReferenceHz);

    reset();
}

void PhysicalEngine::reset()
{
    for (auto& voice : voices)
    {
        voice.string.reset();
        voice.active = false;
        voice.dying = false;
        voice.sustained = false;
        voice.note = -1;
        voice.stringIndex = -1;
        voice.fret = 0;
        voice.harmonicNumber = 0;
        voice.velocity = 0.0f;
        voice.incident = 0.0f;
        voice.fluxPrev = 1.0;
        voice.age = 0;
    }

    structure.reset();
    structure.resetJunction (junction);
    pickup.reset();

    sustainDown = false;
    bendNormalized = 0.0f;
    noteCounter = 0;
    lastAssignedString = -1;
    lastAssignedFret = 0;
    lastHarmonic = 0;
}

void PhysicalEngine::setParameters (const EngineParameters& newParameters)
{
    parameters = newParameters;

    for (auto& voice : voices)
        if (voice.active)
            voice.string.setBentSemitones (bendNormalized * static_cast<float> (parameters.bendRange));
}

float PhysicalEngine::articulationDamping (Articulation articulation) const noexcept
{
    switch (articulation)
    {
        case Articulation::palmMute:
            return std::max (parameters.palmMute, 0.75f);
        case Articulation::naturalHarmonic:
        case Articulation::pinchHarmonic:
            return 0.0f;
        case Articulation::open:
        default:
            return juce::jlimit (0.0f, 1.0f, parameters.palmMute);
    }
}

void PhysicalEngine::noteOn (int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127 || voices.empty())
        return;

    const auto articulation = parameters.articulation;

    std::array<bool, kNumStrings> busy {};
    for (const auto& voice : voices)
        if (voice.active && ! voice.dying && voice.stringIndex >= 0)
            busy[static_cast<size_t> (voice.stringIndex)] = true;

    StringAssignment assignment;

    const bool wantsHarmonic = articulation == Articulation::naturalHarmonic
                            || articulation == Articulation::pinchHarmonic;

    bool gotHarmonic = false;
    if (wantsHarmonic)
        gotHarmonic = TuningManager::findHarmonic (midiNote, parameters.tuning, busy,
                                                   articulation == Articulation::pinchHarmonic,
                                                   assignment);

    if (! gotHarmonic)
        assignment = TuningManager::assignNote (midiNote, parameters.tuning, busy);

    if (assignment.stringIndex < 0 || assignment.stringIndex >= kNumStrings)
        return;

    lastAssignedString = assignment.stringIndex;
    lastAssignedFret = assignment.fret;
    lastHarmonic = gotHarmonic ? assignment.harmonicNumber : 0;

    int freeSlot = -1;
    for (int i = 0; i < static_cast<int> (voices.size()); ++i)
    {
        auto& voice = voices[static_cast<size_t> (i)];
        if (! voice.active && freeSlot < 0)
            freeSlot = i;

        if (! voice.active || voice.dying)
            continue;

        if (voice.stringIndex == assignment.stringIndex)
        {
            // A physical string can only carry one pitch per string; the old
            // note is damped by the fretting hand rather than truncated.
            killVoice (voice, voice.note == midiNote ? 0.006f : 0.004f);
        }
    }

    int activeCount = 0;
    for (const auto& voice : voices)
        if (voice.active && ! voice.dying)
            ++activeCount;

    const int maxActive = juce::jlimit (1, static_cast<int> (voices.size()) - 2, parameters.polyphony);
    if (activeCount >= maxActive)
    {
        const int victim = findOldestActiveSlot();
        if (victim >= 0 && victim != freeSlot)
            killVoice (voices[static_cast<size_t> (victim)], 0.004f);
    }

    if (freeSlot < 0)
    {
        int victim = -1;
        float quietest = 1.0e9f;
        for (int i = 0; i < static_cast<int> (voices.size()); ++i)
        {
            const auto& voice = voices[static_cast<size_t> (i)];
            if (voice.dying && voice.string.getAmplitude() < quietest)
            {
                quietest = voice.string.getAmplitude();
                victim = i;
            }
        }

        if (victim < 0)
            victim = findOldestActiveSlot();

        if (victim < 0)
            return;

        voices[static_cast<size_t> (victim)].string.reset();
        voices[static_cast<size_t> (victim)].active = false;
        voices[static_cast<size_t> (victim)].dying = false;
        freeSlot = victim;
    }

    startVoice (voices[static_cast<size_t> (freeSlot)], midiNote, velocity, assignment, articulation);
}

void PhysicalEngine::startVoice (Voice& voice,
                                 int midiNote,
                                 float velocity,
                                 const StringAssignment& assignment,
                                 Articulation articulation)
{
    const int stringIndex = juce::jlimit (0, kNumStrings - 1, assignment.stringIndex);
    const auto& design = stringSet[static_cast<size_t> (stringIndex)];

    const double fretRatio = std::pow (2.0, static_cast<double> (assignment.fret) / 12.0);
    const double vibratingLength = phys::kScaleLengthMetres / std::max (1.0, fretRatio);

    const double openFrequency = midiNoteToFrequency (parameters.tuning[static_cast<size_t> (stringIndex)]);
    const double soundFrequency = midiNoteToFrequency (midiNote);

    phys::StringState state;

    if (assignment.harmonic)
    {
        // A natural harmonic sounds on the open string: tension comes from the
        // open pitch, while the loop runs at the partial's frequency.
        state = phys::makeStringState (design, openFrequency, phys::kScaleLengthMetres);
        state.frequency = soundFrequency;
    }
    else
    {
        state = phys::makeStringState (design, soundFrequency, vibratingLength);
    }

    PhysicalString::Setup setup;
    setup.stringIndex = stringIndex;
    setup.openMidiNote = parameters.tuning[static_cast<size_t> (stringIndex)];
    setup.midiNote = midiNote;
    setup.velocity = juce::jlimit (0.02f, 1.0f, velocity);
    setup.pickPosition = parameters.pickPosition;
    setup.pickupPosition = parameters.pickupPosition;
    setup.pickIntensity = parameters.pickIntensity;
    setup.palmMute = articulationDamping (articulation);
    setup.bentSemitones = bendNormalized * static_cast<float> (parameters.bendRange);
    setup.harmonicNumber = assignment.harmonic ? assignment.harmonicNumber : 0;
    setup.pinch = articulation == Articulation::pinchHarmonic;

    ++noteCounter;
    setup.seed = static_cast<uint32_t> (noteCounter * 2654435761ull)
               ^ static_cast<uint32_t> ((stringIndex + 1) * 2246822519u)
               ^ static_cast<uint32_t> (midiNote * 3266489917u);
    setup.seed |= 1u;

    voice.string.start (setup, design, state);
    voice.active = true;
    voice.dying = false;
    voice.sustained = false;
    voice.note = midiNote;
    voice.stringIndex = stringIndex;
    voice.fret = assignment.fret;
    voice.harmonicNumber = assignment.harmonicNumber;
    voice.velocity = setup.velocity;
    voice.incident = 0.0f;
    voice.fluxPrev = 1.0;
    voice.age = noteCounter;
}

void PhysicalEngine::noteOff (int midiNote)
{
    for (auto& voice : voices)
    {
        if (! voice.active || voice.dying || voice.note != midiNote)
            continue;

        if (sustainDown)
        {
            voice.sustained = true;
            continue;
        }

        const float damping = articulationDamping (parameters.articulation);
        // The fretting hand damps the string: T60 collapses to a few hundred
        // milliseconds.  Palm-muted notes are stopped faster because the palm
        // is already in contact.
        releaseVoice (voice, damping > 0.5f ? 0.14f : 0.28f);
    }
}

void PhysicalEngine::allNotesOff (bool immediate)
{
    sustainDown = false;

    for (auto& voice : voices)
    {
        if (! voice.active)
            continue;

        voice.sustained = false;
        if (immediate)
            killVoice (voice, 0.010f);
        else
            releaseVoice (voice, 0.20f);
    }
}

void PhysicalEngine::setSustainPedal (bool down)
{
    if (sustainDown == down)
        return;

    sustainDown = down;

    if (! sustainDown)
    {
        for (auto& voice : voices)
        {
            if (voice.active && ! voice.dying && voice.sustained)
            {
                voice.sustained = false;
                const float damping = articulationDamping (parameters.articulation);
                releaseVoice (voice, damping > 0.5f ? 0.14f : 0.28f);
            }
        }
    }
}

void PhysicalEngine::setPitchBend (float normalizedBend)
{
    bendNormalized = juce::jlimit (-1.0f, 1.0f, normalizedBend);

    for (auto& voice : voices)
        if (voice.active)
            voice.string.setBentSemitones (bendNormalized * static_cast<float> (parameters.bendRange));
}

void PhysicalEngine::render (float* output, int numSamples)
{
    if (output == nullptr || numSamples <= 0)
        return;

    constexpr int kChunk = 64;
    int remaining = numSamples;
    float* target = output;

    while (remaining > 0)
    {
        const int chunk = std::min (kChunk, remaining);

        for (int n = 0; n < chunk; ++n)
        {
            double incidentSum = 0.0;
            double impedanceSum = 0.0;

            for (auto& voice : voices)
            {
                if (! voice.active)
                {
                    voice.incident = 0.0f;
                    continue;
                }

                voice.incident = voice.string.pullIncident();

                if (! voice.string.isActive())
                {
                    voice.active = false;
                    voice.incident = 0.0f;
                    continue;
                }

                incidentSum += static_cast<double> (voice.incident);
                impedanceSum += voice.string.getImpedance();
            }

            // Shared mechanical junction.  v = (2 b0 sum(f+) + C) / (1 + b0 sum(Z0))
            // where b0 is the structure's instantaneous admittance, C its
            // zero-input response.
            const double b0 = structure.directTerm();
            const double zeroInput = structure.zeroInputResponse (junction);
            const double denominator = 1.0 + b0 * impedanceSum;

            const double structureVelocity = denominator > 1.0e-12
                ? (2.0 * b0 * incidentSum + zeroInput) / denominator
                : 0.0;

            const double totalForce = 2.0 * incidentSum - impedanceSum * structureVelocity;
            structure.applyForce (junction, totalForce);

            double emfSum = 0.0;

            for (auto& voice : voices)
            {
                if (! voice.active)
                    continue;

                const double z0 = voice.string.getImpedance();
                const float reflected = static_cast<float> (static_cast<double> (voice.incident)
                                                            - z0 * structureVelocity);
                voice.string.pushReflected (reflected);

                if (! voice.string.isActive())
                {
                    voice.active = false;
                    continue;
                }

                const double displacement = static_cast<double> (voice.string.senseAperture());
                const double flux = MagneticPickupModel::fluxLinkage (displacement / kPickupGapMm);

                // Faraday: e = -d(lambda)/dt.
                emfSum += (flux - voice.fluxPrev) * emfScale;
                voice.fluxPrev = flux;
            }

            target[n] = pickup.processSample (static_cast<float> (emfSum)) * parameters.outputGain;
        }

        target += chunk;
        remaining -= chunk;
    }
}

int PhysicalEngine::getActiveVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& voice : voices)
        if (voice.active && ! voice.dying)
            ++count;
    return count;
}

double PhysicalEngine::getStringImpedance (int voiceSlot) const noexcept
{
    if (voiceSlot < 0 || voiceSlot >= static_cast<int> (voices.size()))
        return 0.0;
    return voices[static_cast<size_t> (voiceSlot)].string.getImpedance();
}

float PhysicalEngine::getVoiceIncident (int voiceSlot) const noexcept
{
    if (voiceSlot < 0 || voiceSlot >= static_cast<int> (voices.size()))
        return 0.0f;
    return voices[static_cast<size_t> (voiceSlot)].incident;
}

void PhysicalEngine::releaseVoice (Voice& voice, float t60) noexcept
{
    voice.string.noteOff (t60);
}

void PhysicalEngine::killVoice (Voice& voice, float seconds) noexcept
{
    if (! voice.active)
        return;

    voice.dying = true;
    voice.sustained = false;
    voice.string.kill (seconds);
}

int PhysicalEngine::findFreeSlot() noexcept
{
    for (int i = 0; i < static_cast<int> (voices.size()); ++i)
        if (! voices[static_cast<size_t> (i)].active)
            return i;
    return -1;
}

int PhysicalEngine::findOldestActiveSlot() const noexcept
{
    int oldest = -1;
    uint64_t oldestAge = UINT64_MAX;

    for (int i = 0; i < static_cast<int> (voices.size()); ++i)
    {
        const auto& voice = voices[static_cast<size_t> (i)];
        if (voice.active && ! voice.dying && voice.age < oldestAge)
        {
            oldestAge = voice.age;
            oldest = i;
        }
    }

    return oldest;
}

} // namespace ps
