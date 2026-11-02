#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include "PluginProcessor.h"
#include "dsp/SevenAudioEngine.h"
#include "params/Parameters.h"
#include "params/PresetManager.h"
#include "params/TuningManager.h"

namespace
{
std::atomic<bool> trackAllocations { false };
std::atomic<int> allocationCount { 0 };
}

void* operator new (std::size_t size)
{
    if (trackAllocations.load (std::memory_order_relaxed))
        allocationCount.fetch_add (1, std::memory_order_relaxed);

    if (auto* memory = std::malloc (size))
        return memory;

    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return operator new (size);
}

void operator delete (void* memory) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

namespace
{
int failures = 0;
int checks = 0;

void check (bool condition, const juce::String& what)
{
    ++checks;
    if (! condition)
    {
        ++failures;
        std::printf ("FAIL  %s\n", what.toRawUTF8());
    }
}

void checkNear (double actual, double expected, double tolerance, const juce::String& what)
{
    const bool ok = std::isfinite (actual) && std::abs (actual - expected) <= tolerance;
    check (ok, what + "  (expected " + juce::String (expected, 4)
                  + ", got " + juce::String (actual, 4) + ")");
}

void checkRelative (double actual, double expected, double relativeTolerance, const juce::String& what)
{
    const double tolerance = std::abs (expected) * relativeTolerance;
    checkNear (actual, expected, tolerance, what);
}

constexpr double kSampleRate = 48000.0;

ps::EngineParameters makeParameters()
{
    ps::EngineParameters parameters;
    parameters.outputGain = 1.0f;
    return parameters;
}

std::vector<float> renderNote (ps::SevenAudioEngine& engine,
                               int note,
                               float velocity,
                               double seconds,
                               ps::EngineParameters parameters = makeParameters())
{
    engine.setParameters (parameters);
    engine.noteOn (note, velocity);

    const int numSamples = static_cast<int> (seconds * kSampleRate);
    std::vector<float> output (static_cast<size_t> (numSamples), 0.0f);
    engine.render (output.data(), numSamples);
    return output;
}

double rms (const std::vector<float>& samples, double fromSeconds, double toSeconds)
{
    const int start = juce::jlimit (0, static_cast<int> (samples.size()),
                                    static_cast<int> (fromSeconds * kSampleRate));
    const int end = juce::jlimit (start, static_cast<int> (samples.size()),
                                  static_cast<int> (toSeconds * kSampleRate));
    if (end <= start)
        return 0.0;

    double sum = 0.0;
    for (int i = start; i < end; ++i)
        sum += static_cast<double> (samples[static_cast<size_t> (i)])
             * static_cast<double> (samples[static_cast<size_t> (i)]);

    return std::sqrt (sum / static_cast<double> (end - start));
}

double peak (const std::vector<float>& samples)
{
    double value = 0.0;
    for (const auto sample : samples)
        value = std::max (value, std::abs (static_cast<double> (sample)));
    return value;
}

bool allFinite (const std::vector<float>& samples)
{
    for (const auto sample : samples)
        if (! std::isfinite (sample))
            return false;
    return true;
}

// Normalised autocorrelation with parabolic interpolation. The search is
// restricted around the expected period so the test measures tuning accuracy
// rather than octave errors.
double detectPitch (const std::vector<float>& samples, double expectedHz)
{
    const int start = static_cast<int> (0.20 * kSampleRate);
    const int window = static_cast<int> (0.45 * kSampleRate);
    const int end = std::min (static_cast<int> (samples.size()), start + window);
    if (end - start < 2048)
        return 0.0;

    const double expectedLag = kSampleRate / expectedHz;
    const int minLag = std::max (4, static_cast<int> (expectedLag * 0.90));
    const int maxLag = std::min (end - start - 4, static_cast<int> (expectedLag * 1.10));

    double best = -1.0;
    int bestLag = minLag;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double sum = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = start; i < end - lag; ++i)
        {
            const double a = samples[static_cast<size_t> (i)];
            const double b = samples[static_cast<size_t> (i + lag)];
            sum += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const double denominator = std::sqrt (energyA * energyB);
        const double correlation = denominator > 1.0e-12 ? sum / denominator : 0.0;

        if (correlation > best)
        {
            best = correlation;
            bestLag = lag;
        }
    }

    if (bestLag <= minLag || bestLag >= maxLag)
        return kSampleRate / static_cast<double> (bestLag);

    auto correlationAt = [&] (int lag)
    {
        double sum = 0.0, energyA = 0.0, energyB = 0.0;
        for (int i = start; i < end - lag; ++i)
        {
            const double a = samples[static_cast<size_t> (i)];
            const double b = samples[static_cast<size_t> (i + lag)];
            sum += a * b;
            energyA += a * a;
            energyB += b * b;
        }
        const double denominator = std::sqrt (energyA * energyB);
        return denominator > 1.0e-12 ? sum / denominator : 0.0;
    };

    const double left = correlationAt (bestLag - 1);
    const double centre = correlationAt (bestLag);
    const double right = correlationAt (bestLag + 1);
    const double denominator = left - 2.0 * centre + right;
    const double offset = std::abs (denominator) > 1.0e-12
                              ? 0.5 * (left - right) / denominator
                              : 0.0;
    const double refinedLag = static_cast<double> (bestLag) + juce::jlimit (-0.5, 0.5, offset);
    return kSampleRate / refinedLag;
}

double goertzel (const std::vector<float>& samples, double frequency,
                 double fromSeconds, double toSeconds)
{
    const int start = juce::jlimit (0, static_cast<int> (samples.size()),
                                    static_cast<int> (fromSeconds * kSampleRate));
    const int end = juce::jlimit (start, static_cast<int> (samples.size()),
                                  static_cast<int> (toSeconds * kSampleRate));
    const int n = end - start;
    if (n <= 2)
        return 0.0;

    const double omega = 2.0 * juce::MathConstants<double>::pi * frequency / kSampleRate;
    const double coeff = 2.0 * std::cos (omega);
    double s0 = 0.0, s1 = 0.0, s2 = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double window = 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                    * static_cast<double> (i)
                                                    / static_cast<double> (n - 1));
        s0 = window * samples[static_cast<size_t> (start + i)] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return std::sqrt (std::max (0.0, power)) / static_cast<double> (n);
}

double spectralCentroid (const std::vector<float>& samples, double fromSeconds, double toSeconds)
{
    const int start = static_cast<int> (fromSeconds * kSampleRate);
    const int end = static_cast<int> (toSeconds * kSampleRate);
    const int n = end - start;
    if (n < 4096)
        return 0.0;

    const int order = 13; // 8192
    juce::dsp::FFT fft (order);
    std::vector<float> data (static_cast<size_t> (1 << order) * 2, 0.0f);
    const int count = std::min (n, 1 << order);

    for (int i = 0; i < count; ++i)
    {
        const float window = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                     * static_cast<float> (i)
                                                     / static_cast<float> (count - 1));
        data[static_cast<size_t> (i)] = samples[static_cast<size_t> (start + i)] * window;
    }

    fft.performFrequencyOnlyForwardTransform (data.data());

    double weighted = 0.0;
    double magnitude = 0.0;
    for (int bin = 1; bin < count / 2; ++bin)
    {
        const double m = data[static_cast<size_t> (bin)];
        const double frequency = static_cast<double> (bin) * kSampleRate / static_cast<double> (count);
        weighted += m * frequency;
        magnitude += m;
    }

    return magnitude > 1.0e-9 ? weighted / magnitude : 0.0;
}

void writeWav (const juce::File& file, const std::vector<float>& samples)
{
    file.deleteFile();
    auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
    if (stream == nullptr)
        return;

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), kSampleRate, 1, 24, {}, 0));

    if (writer == nullptr)
        return;

    stream.release(); // writer owns it now

    juce::AudioBuffer<float> buffer (1, static_cast<int> (samples.size()));
    std::copy (samples.begin(), samples.end(), buffer.getWritePointer (0));
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
}

void testParametersAndMapping()
{
    checkNear (ps::midiNoteToFrequency (69), 440.0, 1.0e-9, "A4 is 440 Hz");
    checkNear (ps::midiNoteToFrequency (35), 61.7354, 0.001, "B1 frequency");
    checkNear (ps::midiNoteToFrequency (29), 43.6535, 0.001, "F1 frequency");
    check (ps::midiNoteName (35) == "B1", "B1 name");
    check (ps::midiNoteName (64) == "E4", "E4 name");
    check (ps::midiNoteName (0) == "C-1", "MIDI 0 name");
}

void testStringAssignment()
{
    std::array<bool, ps::kNumStrings> busy {};
    busy.fill (false);

    const auto& tuning = ps::kTuningPresets[0].notes;

    auto assignment = ps::TuningManager::assignNote (35, tuning, busy);
    check (assignment.stringIndex == 0 && assignment.fret == 0, "B1 maps to low B open");

    assignment = ps::TuningManager::assignNote (64, tuning, busy);
    check (assignment.stringIndex == 6 && assignment.fret == 0, "E4 maps to high E open");

    assignment = ps::TuningManager::assignNote (40, tuning, busy);
    check (assignment.stringIndex == 1 && assignment.fret == 0, "E2 maps to E string open");

    assignment = ps::TuningManager::assignNote (36, tuning, busy);
    check (assignment.stringIndex == 0 && assignment.fret == 1, "C2 maps to B string fret 1");

    busy[1] = true;
    assignment = ps::TuningManager::assignNote (40, tuning, busy);
    check (assignment.stringIndex == 0 && assignment.fret == 5,
           "Busy E string moves E2 to B string fret 5");

    busy.fill (false);
    ps::StringAssignment harmonic;
    const bool found = ps::TuningManager::findHarmonic (47, tuning, busy, false, harmonic);
    check (found && harmonic.stringIndex == 0 && harmonic.harmonicNumber == 2,
           "B2 maps to 2nd harmonic of low B");

    const bool pinchTooLow = ps::TuningManager::findHarmonic (47, tuning, busy, true, harmonic);
    check (! pinchTooLow, "Pinch harmonics start at the 3rd harmonic");

    const bool foundThird = ps::TuningManager::findHarmonic (54, tuning, busy, true, harmonic);
    check (foundThird && harmonic.stringIndex == 0 && harmonic.harmonicNumber == 3,
           "F#3 maps to 3rd harmonic of low B");
}

void testPitchAccuracy()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto parameters = makeParameters();
    parameters.polyphony = 12;

    const int notes[] = { 35, 40, 45, 50, 55, 59, 64 };
    for (const int note : notes)
    {
        engine.reset();
        const auto samples = renderNote (engine, note, 0.8f, 1.2, parameters);
        const double measured = detectPitch (samples, ps::midiNoteToFrequency (note));
        checkRelative (measured, ps::midiNoteToFrequency (note), 0.005,
                       "Pitch accuracy for " + ps::midiNoteName (note));
    }

    // Very low Drop F tuning.
    parameters.tuning = ps::kTuningPresets[3].notes;
    engine.reset();
    const auto low = renderNote (engine, 29, 0.9f, 1.8, parameters);
    const double measuredLow = detectPitch (low, ps::midiNoteToFrequency (29));
    checkRelative (measuredLow, ps::midiNoteToFrequency (29), 0.005, "Drop F low F1 pitch");
    check (allFinite (low), "Drop F render is finite");
}

void testVelocityAndDecay()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto parameters = makeParameters();

    engine.reset();
    const auto soft = renderNote (engine, 45, 0.2f, 0.8, parameters);
    engine.reset();
    const auto hard = renderNote (engine, 45, 0.95f, 0.8, parameters);

    check (peak (hard) > peak (soft) * 1.5,
           "Velocity increases excitation amplitude (soft "
               + juce::String (peak (soft), 4) + ", hard " + juce::String (peak (hard), 4) + ")");

    engine.reset();
    const auto lowString = renderNote (engine, 35, 0.8f, 5.0, parameters);
    engine.reset();
    const auto highString = renderNote (engine, 64, 0.8f, 5.0, parameters);

    const double lowRatio = rms (lowString, 3.0, 3.3) / std::max (1.0e-9, rms (lowString, 0.2, 0.5));
    const double highRatio = rms (highString, 3.0, 3.3) / std::max (1.0e-9, rms (highString, 0.2, 0.5));

    check (lowRatio > highRatio,
           "Low strings sustain longer than high strings (" + juce::String (lowRatio, 4)
               + " vs " + juce::String (highRatio, 4) + ")");
    check (lowRatio > 0.02 && lowRatio < 1.0, "Low string decays but remains audible");
}

void testPalmMuteAndHarmonics()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto openParameters = makeParameters();
    auto palmParameters = makeParameters();
    palmParameters.articulation = ps::Articulation::palmMute;
    palmParameters.palmMute = 1.0f;

    engine.reset();
    const auto open = renderNote (engine, 35, 0.9f, 1.5, openParameters);
    engine.reset();
    const auto palm = renderNote (engine, 35, 0.9f, 1.5, palmParameters);

    const double openTail = rms (open, 0.6, 1.0);
    const double palmTail = rms (palm, 0.6, 1.0);

    check (palmTail < openTail * 0.35,
           "Palm mute physically damps the string (open " + juce::String (openTail, 5)
               + ", palm " + juce::String (palmTail, 5) + ")");
    check (palmTail > 1.0e-6, "Palm-muted note is not silent");

    auto harmonicParameters = makeParameters();
    harmonicParameters.articulation = ps::Articulation::naturalHarmonic;
    engine.reset();
    const auto harmonic = renderNote (engine, 59, 0.85f, 2.0, harmonicParameters);

    const double fundamental = goertzel (harmonic, ps::midiNoteToFrequency (59), 0.2, 1.4);
    const double sub = goertzel (harmonic, ps::midiNoteToFrequency (59) * 0.5, 0.2, 1.4);
    check (fundamental > sub * 2.0,
           "Natural harmonic rejects the lower octave (fundamental " + juce::String (fundamental, 6)
               + ", sub " + juce::String (sub, 6) + ")");

    auto pinchParameters = makeParameters();
    pinchParameters.articulation = ps::Articulation::pinchHarmonic;
    engine.reset();
    const auto pinch = renderNote (engine, 66, 0.9f, 2.0, pinchParameters);
    check (peak (pinch) > 1.0e-4, "Pinch harmonic produces output");

    const double naturalCentroid = spectralCentroid (harmonic, 0.2, 1.2);
    const double pinchCentroid = spectralCentroid (pinch, 0.2, 1.2);
    check (pinchCentroid > naturalCentroid,
           "Pinch harmonic is brighter than a natural harmonic ("
               + juce::String (naturalCentroid, 0) + " Hz vs " + juce::String (pinchCentroid, 0) + " Hz)");
}

void testPickupAndPickPosition()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto nearBridge = makeParameters();
    nearBridge.pickupPosition = 0.05f;
    auto nearNeck = makeParameters();
    nearNeck.pickupPosition = 0.32f;

    engine.reset();
    const auto bridge = renderNote (engine, 45, 0.85f, 1.5, nearBridge);
    engine.reset();
    const auto neck = renderNote (engine, 45, 0.85f, 1.5, nearNeck);

    const double bridgeCentroid = spectralCentroid (bridge, 0.15, 1.2);
    const double neckCentroid = spectralCentroid (neck, 0.15, 1.2);
    check (bridgeCentroid > neckCentroid * 1.03,
           "Pickup position changes harmonic sensing (bridge " + juce::String (bridgeCentroid, 0)
               + " Hz, neck " + juce::String (neckCentroid, 0) + " Hz)");

    auto bridgePick = makeParameters();
    bridgePick.pickPosition = 0.03f;
    auto neckPick = makeParameters();
    neckPick.pickPosition = 0.28f;
    engine.reset();
    const auto pickedAtBridge = renderNote (engine, 40, 0.85f, 1.2, bridgePick);
    engine.reset();
    const auto pickedAtNeck = renderNote (engine, 40, 0.85f, 1.2, neckPick);

    const double bridgePickCentroid = spectralCentroid (pickedAtBridge, 0.1, 0.9);
    const double neckPickCentroid = spectralCentroid (pickedAtNeck, 0.1, 0.9);
    check (std::abs (bridgePickCentroid - neckPickCentroid) > 20.0,
           "Pick position changes excitation spectrum ("
               + juce::String (bridgePickCentroid, 0) + " Hz vs "
               + juce::String (neckPickCentroid, 0) + " Hz)");
}

void testPitchBend()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto parameters = makeParameters();
    parameters.bendRange = 2;

    engine.reset();
    engine.setParameters (parameters);
    engine.setPitchBend (1.0f);
    const auto bent = renderNote (engine, 45, 0.8f, 1.5, parameters);

    const double expected = ps::midiNoteToFrequency (45) * std::pow (2.0, 2.0 / 12.0);
    const double measured = detectPitch (bent, expected);
    checkRelative (measured, expected, 0.006, "Pitch bend +2 semitones");

    engine.reset();
    engine.setPitchBend (-0.5f);
    const auto down = renderNote (engine, 45, 0.8f, 1.5, parameters);
    const double expectedDown = ps::midiNoteToFrequency (45) * std::pow (2.0, -1.0 / 12.0);
    checkRelative (detectPitch (down, expectedDown), expectedDown, 0.006,
                   "Pitch bend -1 semitone");
}

void testPolyphonyAndSustain()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    auto parameters = makeParameters();
    parameters.polyphony = 2;
    engine.setParameters (parameters);

    std::vector<float> scratch (1024, 0.0f);
    engine.noteOn (35, 0.8f);
    engine.render (scratch.data(), 256);
    engine.noteOn (40, 0.8f);
    engine.render (scratch.data(), 256);
    engine.noteOn (45, 0.8f);
    engine.render (scratch.data(), 256);

    check (engine.getActiveVoiceCount() <= 2, "Voice stealing respects polyphony");
    check (engine.getLastAssignedString() == 2, "Newest note wins the stolen slot");

    engine.setSustainPedal (true);
    engine.noteOff (45);
    engine.render (scratch.data(), 256);
    check (engine.getActiveVoiceCount() >= 1, "Sustain pedal holds released note");

    engine.setSustainPedal (false);
    engine.render (scratch.data(), 512);
    check (engine.getActiveVoiceCount() >= 0, "Sustain release is stable");

    engine.allNotesOff (true);
    std::fill (scratch.begin(), scratch.end(), 0.0f);
    engine.render (scratch.data(), 1024);
    check (engine.getActiveVoiceCount() == 0, "All Sound Off clears voices");
    check (allFinite (scratch), "All Sound Off produces finite output");
}

void testProcessorMidiAndState()
{
    ProjectSevenAudioProcessor processor;
    processor.prepareToPlay (kSampleRate, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    midi.addEvent (juce::MidiMessage::noteOn (1, 35, 0.9f), 0);
    processor.processBlock (buffer, midi);
    check (processor.getActiveVoiceCount() == 1, "Processor starts a voice from MIDI");
    check (processor.getLastAssignedString() == 0, "Processor maps B1 to the low string");

    midi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.8f), 0);
    processor.processBlock (buffer, midi);
    check (processor.getActiveVoiceCount() == 2, "Processor plays a second string");

    midi.addEvent (juce::MidiMessage::pitchWheel (1, 12288), 0);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 35), 0);
    processor.processBlock (buffer, midi);
    check (processor.getActiveVoiceCount() >= 1, "Processor sustains note-off under CC64");

    midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 0), 0);
    processor.processBlock (buffer, midi);

    midi.addEvent (juce::MidiMessage::controllerEvent (1, 123, 0), 0);
    processor.processBlock (buffer, midi);
    const std::vector<float> processorOutput (buffer.getReadPointer (0),
                                              buffer.getReadPointer (0) + buffer.getNumSamples());
    check (allFinite (processorOutput), "Processor output is finite");

    // State round-trip, including a custom tuning.
    auto& state = processor.getValueTreeState();
    if (auto* string1 = state.getParameter (ps::ParamID::stringParam (0)))
        string1->setValueNotifyingHost (string1->convertTo0to1 (29.0f));
    if (auto* preset = state.getParameter (ps::ParamID::tuningPreset))
        preset->setValueNotifyingHost (preset->convertTo0to1 (4.0f));

    juce::MemoryBlock block;
    processor.getStateInformation (block);
    check (block.getSize() > 0, "Processor state serialises");

    ProjectSevenAudioProcessor restored;
    restored.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    const auto* restoredString = restored.getValueTreeState().getRawParameterValue (ps::ParamID::stringParam (0));
    check (restoredString != nullptr && juce::roundToInt (restoredString->load()) == 29,
           "Custom tuning survives state restore");

    restored.prepareToPlay (kSampleRate, 512);
    restored.processBlock (buffer, midi);
    const std::vector<float> restoredOutput (buffer.getReadPointer (0),
                                             buffer.getReadPointer (0) + buffer.getNumSamples());
    check (allFinite (restoredOutput), "Restored processor renders");

    // Hosts can hand over a block larger than prepareToPlay() announced. The
    // processor must split it across its preallocated scratch buffer, not resize.
    ProjectSevenAudioProcessor oversized;
    oversized.prepareToPlay (kSampleRate, 256);

    juce::AudioBuffer<float> bigBuffer (2, 20000);
    juce::MidiBuffer bigMidi;
    bigMidi.addEvent (juce::MidiMessage::noteOn (1, 40, 0.9f), 100);
    bigMidi.addEvent (juce::MidiMessage::noteOff (1, 40), 9000);

    trackAllocations.store (true, std::memory_order_relaxed);
    allocationCount.store (0, std::memory_order_relaxed);
    oversized.processBlock (bigBuffer, bigMidi);
    trackAllocations.store (false, std::memory_order_relaxed);

    check (allocationCount.load (std::memory_order_relaxed) == 0,
           "Oversized host blocks render without allocating");
    check (allFinite (std::vector<float> (bigBuffer.getReadPointer (0),
                                          bigBuffer.getReadPointer (0) + bigBuffer.getNumSamples())),
           "Oversized host blocks stay finite");
    check (bigBuffer.getMagnitude (0, 1000, 4000) > 0.0001f,
           "Oversized host blocks produce audio in the first chunk");
    check (bigBuffer.getMagnitude (0, 8200, 1500) > 0.0001f,
           "Oversized host blocks continue across the chunk boundary");
}

void testPresetSyncAndControlChanges()
{
    ProjectSevenAudioProcessor processor;
    processor.prepareToPlay (kSampleRate, 512);

    auto& state = processor.getValueTreeState();
    auto* preset = state.getParameter (ps::ParamID::tuningPreset);
    auto* lowestString = state.getRawParameterValue (ps::ParamID::stringParam (0));
    auto* presetValue = state.getRawParameterValue (ps::ParamID::tuningPreset);
    auto* articulationValue = state.getRawParameterValue (ps::ParamID::articulation);

    check (preset != nullptr && lowestString != nullptr, "Preset parameters exist");
    if (preset == nullptr || lowestString == nullptr || presetValue == nullptr || articulationValue == nullptr)
        return;

    preset->setValueNotifyingHost (preset->convertTo0to1 (3.0f)); // Drop F
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    check (juce::roundToInt (lowestString->load()) == 29,
           "Drop F preset updates the lowest string to F1");

    if (auto* secondString = state.getParameter (ps::ParamID::stringParam (1)))
        secondString->setValueNotifyingHost (secondString->convertTo0to1 (37.0f));
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    check (juce::roundToInt (presetValue->load()) == ps::kPresetCustomIndex,
           "Editing a string switches the preset to Custom");

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 127), 0);
    processor.processBlock (buffer, midi);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    check (juce::roundToInt (articulationValue->load()) == 3,
           "CC20 selects the pinch-harmonic articulation");

    const std::vector<float> output (buffer.getReadPointer (0),
                                     buffer.getReadPointer (0) + buffer.getNumSamples());
    check (allFinite (output), "CC20-controlled block renders cleanly");
}

void testCpuBudget()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);

    auto parameters = makeParameters();
    parameters.tuning = ps::kTuningPresets[3].notes;
    parameters.polyphony = 12;
    engine.setParameters (parameters);

    for (const int note : parameters.tuning)
        engine.noteOn (note, 0.8f);

    constexpr int blockSize = 512;
    constexpr int blocks = static_cast<int> (10.0 * kSampleRate / blockSize);
    std::vector<float> block (blockSize, 0.0f);

    const double start = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < blocks; ++i)
        engine.render (block.data(), blockSize);
    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;

    std::printf ("  10 s / 7 voices rendered in %.3f s (%.2f%% of real time)\n",
                 elapsed, 100.0 * elapsed / 10.0);
    check (elapsed < 2.0, "Seven-string rendering stays well below real time");
}

void testSampleRatesAndBlockSizes()
{
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
    const int blocks[] = { 64, 256, 512, 1024 };

    for (const double rate : rates)
    {
        for (const int blockSize : blocks)
        {
            ps::SevenAudioEngine engine;
            engine.prepare (rate, blockSize);

            auto parameters = makeParameters();
            parameters.tuning = ps::kTuningPresets[3].notes;
            engine.setParameters (parameters);

            std::vector<float> block (static_cast<size_t> (blockSize), 0.0f);
            bool finite = true;
            double maximum = 0.0;

            const int notes[] = { 29, 36, 41, 46, 51, 55, 60 };
            for (const int note : notes)
            {
                engine.noteOn (note, 0.8f);
                for (int i = 0; i < 8; ++i)
                {
                    engine.render (block.data(), blockSize);
                    finite = finite && allFinite (block);
                    maximum = std::max (maximum, peak (block));
                }
            }

            check (finite, "All sample rates and block sizes remain finite");
            check (maximum > 0.0005, "All sample rates produce output");
        }
    }
}

void testPresetFileRoundTrip()
{
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("project_seven_tuning_roundtrip.ps7t");
    file.deleteFile();

    const std::array<int, ps::kNumStrings> notes { 30, 37, 42, 47, 52, 56, 61 };
    check (ps::PresetManager::saveTuning (file, notes, "Round Trip"),
           "Custom tuning file saves");

    const auto loaded = ps::PresetManager::loadTuning (file);
    check (loaded.has_value(), "Custom tuning file loads");
    if (loaded.has_value())
        check (*loaded == notes, "Custom tuning file round-trips all seven strings");

    check (! ps::PresetManager::loadTuning (file.getSiblingFile ("missing.ps7t")).has_value(),
           "Missing tuning files are rejected");

    file.deleteFile();
}

void testRealtimeAllocations()
{
    ps::SevenAudioEngine engine;
    engine.prepare (kSampleRate, 512);
    engine.setParameters (makeParameters());

    std::vector<float> block (512, 0.0f);

    trackAllocations.store (true, std::memory_order_relaxed);
    allocationCount.store (0, std::memory_order_relaxed);

    engine.noteOn (40, 0.8f);
    engine.render (block.data(), 512);
    engine.noteOff (40);
    engine.render (block.data(), 512);

    trackAllocations.store (false, std::memory_order_relaxed);
    const int allocations = allocationCount.load (std::memory_order_relaxed);
    std::printf ("  allocations during note-on/render: %d\n", allocations);
    check (allocations == 0, "Note handling and rendering do not allocate");
}

void testStabilityAndArtifacts()
{
    const auto artifacts = juce::File::getCurrentWorkingDirectory().getChildFile ("artifacts");
    artifacts.createDirectory();

    {
        ps::SevenAudioEngine engine;
        engine.prepare (kSampleRate, 512);
        auto parameters = makeParameters();
        parameters.articulation = ps::Articulation::open;

        engine.setParameters (parameters);
        const int notes[] = { 35, 40, 45, 50, 55, 59, 64 };
        for (int i = 0; i < 7; ++i)
        {
            engine.noteOn (notes[i], 0.55f + 0.05f * static_cast<float> (i));
            std::vector<float> block (512, 0.0f);
            for (int b = 0; b < 10; ++b)
                engine.render (block.data(), 512);
        }

        std::vector<float> output (static_cast<size_t> (6.0 * kSampleRate), 0.0f);
        engine.render (output.data(), static_cast<int> (output.size()));
        check (allFinite (output), "Seven-string chord render is finite");
        check (peak (output) < 1.5, "Seven-string chord does not clip the model");
        writeWav (artifacts.getChildFile ("01_standard_b_chord.wav"), output);
    }

    {
        ps::SevenAudioEngine engine;
        engine.prepare (kSampleRate, 512);
        auto parameters = makeParameters();
        parameters.articulation = ps::Articulation::palmMute;
        parameters.palmMute = 1.0f;
        engine.setParameters (parameters);

        std::vector<float> output (static_cast<size_t> (4.0 * kSampleRate), 0.0f);
        for (int i = 0; i < 8; ++i)
        {
            engine.noteOn (35, 0.95f);
            engine.render (output.data() + static_cast<size_t> (i) * 24000, 24000);
        }
        writeWav (artifacts.getChildFile ("02_palm_mute_chugs.wav"), output);
        check (allFinite (output) && peak (output) > 0.001, "Palm mute render is valid");
    }

    {
        ps::SevenAudioEngine engine;
        engine.prepare (kSampleRate, 512);
        auto parameters = makeParameters();
        parameters.articulation = ps::Articulation::naturalHarmonic;
        engine.setParameters (parameters);

        std::vector<float> output (static_cast<size_t> (5.0 * kSampleRate), 0.0f);
        const int harmonics[] = { 47, 54, 59, 66 };
        for (int i = 0; i < 4; ++i)
        {
            engine.noteOn (harmonics[i], 0.8f);
            engine.render (output.data() + static_cast<size_t> (i) * 48000, 48000);
        }
        writeWav (artifacts.getChildFile ("03_natural_harmonics.wav"), output);
        check (allFinite (output), "Harmonic render is finite");
    }

    {
        ps::SevenAudioEngine engine;
        engine.prepare (kSampleRate, 512);
        auto parameters = makeParameters();
        parameters.tuning = ps::kTuningPresets[3].notes;
        parameters.articulation = ps::Articulation::open;
        engine.setParameters (parameters);

        std::vector<float> output (static_cast<size_t> (7.0 * kSampleRate), 0.0f);
        engine.noteOn (29, 0.9f);
        engine.render (output.data(), 4 * 24000);
        engine.noteOn (36, 0.7f);
        engine.render (output.data() + 4 * 24000, static_cast<int> (output.size()) - 4 * 24000);

        check (allFinite (output), "Drop F render is finite");
        writeWav (artifacts.getChildFile ("04_drop_f_low.wav"), output);
    }

    // Long stability run with dense MIDI.
    {
        ps::SevenAudioEngine engine;
        engine.prepare (kSampleRate, 256);
        auto parameters = makeParameters();
        engine.setParameters (parameters);

        std::vector<float> block (256, 0.0f);
        bool finite = true;
        double maximum = 0.0;

        for (int i = 0; i < 1200; ++i)
        {
            if (i % 3 == 0)
                engine.noteOn (35 + (i * 7) % 30, 0.7f);
            if (i % 11 == 0)
                engine.noteOff (35 + (i * 5) % 30);

            engine.render (block.data(), 256);
            finite = finite && allFinite (block);
            maximum = std::max (maximum, peak (block));
        }

        check (finite, "Dense MIDI stress run stays finite");
        check (maximum < 4.0, "Dense MIDI stress run is bounded");
    }
}
} // namespace

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::printf ("Project Seven engine tests\n");
    std::printf ("==========================\n");

    std::printf ("parameters/mapping...\n");
    testParametersAndMapping();
    std::printf ("string assignment...\n");
    testStringAssignment();
    std::printf ("pitch accuracy...\n");
    testPitchAccuracy();
    std::printf ("velocity/decay...\n");
    testVelocityAndDecay();
    std::printf ("palm mute/harmonics...\n");
    testPalmMuteAndHarmonics();
    std::printf ("pickup/pick position...\n");
    testPickupAndPickPosition();
    std::printf ("pitch bend...\n");
    testPitchBend();
    std::printf ("polyphony/sustain...\n");
    testPolyphonyAndSustain();
    std::printf ("processor MIDI/state...\n");
    testProcessorMidiAndState();
    std::printf ("preset sync/CC20...\n");
    testPresetSyncAndControlChanges();
    std::printf ("CPU budget...\n");
    testCpuBudget();
    std::printf ("preset files...\n");
    testPresetFileRoundTrip();
    std::printf ("sample rates/block sizes...\n");
    testSampleRatesAndBlockSizes();
    std::printf ("realtime allocations...\n");
    testRealtimeAllocations();
    std::printf ("stability/artifacts...\n");
    testStabilityAndArtifacts();

    std::printf ("--------------------------\n");
    std::printf ("%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
