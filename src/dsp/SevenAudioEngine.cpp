#include "dsp/SevenAudioEngine.h"

#include <algorithm>
#include <cmath>

#include "params/TuningManager.h"

namespace ps
{
void SevenAudioEngine::prepare (double sampleRate, int maxBlockSize)
{
    fs = std::max (8000.0, sampleRate);

    physical.prepare (fs, maxBlockSize);

    const int maxLoopSamples = static_cast<int> (fs / 15.0) + 128;

    voices.clear();
    voices.resize (static_cast<size_t> (kVoicePoolSize));
    for (auto& voice : voices)
        voice.string.prepare (fs, maxLoopSamples);

    pickup.prepare (fs);
    reset();
}

void SevenAudioEngine::reset()
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
        voice.age = 0;
    }

    pickup.reset();
    physical.reset();
    sustainDown = false;
    bendNormalized = 0.0f;
    noteCounter = 0;
    lastAssignedString = -1;
    lastAssignedFret = 0;
    lastHarmonic = 0;
}

void SevenAudioEngine::setParameters (const EngineParameters& newParameters)
{
    parameters = newParameters;
    physical.setParameters (newParameters);

    for (auto& voice : voices)
    {
        if (! voice.active)
            continue;

        voice.string.setBendSemitones (bendNormalized * static_cast<float> (parameters.bendRange));
        voice.string.setPalmMuteTarget (articulationMute (parameters.articulation));
    }
}

float SevenAudioEngine::articulationMute (Articulation articulation) const noexcept
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

float SevenAudioEngine::baseInharmonicityForString (int stringIndex) const noexcept
{
    const auto index = static_cast<size_t> (juce::jlimit (0, kNumStrings - 1, stringIndex));
    const double open = midiNoteToFrequency (parameters.tuning[index]);
    const double ratio = 82.407 / std::max (10.0, open);
    return static_cast<float> (juce::jlimit (8.0e-6, 3.0e-4, 5.5e-5 * std::pow (ratio, 1.25)));
}

void SevenAudioEngine::noteOn (int midiNote, float velocity)
{
    if (! parameters.useLegacyEngine)
    {
        physical.noteOn (midiNote, velocity);
        lastAssignedString = physical.getLastAssignedString();
        lastAssignedFret = physical.getLastAssignedFret();
        lastHarmonic = physical.getLastHarmonic();
        return;
    }

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

    // A physical string can only produce one note at a time. Retrigger the same
    // note on the same string (fast repeated picking) or fade the previous note
    // before starting a new pitch on this string.
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
            const bool sameNote = voice.note == midiNote
                               && voice.harmonicNumber == lastHarmonic.load (std::memory_order_relaxed);
            if (sameNote)
            {
                // Re-picking the same string starts a fresh excitation, but the
                // old string state is faded out rather than truncated. Resetting
                // a ringing delay line in place produces an audible click on
                // fast repeated notes.
                killVoice (voice, 0.006f);
                continue;
            }

            killVoice (voice, 0.004f);
        }
    }

    // Voice stealing: respect the polyphony limit set by the user.
    // The pool has kMaxPolyphony + 12 slots, but we only use up to the
    // user-configurable polyphony limit to avoid overpowering the mix.
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
        // The pool has headroom above the maximum polyphony, so this is only a
        // fallback. Recycle the quietest dying voice, otherwise the oldest one.
        int victim = -1;
        float quietest = 1.0e9f;
        for (int i = 0; i < static_cast<int> (voices.size()); ++i)
        {
            const auto& voice = voices[static_cast<size_t> (i)];
            if (voice.dying && voice.string.getCurrentAmplitude() < quietest)
            {
                quietest = voice.string.getCurrentAmplitude();
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

void SevenAudioEngine::startVoice (Voice& voice,
                                   int midiNote,
                                   float velocity,
                                   const StringAssignment& assignment,
                                   Articulation articulation)
{
    const int stringIndex = juce::jlimit (0, kNumStrings - 1, assignment.stringIndex);

    StringModel::NoteSetup setup;
    setup.stringIndex = stringIndex;
    setup.openMidiNote = parameters.tuning[static_cast<size_t> (stringIndex)];
    setup.midiNote = midiNote;
    setup.velocity = juce::jlimit (0.01f, 1.0f, velocity);
    setup.pickPosition = parameters.pickPosition;
    setup.pickupPosition = parameters.pickupPosition;
    setup.pickIntensity = parameters.pickIntensity;
    setup.palmMute = articulationMute (articulation);
    setup.baseInharmonicity = baseInharmonicityForString (stringIndex);
    setup.articulation = assignment.harmonic ? articulation : Articulation::open;
    setup.harmonicNumber = assignment.harmonicNumber;
    setup.bendSemitones = bendNormalized * static_cast<float> (parameters.bendRange);

    ++noteCounter;
    setup.seed = static_cast<uint32_t> (noteCounter * 2654435761ull)
               ^ static_cast<uint32_t> ((stringIndex + 1) * 2246822519u)
               ^ static_cast<uint32_t> (midiNote * 3266489917u);
    setup.seed |= 1u;

    voice.string.start (setup);
    voice.active = true;
    voice.dying = false;
    voice.sustained = false;
    voice.note = midiNote;
    voice.stringIndex = stringIndex;
    voice.fret = assignment.fret;
    voice.harmonicNumber = assignment.harmonicNumber;
    voice.velocity = setup.velocity;
    voice.age = noteCounter;
}

void SevenAudioEngine::noteOff (int midiNote)
{
    if (! parameters.useLegacyEngine)
    {
        physical.noteOff (midiNote);
        return;
    }

    for (auto& voice : voices)
    {
        if (! voice.active || voice.dying || voice.note != midiNote)
            continue;

        if (sustainDown)
        {
            voice.sustained = true;
            continue;
        }

        const float mute = articulationMute (parameters.articulation);
        // Longer release for realistic sustain - strings keep ringing after note-off
        // 0.2s for palm mute (tighter chug), 0.4s for open notes (longer ring)
        releaseVoice (voice, mute > 0.5f ? 0.2f : 0.4f);
    }
}

void SevenAudioEngine::allNotesOff (bool immediate)
{
    physical.allNotesOff (immediate);

    sustainDown = false;

    for (auto& voice : voices)
    {
        if (! voice.active)
            continue;

        voice.sustained = false;
        if (immediate)
            killVoice (voice, 0.010f);
        else
            releaseVoice (voice, 0.3f);
    }
}

void SevenAudioEngine::setSustainPedal (bool down)
{
    physical.setSustainPedal (down);

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
                const float mute = articulationMute (parameters.articulation);
                // Longer release when sustain pedal is released
                releaseVoice (voice, mute > 0.5f ? 0.2f : 0.4f);
            }
        }
    }
}

void SevenAudioEngine::setPitchBend (float normalizedBend)
{
    bendNormalized = juce::jlimit (-1.0f, 1.0f, normalizedBend);
    physical.setPitchBend (bendNormalized);

    for (auto& voice : voices)
        if (voice.active)
            voice.string.setBendSemitones (bendNormalized * static_cast<float> (parameters.bendRange));
}

void SevenAudioEngine::render (float* output, int numSamples)
{
    if (output == nullptr || numSamples <= 0)
        return;

    if (! parameters.useLegacyEngine)
    {
        physical.render (output, numSamples);
        return;
    }

    std::fill (output, output + numSamples, 0.0f);

    for (auto& voice : voices)
    {
        if (! voice.active)
            continue;

        for (int i = 0; i < numSamples; ++i)
            output[i] += voice.string.processSample();

        if (! voice.string.isActive())
            voice.active = false;
    }

    for (int i = 0; i < numSamples; ++i)
        output[i] = pickup.processSample (output[i]) * parameters.outputGain;
}

int SevenAudioEngine::getActiveVoiceCount() const noexcept
{
    if (! parameters.useLegacyEngine)
        return physical.getActiveVoiceCount();

    int count = 0;
    for (const auto& voice : voices)
        if (voice.active && ! voice.dying)
            ++count;
    return count;
}

void SevenAudioEngine::releaseVoice (Voice& voice, float seconds) noexcept
{
    voice.string.noteOff (seconds);
}

void SevenAudioEngine::killVoice (Voice& voice, float seconds) noexcept
{
    if (! voice.active)
        return;

    voice.dying = true;
    voice.sustained = false;
    voice.string.kill (seconds);
}

int SevenAudioEngine::findFreeSlot() noexcept
{
    for (int i = 0; i < static_cast<int> (voices.size()); ++i)
        if (! voices[static_cast<size_t> (i)].active)
            return i;
    return -1;
}

int SevenAudioEngine::findOldestActiveSlot() const noexcept
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
